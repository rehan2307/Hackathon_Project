"""
Generates the Chaos Control master materials as real .uasset files.

Run headless (Scripts/CreateMaterials.ps1 does this):
    UnrealEditor-Cmd.exe ChaosControl.uproject -run=pythonscript -script=Scripts/create_materials.py

Every material exposes a vector parameter named "Color" so gameplay code can tint any
surface through ChaosUtil::SetMeshColor. All patterns are world-aligned, so they stay
correctly scaled on the non-uniformly scaled cubes the arenas are built from.
"""

import math
import unreal

MATERIAL_PATH = "/Game/Materials"
NOISE_TEXTURE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


class Graph:
    """Small helper that builds a material expression graph."""

    def __init__(self, name):
        full_path = f"{MATERIAL_PATH}/{name}"
        if eal.does_asset_exist(full_path):
            eal.delete_asset(full_path)
        self.path = full_path
        self.material = asset_tools.create_asset(name, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
        self._y = 0

    # -- node creation ---------------------------------------------------------------------
    def node(self, cls, **props):
        expr = mel.create_material_expression(self.material, cls, -1400, self._y)
        self._y += 70
        for key, value in props.items():
            expr.set_editor_property(key, value)
        return expr

    def link(self, src, dst, dst_input="", src_output=""):
        if not mel.connect_material_expressions(src, src_output, dst, dst_input):
            raise RuntimeError(f"{self.path}: failed to connect {src.get_name()}.{src_output} -> {dst.get_name()}.{dst_input}")

    def output(self, src, prop, src_output=""):
        if not mel.connect_material_property(src, src_output, prop):
            raise RuntimeError(f"{self.path}: failed to connect {src.get_name()} -> {prop}")

    # -- parameters and constants ----------------------------------------------------------
    def scalar(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def vector(self, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def const2(self, a, b):
        return self.node(unreal.MaterialExpressionConstant2Vector, r=a, g=b)

    # -- math ------------------------------------------------------------------------------
    def _binary(self, cls, a, b, in_a="A", in_b="B"):
        expr = self.node(cls)
        self.link(a, expr, in_a)
        self.link(b, expr, in_b)
        return expr

    def mul(self, a, b):
        return self._binary(unreal.MaterialExpressionMultiply, a, b)

    def add(self, a, b):
        return self._binary(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self._binary(unreal.MaterialExpressionSubtract, a, b)

    def div(self, a, b):
        return self._binary(unreal.MaterialExpressionDivide, a, b)

    def min(self, a, b):
        return self._binary(unreal.MaterialExpressionMin, a, b)

    def dot(self, a, b):
        return self._binary(unreal.MaterialExpressionDotProduct, a, b)

    def lerp(self, a, b, alpha):
        expr = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, expr, "A")
        self.link(b, expr, "B")
        self.link(alpha, expr, "Alpha")
        return expr

    def unary(self, cls, source, **props):
        expr = self.node(cls, **props)
        self.link(source, expr, "")
        return expr

    def frac(self, a):
        return self.unary(unreal.MaterialExpressionFrac, a)

    def floor(self, a):
        return self.unary(unreal.MaterialExpressionFloor, a)

    def abs(self, a):
        return self.unary(unreal.MaterialExpressionAbs, a)

    def round(self, a):
        return self.unary(unreal.MaterialExpressionRound, a)

    def one_minus(self, a):
        return self.unary(unreal.MaterialExpressionOneMinus, a)

    def saturate(self, a):
        return self.unary(unreal.MaterialExpressionSaturate, a)

    def sin(self, a):
        """sin(a) in radians (the Sine node's default period is 1)."""
        return self.unary(unreal.MaterialExpressionSine, a, period=2.0 * math.pi)

    def mask(self, a, r=False, g=False, b=False, alpha=False):
        return self.unary(unreal.MaterialExpressionComponentMask, a, r=r, g=g, b=b, a=alpha)

    # -- shared building blocks --------------------------------------------------------------
    def world_position(self):
        return self.node(unreal.MaterialExpressionWorldPosition)

    def time(self):
        return self.node(unreal.MaterialExpressionTime)

    def world_aligned_coords(self):
        """2D coordinates in the plane facing the surface normal (axis-aligned geometry)."""
        pos = self.world_position()
        normal = self.node(unreal.MaterialExpressionVertexNormalWS)
        along_x = self.round(self.abs(self.mask(normal, r=True)))
        along_z = self.round(self.abs(self.mask(normal, b=True)))
        plane_xy = self.mask(pos, r=True, g=True)
        plane_yz = self.mask(pos, g=True, b=True)
        plane_xz = self.mask(pos, r=True, b=True)
        side = self.lerp(plane_xz, plane_yz, along_x)
        return self.lerp(side, plane_xy, along_z)

    def pulse(self, speed_name, amount_name, default_speed, default_amount):
        """1 + Amount * sin(Time * Speed)."""
        speed = self.scalar(speed_name, default_speed)
        amount = self.scalar(amount_name, default_amount)
        wave = self.sin(self.mul(self.time(), speed))
        return self.add(self.const(1.0), self.mul(wave, amount))

    def finish(self):
        # Arena pieces are batched as instanced static meshes; without this usage flag the
        # engine silently renders them with the default material in game.
        mel.set_material_usage(self.material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        mel.recompile_material(self.material)
        eal.save_asset(self.path, only_if_is_dirty=False)
        unreal.log(f"CHAOS MATERIALS: saved {self.path}")


def build_surface():
    """Lit PBR surface: world-aligned panels with seams, per-panel tone variation and wear noise."""
    g = Graph("M_ChaosSurface")

    color = g.vector("Color", (0.05, 0.055, 0.065))
    metallic = g.scalar("Metallic", 0.5)
    roughness = g.scalar("Roughness", 0.45)
    panel_size = g.scalar("PanelSize", 200.0)
    seam_width = g.scalar("SeamWidth", 0.015)
    seam_darken = g.scalar("SeamDarken", 0.6)
    variation = g.scalar("Variation", 0.12)
    noise_scale = g.scalar("NoiseScale", 350.0)
    noise_amount = g.scalar("NoiseAmount", 0.25)
    emissive = g.vector("Emissive", (0.0, 0.0, 0.0))
    emissive_intensity = g.scalar("EmissiveIntensity", 0.0)
    seam_glow_color = g.vector("SeamGlowColor", (0.0, 0.6, 1.0))
    seam_glow = g.scalar("SeamGlow", 0.0)

    coords = g.world_aligned_coords()
    uv = g.div(coords, panel_size)

    # Distance to the nearest panel edge -> seam mask (1 on the seam).
    cell = g.frac(uv)
    edge = g.min(cell, g.one_minus(cell))
    edge_distance = g.min(g.mask(edge, r=True), g.mask(edge, g=True))
    seam = g.one_minus(g.saturate(g.div(edge_distance, seam_width)))

    # Stable per-panel random value.
    panel_id = g.floor(uv)
    hash_value = g.frac(g.mul(g.sin(g.dot(panel_id, g.const2(12.9898, 78.233))), g.const(43758.5453)))
    panel_variation = g.mul(g.sub(hash_value, g.const(0.5)), variation)

    # Large-scale wear from the engine's tiling noise texture.
    noise_texture = unreal.load_asset(NOISE_TEXTURE)
    noise_sample = g.node(unreal.MaterialExpressionTextureSample, texture=noise_texture,
                          sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    g.link(g.div(coords, noise_scale), noise_sample, "UVs")
    wear = g.mul(g.sub(g.mask(noise_sample, r=True), g.const(0.5)), noise_amount)

    tone = g.sub(g.add(g.add(g.const(1.0), panel_variation), wear), g.mul(seam, seam_darken))
    g.output(g.mul(color, tone), unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(metallic, unreal.MaterialProperty.MP_METALLIC)

    rough = g.add(g.add(roughness, g.mul(panel_variation, g.const(0.8))), g.add(wear, g.mul(seam, g.const(0.25))))
    g.output(g.saturate(rough), unreal.MaterialProperty.MP_ROUGHNESS)

    glow = g.add(g.mul(emissive, emissive_intensity), g.mul(g.mul(seam_glow_color, seam_glow), seam))
    g.output(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_emissive():
    """Unlit neon: solid glow with optional pulse and travelling bands."""
    g = Graph("M_ChaosEmissive")
    g.material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    color = g.vector("Color", (1.0, 1.0, 1.0))
    intensity = g.scalar("Intensity", 4.0)
    band_mix = g.scalar("BandMix", 0.0)
    band_scale = g.scalar("BandScale", 120.0)
    band_speed = g.scalar("BandSpeed", 2.0)
    pulse = g.pulse("PulseSpeed", "PulseAmount", 0.0, 0.0)

    pos = g.world_position()
    along = g.add(g.add(g.mask(pos, r=True), g.mask(pos, g=True)), g.mask(pos, b=True))
    phase = g.sub(g.mul(g.div(along, band_scale), g.const(2.0 * math.pi)), g.mul(g.time(), band_speed))
    band = g.add(g.mul(g.sin(phase), g.const(0.5)), g.const(0.5))
    band_factor = g.lerp(g.const(1.0), g.mul(band, band), band_mix)

    g.output(g.mul(g.mul(g.mul(color, intensity), pulse), band_factor), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_energy():
    """Additive translucent energy: shields, beams, holograms. Rim glow plus scanlines."""
    g = Graph("M_ChaosEnergy")
    g.material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g.material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    g.material.set_editor_property("two_sided", True)

    color = g.vector("Color", (0.0, 0.8, 1.0))
    intensity = g.scalar("Intensity", 3.0)
    fresnel_mix = g.scalar("FresnelMix", 0.5)
    fresnel_exponent = g.scalar("FresnelExponent", 2.5)
    line_mix = g.scalar("LineMix", 0.35)
    line_frequency = g.scalar("LineFrequency", 0.12)
    line_speed = g.scalar("LineSpeed", 4.0)
    pulse = g.pulse("PulseSpeed", "PulseAmount", 3.0, 0.2)

    fresnel = g.node(unreal.MaterialExpressionFresnel)
    g.link(fresnel_exponent, fresnel, "ExponentIn")
    fresnel_factor = g.lerp(g.const(1.0), fresnel, fresnel_mix)

    height = g.mask(g.world_position(), b=True)
    lines = g.add(g.mul(g.sin(g.sub(g.mul(height, line_frequency), g.mul(g.time(), line_speed))), g.const(0.5)), g.const(0.5))
    line_factor = g.lerp(g.const(1.0), lines, line_mix)

    glow = g.mul(g.mul(g.mul(g.mul(color, intensity), pulse), fresnel_factor), line_factor)
    g.output(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_hazard():
    """Lit diagonal warning stripes with a faint glow on the bright stripe."""
    g = Graph("M_ChaosHazard")

    color = g.vector("Color", (1.0, 0.5, 0.04))
    dark = g.vector("DarkColor", (0.015, 0.015, 0.018))
    stripe_width = g.scalar("StripeWidth", 30.0)
    glow = g.scalar("Glow", 0.4)
    roughness = g.scalar("Roughness", 0.55)

    pos = g.world_position()
    along = g.add(g.add(g.mask(pos, r=True), g.mask(pos, g=True)), g.mask(pos, b=True))
    stripe = g.round(g.frac(g.div(along, stripe_width)))

    g.output(g.lerp(dark, color, stripe), unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    g.output(g.mul(g.mul(color, glow), stripe), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def main():
    if not eal.does_directory_exist(MATERIAL_PATH):
        eal.make_directory(MATERIAL_PATH)
    build_surface()
    build_emissive()
    build_energy()
    build_hazard()
    unreal.log("CHAOS MATERIALS: complete")


main()
