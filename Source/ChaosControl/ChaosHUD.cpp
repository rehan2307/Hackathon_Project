#include "ChaosHUD.h"
#include "ChaosControl.h"
#include "ChaosAbilityComponent.h"
#include "ChaosArenaManager.h"
#include "ChaosExit.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "ChaosPlayerController.h"
#include "ChaosTrapBase.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace ChaosStyle
{
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Text(0.86f, 0.89f, 0.95f);
	const FLinearColor Muted(0.5f, 0.54f, 0.62f);
	const FLinearColor Panel(0.012f, 0.016f, 0.026f, 0.86f);
	const FLinearColor PanelLight(0.04f, 0.05f, 0.075f, 0.9f);
	const FLinearColor Ink(0.01f, 0.012f, 0.02f);
	const FLinearColor Gold(1.f, 0.78f, 0.15f);

	FLinearColor WithAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * Alpha);
	}

	FString AbilityShortName(EChaosSabotageAbility Ability)
	{
		switch (Ability)
		{
		case EChaosSabotageAbility::LaserBarrier:   return TEXT("LASER");
		case EChaosSabotageAbility::MovingPlatform: return TEXT("PLATFORM");
		case EChaosSabotageAbility::StunTrap:       return TEXT("STUN");
		}
		return FString();
	}
}

using ChaosStyle::WithAlpha;

AChaosHUD::AChaosHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFinder(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	UIFont = RobotoFinder.Object;
}

// ---------------------------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------------------------

void AChaosHUD::DrawHUD()
{
	Super::DrawHUD();

	GM = GetWorld() ? GetWorld()->GetAuthGameMode<AChaosGameMode>() : nullptr;
	PC = Cast<AChaosPlayerController>(GetOwningPlayerController());
	if (!Canvas || !GM || !PC)
	{
		return;
	}

	ScreenW = Canvas->ClipX;
	ScreenH = Canvas->ClipY;
	UIScale = ScreenH / 1080.f;

	if (GM->GetPhase() != LastPhase)
	{
		LastPhase = GM->GetPhase();
		PhaseChangeTime = RealNow();
	}

	switch (GM->GetPhase())
	{
	case EChaosMatchPhase::MainMenu:
		PC->IsShowingControls() ? DrawControlsScreen() : DrawMainMenu();
		break;
	case EChaosMatchPhase::ArenaSelect:
		DrawArenaSelect();
		break;
	case EChaosMatchPhase::RoundStarting:
		DrawGameplayHud();
		DrawRoundIntro();
		break;
	case EChaosMatchPhase::RoundActive:
		DrawGameplayHud();
		break;
	case EChaosMatchPhase::RoundComplete:
		DrawGameplayHud();
		DrawRoundResult();
		break;
	case EChaosMatchPhase::MatchComplete:
		DrawFinalResults();
		break;
	}

	if (GM->IsMenuPaused())
	{
		PC->IsShowingControls() ? DrawControlsScreen() : DrawPauseMenu();
	}

	DrawMessages();
}

void AChaosHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	UE_LOG(LogChaosControl, Log, TEXT("HUD hit box clicked: %s"), *BoxName.ToString());
	if (AChaosPlayerController* Controller = Cast<AChaosPlayerController>(GetOwningPlayerController()))
	{
		Controller->ActivateMenuItem(BoxName);
	}
}

void AChaosHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	Super::NotifyHitBoxBeginCursorOver(BoxName);
	if (AChaosPlayerController* Controller = Cast<AChaosPlayerController>(GetOwningPlayerController()))
	{
		Controller->SetMenuSelectionById(BoxName);
	}
}

float AChaosHUD::RealNow() const
{
	return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
}

float AChaosHUD::EaseIn(float StartTime, float Duration) const
{
	const float T = FMath::Clamp((RealNow() - StartTime) / FMath::Max(0.01f, Duration), 0.f, 1.f);
	return 1.f - FMath::Pow(1.f - T, 3.f);
}

FLinearColor AChaosHUD::ArenaAccent(EChaosArena Arena) const
{
	switch (Arena)
	{
	case EChaosArena::NeonGrid:   return FLinearColor(0.f, 0.72f, 1.f);
	case EChaosArena::ReactorRun: return FLinearColor(1.f, 0.45f, 0.05f);
	case EChaosArena::ChaosCore:  return FLinearColor(1.f, 0.1f, 0.6f);
	}
	return ChaosStyle::White;
}

// ---------------------------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------------------------

FSlateFontInfo AChaosHUD::MakeFont(float SizePx, EChaosFontWeight Weight, int32 Spacing) const
{
	static const FName LightFace(TEXT("Light"));
	static const FName RegularFace(TEXT("Regular"));
	static const FName BoldFace(TEXT("Bold"));
	const FName Face = Weight == EChaosFontWeight::Bold ? BoldFace : (Weight == EChaosFontWeight::Light ? LightFace : RegularFace);

	// Slate sizes are points (96 DPI): px = pt * 4/3.
	FSlateFontInfo Font(UIFont.Get(), FMath::Max(4.f, SizePx * UIScale * 0.75f), Face);
	Font.LetterSpacing = Spacing;
	return Font;
}

FVector2D AChaosHUD::MeasureLabel(const FString& Text, float SizePx, EChaosFontWeight Weight, int32 Spacing) const
{
	if (FSlateApplication::IsInitialized())
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return Measure->Measure(Text, MakeFont(SizePx, Weight, Spacing));
	}
	return FVector2D(Text.Len() * SizePx * UIScale * 0.55f, SizePx * UIScale * 1.2f);
}

float AChaosHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float SizePx, EChaosFontWeight Weight, int32 Align, int32 Spacing)
{
	if (Text.IsEmpty() || Color.A <= 0.f)
	{
		return 0.f;
	}

	const FVector2D Size = MeasureLabel(Text, SizePx, Weight, Spacing);
	float DrawX = X;
	if (Align == 0)
	{
		DrawX -= Size.X * 0.5f;
	}
	else if (Align > 0)
	{
		DrawX -= Size.X;
	}

	FCanvasTextItem Item(FVector2D(DrawX, Y), FText::FromString(Text), MakeFont(SizePx, Weight, Spacing), Color);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f * Color.A), FVector2D(FMath::Max(1.f, S(1.5f))));
	Canvas->DrawItem(Item);
	return Size.Y;
}

void AChaosHUD::FillPolygon(const TArray<FVector2D>& Points, const TArray<FLinearColor>& Colors)
{
	if (Points.Num() < 3 || Colors.Num() != Points.Num())
	{
		return;
	}

	TArray<FCanvasUVTri> Triangles;
	for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
	{
		FCanvasUVTri& Tri = Triangles.AddDefaulted_GetRef();
		Tri.V0_Pos = Points[0];
		Tri.V1_Pos = Points[Index];
		Tri.V2_Pos = Points[Index + 1];
		Tri.V0_Color = Colors[0];
		Tri.V1_Color = Colors[Index];
		Tri.V2_Color = Colors[Index + 1];
	}

	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AChaosHUD::FillRect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	FillGradient(X, Y, W, H, Color, Color);
}

void AChaosHUD::FillGradient(float X, float Y, float W, float H, const FLinearColor& Left, const FLinearColor& Right)
{
	FillPolygon({ FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H) }, { Left, Right, Right, Left });
}

void AChaosHUD::FillGradientV(float X, float Y, float W, float H, const FLinearColor& Top, const FLinearColor& Bottom)
{
	FillPolygon({ FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H) }, { Top, Top, Bottom, Bottom });
}

void AChaosHUD::FillChamfer(float X, float Y, float W, float H, float Cut, const FLinearColor& Left, const FLinearColor& Right)
{
	// Top-left and bottom-right corners cut: the shared sci-fi panel silhouette.
	const float C = FMath::Min(Cut, FMath::Min(W, H) * 0.5f);
	auto ColorAt = [&](float PX) { return FMath::Lerp(Left, Right, W > 0.f ? (PX - X) / W : 0.f); };
	const TArray<FVector2D> Points = {
		FVector2D(X + C, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H - C), FVector2D(X + W - C, Y + H), FVector2D(X, Y + H), FVector2D(X, Y + C)
	};
	TArray<FLinearColor> Colors;
	for (const FVector2D& Point : Points)
	{
		Colors.Add(ColorAt(Point.X));
	}
	FillPolygon(Points, Colors);
}

void AChaosHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Alpha, float Cut)
{
	const float C = S(Cut);
	// Soft outer glow, body with a faint accent wash, then an accent top edge.
	FillChamfer(X - S(2.f), Y - S(2.f), W + S(4.f), H + S(4.f), C + S(1.f), WithAlpha(Accent, 0.18f * Alpha), WithAlpha(Accent, 0.05f * Alpha));
	FillChamfer(X, Y, W, H, C, WithAlpha(ChaosStyle::Panel, Alpha), WithAlpha(FMath::Lerp(ChaosStyle::Panel, Accent * 0.25f, 0.25f), Alpha));
	FillPolygon({ FVector2D(X + C, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + S(3.f)), FVector2D(X + C - S(3.f), Y + S(3.f)) },
		{ WithAlpha(Accent, Alpha), WithAlpha(Accent, 0.2f * Alpha), WithAlpha(Accent, 0.2f * Alpha), WithAlpha(Accent, Alpha) });
}

void AChaosHUD::DrawDiamond(float CX, float CY, float Radius, const FLinearColor& Color)
{
	FillPolygon({ FVector2D(CX, CY - Radius), FVector2D(CX + Radius, CY), FVector2D(CX, CY + Radius), FVector2D(CX - Radius, CY) }, { Color, Color, Color, Color });
}

void AChaosHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color, int32 Segments)
{
	const float F = FMath::Clamp(Fraction, 0.f, 1.f);
	FillRect(X, Y, W, H, FLinearColor(0.08f, 0.09f, 0.13f, 0.9f));
	if (Segments <= 1)
	{
		FillGradient(X, Y, W * F, H, Color * 0.6f, Color);
		return;
	}

	const float Gap = S(3.f);
	const float SegmentW = (W - Gap * (Segments - 1)) / Segments;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float SegStart = static_cast<float>(Index) / Segments;
		const float Fill = FMath::Clamp((F - SegStart) * Segments, 0.f, 1.f);
		const float SX = X + Index * (SegmentW + Gap);
		if (Fill > 0.f)
		{
			FillRect(SX, Y, SegmentW * Fill, H, Color);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------------------------

void AChaosHUD::DrawChip(const FString& Text, float X, float Y, const FLinearColor& Fill, const FLinearColor& TextColor, float Size, int32 Align)
{
	const FVector2D TextSize = MeasureLabel(Text, Size, EChaosFontWeight::Bold, 120);
	const float W = TextSize.X + S(20.f);
	const float H = TextSize.Y + S(6.f);
	float Left = X;
	if (Align == 0)
	{
		Left -= W * 0.5f;
	}
	else if (Align > 0)
	{
		Left -= W;
	}
	FillChamfer(Left, Y, W, H, S(7.f), Fill, Fill);
	DrawLabel(Text, Left + S(10.f), Y + S(3.f), TextColor, Size, EChaosFontWeight::Bold, -1, 120);
}

float AChaosHUD::DrawKeyCap(const FString& Key, float X, float Y, float Height, const FLinearColor& Accent)
{
	const float Size = Height * 0.48f / UIScale;
	const FVector2D TextSize = MeasureLabel(Key, Size, EChaosFontWeight::Bold);
	const float W = FMath::Max(Height, TextSize.X + S(16.f));
	FillRect(X, Y + Height - S(3.f), W, S(3.f), WithAlpha(Accent, 0.8f));
	FillGradientV(X, Y, W, Height - S(3.f), FLinearColor(0.12f, 0.13f, 0.17f, 0.95f), FLinearColor(0.06f, 0.07f, 0.1f, 0.95f));
	DrawLabel(Key, X + W * 0.5f, Y + (Height - S(3.f) - TextSize.Y) * 0.5f, ChaosStyle::White, Size, EChaosFontWeight::Bold, 0);
	return W;
}

void AChaosHUD::DrawButton(const FString& Label, const FString& Detail, float X, float Y, float W, float H, bool bSelected, const FLinearColor& Accent)
{
	const float Slide = bSelected ? S(14.f) * EaseIn(PC->GetSelectionChangedRealTime(), 0.18f) : 0.f;
	const float BX = X + Slide;

	if (bSelected)
	{
		FillChamfer(BX - S(3.f), Y - S(3.f), W + S(6.f), H + S(6.f), S(15.f), WithAlpha(Accent, 0.35f), WithAlpha(Accent, 0.05f));
		FillChamfer(BX, Y, W, H, S(14.f), FLinearColor(Accent.R * 0.45f, Accent.G * 0.45f, Accent.B * 0.45f, 0.95f), FLinearColor(0.02f, 0.03f, 0.05f, 0.9f));
		FillRect(BX, Y + S(14.f), S(5.f), H - S(14.f), Accent);
	}
	else
	{
		FillChamfer(BX, Y, W, H, S(14.f), ChaosStyle::PanelLight, WithAlpha(ChaosStyle::Panel, 0.7f));
	}

	const float TextX = BX + S(28.f);
	const bool bDetail = !Detail.IsEmpty();
	const float LabelSize = 24.f;
	const float LabelY = Y + (bDetail ? S(10.f) : (H - MeasureLabel(Label, LabelSize, EChaosFontWeight::Bold, 80).Y) * 0.5f);
	DrawLabel(Label.ToUpper(), TextX, LabelY, bSelected ? ChaosStyle::White : ChaosStyle::Text, LabelSize, EChaosFontWeight::Bold, -1, 80);
	if (bDetail)
	{
		DrawLabel(Detail, TextX, Y + S(44.f), bSelected ? ChaosStyle::Text : ChaosStyle::Muted, 15.f, EChaosFontWeight::Regular);
	}
	if (bSelected)
	{
		DrawLabel(TEXT(">"), BX + W - S(30.f), Y + (H - S(30.f)) * 0.5f, Accent, 26.f, EChaosFontWeight::Bold);
	}
}

void AChaosHUD::DrawMenuList(float X, float Y, float Width, bool bCentered)
{
	const TArray<FChaosMenuItem> Items = PC->GetMenuItems();
	const int32 Selected = PC->GetMenuSelection();
	const float W = S(Width);
	const float Left = bCentered ? X - W * 0.5f : X;
	const float Appear = EaseIn(PC->GetMenuOpenedRealTime(), 0.35f);

	float RowY = Y;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FChaosMenuItem& Item = Items[Index];
		const float H = S(Item.Detail.IsEmpty() ? 60.f : 76.f);
		// Staggered slide-in when the menu opens.
		const float Stagger = FMath::Clamp(Appear * 1.6f - Index * 0.15f, 0.f, 1.f);
		const float OffsetX = (1.f - Stagger) * S(-40.f);
		DrawButton(Item.Label, Item.Detail, Left + OffsetX, RowY, W, H, Index == Selected, ChaosUtil::RunnerColor);
		AddHitBox(FVector2D(Left, RowY), FVector2D(W, H), Item.Id, true);
		RowY += H + S(12.f);
	}
}

// ---------------------------------------------------------------------------------------------
// Menu screens
// ---------------------------------------------------------------------------------------------

void AChaosHUD::DrawMainMenu()
{
	// Dark wash on the left for the menu, live arena preview on the right.
	FillGradient(0.f, 0.f, ScreenW * 0.62f, ScreenH, FLinearColor(0.f, 0.005f, 0.015f, 0.94f), FLinearColor(0.f, 0.005f, 0.015f, 0.f));
	FillGradientV(0.f, ScreenH * 0.7f, ScreenW, ScreenH * 0.3f, FLinearColor(0.f, 0.f, 0.f, 0.f), FLinearColor(0.f, 0.f, 0.01f, 0.7f));

	const float Appear = EaseIn(PhaseChangeTime, 0.6f);
	const float X = S(130.f) - (1.f - Appear) * S(60.f);
	const float TitleY = S(150.f);

	FillGradientV(X - S(28.f), TitleY + S(10.f), S(6.f), S(300.f), ChaosUtil::RunnerColor, ChaosUtil::SaboteurColor);
	DrawLabel(TEXT("PS02  //  SABOTAGE THE SYSTEM"), X, TitleY, WithAlpha(ChaosUtil::SaboteurColor, Appear), 18.f, EChaosFontWeight::Bold, -1, 400);
	DrawLabel(TEXT("CHAOS"), X - S(6.f), TitleY + S(26.f), WithAlpha(ChaosStyle::White, Appear), 128.f, EChaosFontWeight::Bold, -1, 40);
	DrawLabel(TEXT("CONTROL"), X - S(6.f), TitleY + S(150.f), WithAlpha(ChaosUtil::RunnerColor, Appear), 128.f, EChaosFontWeight::Bold, -1, 40);
	DrawLabel(TEXT("One player escapes. One rewires the arena. Then you swap."), X, TitleY + S(300.f), WithAlpha(ChaosStyle::Text, Appear), 21.f, EChaosFontWeight::Light);

	DrawMenuList(X, S(540.f), 460.f, false);

	// Selected-arena card, bottom right.
	const EChaosArena Arena = GM->GetSelectedArena();
	const FLinearColor Accent = ArenaAccent(Arena);
	const float CardW = S(560.f);
	const float CardH = S(150.f);
	const float CardX = ScreenW - CardW - S(70.f);
	const float CardY = ScreenH - CardH - S(70.f);
	DrawPanel(CardX, CardY, CardW, CardH, Accent);
	DrawLabel(TEXT("SELECTED ARENA"), CardX + S(28.f), CardY + S(20.f), ChaosStyle::Muted, 14.f, EChaosFontWeight::Bold, -1, 300);
	DrawLabel(ChaosUtil::GetArenaName(Arena), CardX + S(28.f), CardY + S(42.f), Accent, 40.f, EChaosFontWeight::Bold, -1, 60);
	DrawLabel(ChaosUtil::GetArenaDescription(Arena), CardX + S(28.f), CardY + S(98.f), ChaosStyle::Text, 16.f);

	DrawLabel(TEXT("MOUSE  OR  UP / DOWN + ENTER"), X, ScreenH - S(70.f), ChaosStyle::Muted, 14.f, EChaosFontWeight::Bold, -1, 300);
}

void AChaosHUD::DrawArenaSelect()
{
	FillGradientV(0.f, 0.f, ScreenW, ScreenH * 0.3f, FLinearColor(0.f, 0.f, 0.01f, 0.85f), FLinearColor(0.f, 0.f, 0.01f, 0.f));
	FillGradientV(0.f, ScreenH * 0.5f, ScreenW, ScreenH * 0.5f, FLinearColor(0.f, 0.f, 0.01f, 0.f), FLinearColor(0.f, 0.f, 0.01f, 0.92f));

	DrawLabel(TEXT("CHOOSE YOUR BATTLEGROUND"), S(90.f), S(60.f), ChaosUtil::SaboteurColor, 18.f, EChaosFontWeight::Bold, -1, 400);
	DrawLabel(TEXT("SELECT ARENA"), S(84.f), S(86.f), ChaosStyle::White, 64.f, EChaosFontWeight::Bold, -1, 40);
	DrawLabel(TEXT("Highlight to preview live  -  Enter or click to play"), S(90.f), S(170.f), ChaosStyle::Text, 18.f, EChaosFontWeight::Light);

	const TArray<FChaosMenuItem> Items = PC->GetMenuItems();
	const int32 Selected = PC->GetMenuSelection();
	const float CardW = S(500.f);
	const float CardH = S(220.f);
	const float Gap = S(36.f);
	const float TotalW = CardW * ChaosUtil::NumArenas + Gap * (ChaosUtil::NumArenas - 1);
	const float StartX = (ScreenW - TotalW) * 0.5f;
	const float BaseY = ScreenH - CardH - S(140.f);

	static const TCHAR* Features[ChaosUtil::NumArenas] = {
		TEXT("2 LASERS  /  1 PLATFORM  /  2 STUN PADS"),
		TEXT("2 LASERS  /  2 PLATFORMS  /  3 STUN PADS"),
		TEXT("3 LASERS  /  1 PLATFORM  /  3 STUN PADS"),
	};

	for (int32 Index = 0; Index < ChaosUtil::NumArenas && Index < Items.Num(); ++Index)
	{
		const EChaosArena Arena = static_cast<EChaosArena>(Index);
		const FLinearColor Accent = ArenaAccent(Arena);
		const bool bSelected = Index == Selected;
		const float Lift = bSelected ? S(22.f) * EaseIn(PC->GetSelectionChangedRealTime(), 0.2f) : 0.f;
		const float X = StartX + Index * (CardW + Gap);
		const float Y = BaseY - Lift;

		if (bSelected)
		{
			FillChamfer(X - S(4.f), Y - S(4.f), CardW + S(8.f), CardH + S(8.f), S(22.f), WithAlpha(Accent, 0.55f), WithAlpha(Accent, 0.15f));
		}
		DrawPanel(X, Y, CardW, CardH, Accent, bSelected ? 1.f : 0.8f, 20.f);
		FillGradient(X, Y + S(3.f), CardW, S(60.f), WithAlpha(Accent, bSelected ? 0.35f : 0.12f), WithAlpha(Accent, 0.f));

		DrawLabel(FString::Printf(TEXT("0%d"), Index + 1), X + S(28.f), Y + S(18.f), WithAlpha(Accent, 0.9f), 20.f, EChaosFontWeight::Bold, -1, 200);
		DrawLabel(ChaosUtil::GetArenaName(Arena), X + S(28.f), Y + S(48.f), bSelected ? ChaosStyle::White : ChaosStyle::Text, 40.f, EChaosFontWeight::Bold, -1, 60);
		DrawLabel(ChaosUtil::GetArenaDescription(Arena), X + S(28.f), Y + S(108.f), ChaosStyle::Text, 15.f);
		DrawLabel(Features[Index], X + S(28.f), Y + S(150.f), Accent, 13.f, EChaosFontWeight::Bold, -1, 150);
		if (bSelected)
		{
			DrawLabel(TEXT("ENTER TO DEPLOY"), X + CardW - S(28.f), Y + CardH - S(36.f), ChaosStyle::White, 14.f, EChaosFontWeight::Bold, 1, 250);
		}
		AddHitBox(FVector2D(X, BaseY - S(22.f)), FVector2D(CardW, CardH + S(22.f)), Items[Index].Id, true);
	}

	// Back button.
	if (Items.IsValidIndex(ChaosUtil::NumArenas))
	{
		const float BW = S(220.f);
		const float BH = S(54.f);
		const float BX = S(90.f);
		const float BY = ScreenH - BH - S(50.f);
		DrawButton(Items[ChaosUtil::NumArenas].Label, FString(), BX, BY, BW, BH, Selected == ChaosUtil::NumArenas, ChaosUtil::SaboteurColor);
		AddHitBox(FVector2D(BX, BY), FVector2D(BW, BH), Items[ChaosUtil::NumArenas].Id, true);
	}
}

void AChaosHUD::DrawControlsScreen()
{
	FillRect(0.f, 0.f, ScreenW, ScreenH, FLinearColor(0.f, 0.f, 0.01f, 0.8f));

	const float W = S(1240.f);
	const float H = S(700.f);
	const float X = (ScreenW - W) * 0.5f;
	const float Y = (ScreenH - H) * 0.5f;
	DrawPanel(X, Y, W, H, ChaosStyle::White, 1.f, 26.f);
	DrawLabel(TEXT("FIELD MANUAL"), X + S(50.f), Y + S(36.f), ChaosStyle::Muted, 15.f, EChaosFontWeight::Bold, -1, 400);
	DrawLabel(TEXT("CONTROLS"), X + S(46.f), Y + S(58.f), ChaosStyle::White, 52.f, EChaosFontWeight::Bold, -1, 40);

	const float ColumnW = (W - S(100.f)) * 0.5f;
	for (int32 PlayerIndex = 0; PlayerIndex < ChaosUtil::NumPlayers; ++PlayerIndex)
	{
		const FChaosKeyBindings& Keys = PC->GetKeysForPlayer(PlayerIndex);
		const FLinearColor Accent = PlayerIndex == 0 ? ChaosUtil::RunnerColor : ChaosUtil::SaboteurColor;
		const float ColX = X + S(50.f) + ColumnW * PlayerIndex + S(PlayerIndex * 20.f);
		float RowY = Y + S(150.f);

		DrawChip(FString::Printf(TEXT("PLAYER %d"), PlayerIndex + 1), ColX, RowY, Accent, ChaosStyle::Ink, 18.f);
		RowY += S(56.f);

		struct FControlRow
		{
			const TCHAR* Action;
			TArray<FString> KeyNames;
		};
		const FControlRow Rows[] = {
			{ TEXT("MOVE"), { AChaosPlayerController::KeyName(Keys.Up), AChaosPlayerController::KeyName(Keys.Left), AChaosPlayerController::KeyName(Keys.Down), AChaosPlayerController::KeyName(Keys.Right) } },
			{ TEXT("JUMP"), { AChaosPlayerController::KeyName(Keys.Jump) } },
			{ TEXT("INTERACT / CYCLE TRAP"), { AChaosPlayerController::KeyName(Keys.Interact) } },
			{ TEXT("TRIGGER SABOTAGE"), { AChaosPlayerController::KeyName(Keys.Sabotage) } },
		};
		for (const FControlRow& Row : Rows)
		{
			DrawLabel(Row.Action, ColX, RowY + S(10.f), ChaosStyle::Text, 16.f, EChaosFontWeight::Bold, -1, 150);
			float KeyX = ColX + S(270.f);
			for (const FString& KeyName : Row.KeyNames)
			{
				KeyX += DrawKeyCap(KeyName.ToUpper(), KeyX, RowY, S(40.f), Accent) + S(8.f);
			}
			RowY += S(54.f);
		}
	}

	float InfoY = Y + S(470.f);
	FillRect(X + S(50.f), InfoY - S(18.f), W - S(100.f), S(1.f), FLinearColor(1.f, 1.f, 1.f, 0.12f));
	DrawChip(TEXT("RUNNER"), X + S(50.f), InfoY, ChaosUtil::RunnerColor, ChaosStyle::Ink, 14.f);
	DrawLabel(TEXT("Touch all 3 energy cores (or press Interact near one), then enter the green EXIT."), X + S(190.f), InfoY + S(2.f), ChaosStyle::Text, 17.f);
	InfoY += S(44.f);
	DrawChip(TEXT("SABOTEUR"), X + S(50.f), InfoY, ChaosUtil::SaboteurColor, ChaosStyle::Ink, 14.f);
	DrawLabel(TEXT("Interact cycles Laser / Platform / Stun. Sabotage triggers the idle one nearest you."), X + S(190.f), InfoY + S(2.f), ChaosStyle::Text, 17.f);
	InfoY += S(44.f);
	DrawLabel(TEXT("Limited energy, no regeneration, per-ability cooldowns. Roles swap every round.  P / Esc pauses."), X + S(50.f), InfoY + S(2.f), ChaosStyle::Muted, 16.f);

	DrawMenuList(X + W - S(330.f), Y + H - S(96.f), 280.f, false);
}

void AChaosHUD::DrawPauseMenu()
{
	FillRect(0.f, 0.f, ScreenW, ScreenH, FLinearColor(0.f, 0.f, 0.01f, 0.65f));
	const float CX = ScreenW * 0.5f;
	DrawLabel(TEXT("MATCH SUSPENDED"), CX, S(250.f), ChaosStyle::Muted, 16.f, EChaosFontWeight::Bold, 0, 400);
	DrawLabel(TEXT("PAUSED"), CX, S(276.f), ChaosStyle::White, 72.f, EChaosFontWeight::Bold, 0, 60);
	DrawMenuList(CX, S(400.f), 460.f, true);
}

// ---------------------------------------------------------------------------------------------
// In-game HUD
// ---------------------------------------------------------------------------------------------

void AChaosHUD::DrawGameplayHud()
{
	DrawWorldMarkers();
	DrawTimerModule();
	DrawPlayerCard(0, false);
	DrawPlayerCard(1, true);
	DrawKeyHints();
}

void AChaosHUD::DrawTimerModule()
{
	const float W = S(300.f);
	const float H = S(104.f);
	const float X = (ScreenW - W) * 0.5f;
	const float Y = S(16.f);
	const float TimeLeft = GM->GetRoundTimeRemaining();
	const bool bUrgent = TimeLeft <= 15.f && GM->GetPhase() == EChaosMatchPhase::RoundActive;
	const float Pulse = bUrgent ? 0.5f + 0.5f * FMath::Sin(RealNow() * 8.f) : 0.f;
	const FLinearColor Accent = bUrgent ? ChaosUtil::DangerColor : ArenaAccent(GM->GetSelectedArena());

	DrawPanel(X, Y, W, H, Accent);
	DrawLabel(FString::Printf(TEXT("ROUND %d / %d  -  %s"), GM->GetRoundNumber(), GM->GetRoundsPerMatch(), *ChaosUtil::GetArenaName(GM->GetSelectedArena())),
		X + W * 0.5f, Y + S(12.f), ChaosStyle::Muted, 12.f, EChaosFontWeight::Bold, 0, 250);
	DrawLabel(ChaosUtil::FormatTime(TimeLeft), X + W * 0.5f, Y + S(28.f), FMath::Lerp(ChaosStyle::White, ChaosUtil::DangerColor, bUrgent ? 0.6f + 0.4f * Pulse : 0.f), 50.f, EChaosFontWeight::Bold, 0, 40);
	const float Fraction = GM->RoundDuration > 0.f ? TimeLeft / GM->RoundDuration : 0.f;
	DrawBar(X + S(24.f), Y + H - S(14.f), W - S(48.f), S(4.f), Fraction, Accent);
}

void AChaosHUD::DrawPlayerCard(int32 PlayerIndex, bool bRightSide)
{
	AChaosPlayerCharacter* Avatar = GM->GetPlayerCharacter(PlayerIndex);
	if (!Avatar)
	{
		return;
	}

	const bool bRunner = Avatar->IsRunner();
	const FLinearColor RoleColor = Avatar->GetRoleColor();
	const float W = S(400.f);
	const float H = S(bRunner ? 128.f : 182.f);
	const float X = bRightSide ? ScreenW - W - S(20.f) : S(20.f);
	const float Y = S(16.f);
	DrawPanel(X, Y, W, H, RoleColor);

	// Header: role chip, player name, score with a pop when it changes.
	const float Left = X + S(22.f);
	const float Right = X + W - S(22.f);
	DrawChip(ChaosUtil::GetRoleName(Avatar->GetRole()), Left, Y + S(16.f), RoleColor, ChaosStyle::Ink, 13.f);
	DrawLabel(FString::Printf(TEXT("PLAYER %d"), PlayerIndex + 1), Left, Y + S(46.f), ChaosStyle::White, 22.f, EChaosFontWeight::Bold, -1, 60);

	const int32 Score = GM->GetScore(PlayerIndex);
	if (Score != ShownScores[PlayerIndex])
	{
		ShownScores[PlayerIndex] = Score;
		ScoreChangeTime[PlayerIndex] = RealNow();
	}
	const float Pop = 1.f - EaseIn(ScoreChangeTime[PlayerIndex], 0.6f);
	DrawLabel(TEXT("SCORE"), Right, Y + S(16.f), ChaosStyle::Muted, 12.f, EChaosFontWeight::Bold, 1, 300);
	DrawLabel(FString::FromInt(Score), Right, Y + S(32.f) - S(6.f) * Pop, FMath::Lerp(ChaosStyle::White, ChaosStyle::Gold, Pop), 34.f + 10.f * Pop, EChaosFontWeight::Bold, 1);

	const float BodyY = Y + S(84.f);
	if (bRunner)
	{
		const int32 Required = GM->GetCoresRequired();
		const int32 Collected = GM->GetCoresCollected();
		DrawLabel(TEXT("CORES"), Left, BodyY + S(4.f), ChaosStyle::Muted, 12.f, EChaosFontWeight::Bold, -1, 300);
		for (int32 Pip = 0; Pip < Required; ++Pip)
		{
			const float CX = Left + S(80.f) + Pip * S(30.f);
			DrawDiamond(CX, BodyY + S(13.f), S(11.f), Pip < Collected ? ChaosStyle::Gold : FLinearColor(0.2f, 0.2f, 0.25f, 0.9f));
			if (Pip < Collected)
			{
				DrawDiamond(CX, BodyY + S(13.f), S(5.f), ChaosStyle::White);
			}
		}

		const bool bOpen = GM->IsExitUnlocked();
		if (Avatar->IsStunned())
		{
			DrawChip(FString::Printf(TEXT("STUNNED %.1f"), Avatar->GetStunTimeRemaining()), Right, BodyY, ChaosUtil::StunColor, ChaosStyle::Ink, 13.f, 1);
		}
		else
		{
			DrawChip(bOpen ? TEXT("EXIT OPEN") : TEXT("EXIT LOCKED"), Right, BodyY, bOpen ? ChaosUtil::SuccessColor : FLinearColor(0.35f, 0.06f, 0.06f, 0.95f),
				bOpen ? ChaosStyle::Ink : ChaosStyle::Text, 13.f, 1);
		}
		return;
	}

	// Saboteur: segmented energy bar and three ability tiles.
	const UChaosAbilityComponent* Abilities = Avatar->GetAbilityComponent();
	const float Energy = Abilities->GetEnergy();
	DrawLabel(TEXT("ENERGY"), Left, BodyY - S(2.f), ChaosStyle::Muted, 12.f, EChaosFontWeight::Bold, -1, 300);
	DrawLabel(FString::Printf(TEXT("%d"), FMath::RoundToInt(Energy)), Right, BodyY - S(6.f), ChaosUtil::SaboteurColor, 18.f, EChaosFontWeight::Bold, 1);
	DrawBar(Left + S(82.f), BodyY + S(2.f), W - S(150.f), S(10.f), Energy / FMath::Max(1.f, Abilities->MaxEnergy), ChaosUtil::SaboteurColor, 10);

	const float TileY = BodyY + S(26.f);
	const float TileGap = S(8.f);
	const float TileW = (W - S(44.f) - TileGap * 2.f) / 3.f;
	const float TileH = S(58.f);
	for (int32 AbilityIndex = 0; AbilityIndex < ChaosUtil::NumAbilities; ++AbilityIndex)
	{
		const EChaosSabotageAbility Ability = static_cast<EChaosSabotageAbility>(AbilityIndex);
		const FChaosAbilityTuning& Tuning = Abilities->GetTuning(Ability);
		const bool bSelected = Abilities->GetSelectedAbility() == Ability;
		const float Cooldown = Abilities->GetCooldownRemaining(Ability);
		const bool bAffordable = Abilities->HasEnergyFor(Ability);
		const float TX = Left + AbilityIndex * (TileW + TileGap);

		FillChamfer(TX, TileY, TileW, TileH, S(9.f), bSelected ? FLinearColor(0.35f, 0.04f, 0.22f, 0.95f) : FLinearColor(0.05f, 0.06f, 0.09f, 0.95f), FLinearColor(0.03f, 0.03f, 0.05f, 0.95f));
		if (Cooldown > 0.f && Tuning.Cooldown > 0.f)
		{
			// Cooldown shade drains downward as the ability recharges.
			const float Shade = TileH * Cooldown / Tuning.Cooldown;
			FillRect(TX, TileY + TileH - Shade, TileW, Shade, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		}
		if (bSelected)
		{
			FillRect(TX, TileY, TileW, S(3.f), ChaosUtil::SaboteurColor);
			DrawDiamond(TX + TileW * 0.5f, TileY + TileH + S(6.f), S(5.f), ChaosUtil::SaboteurColor);
		}

		FString Status = TEXT("READY");
		FLinearColor StatusColor = ChaosUtil::SuccessColor;
		if (Cooldown > 0.f)
		{
			Status = FString::Printf(TEXT("%.1fs"), Cooldown);
			StatusColor = ChaosUtil::WarningColor;
		}
		else if (!bAffordable)
		{
			Status = TEXT("NO ENERGY");
			StatusColor = ChaosUtil::DangerColor;
		}
		DrawLabel(ChaosStyle::AbilityShortName(Ability), TX + S(10.f), TileY + S(8.f), bSelected ? ChaosStyle::White : ChaosStyle::Text, 13.f, EChaosFontWeight::Bold, -1, 150);
		DrawLabel(FString::FromInt(FMath::RoundToInt(Tuning.EnergyCost)), TX + TileW - S(10.f), TileY + S(8.f), bAffordable ? ChaosUtil::SaboteurColor : ChaosUtil::DangerColor, 13.f, EChaosFontWeight::Bold, 1);
		DrawLabel(Status, TX + S(10.f), TileY + S(32.f), StatusColor, 14.f, EChaosFontWeight::Bold, -1, 100);
	}
}

void AChaosHUD::DrawKeyHints()
{
	for (int32 PlayerIndex = 0; PlayerIndex < ChaosUtil::NumPlayers; ++PlayerIndex)
	{
		const AChaosPlayerCharacter* Avatar = GM->GetPlayerCharacter(PlayerIndex);
		if (!Avatar)
		{
			continue;
		}
		const FChaosKeyBindings& Keys = PC->GetKeysForPlayer(PlayerIndex);
		const FLinearColor Accent = Avatar->GetRoleColor();
		const bool bRunner = Avatar->IsRunner();

		struct FHint
		{
			FString Key;
			const TCHAR* Action;
		};
		const FString MoveKeys = PC->DescribeMovementKeys(PlayerIndex) == TEXT("Arrow keys") ? FString(TEXT("ARROWS")) : FString(TEXT("WASD"));
		const FHint Hints[] = {
			{ MoveKeys, TEXT("MOVE") },
			{ AChaosPlayerController::KeyName(Keys.Jump).ToUpper(), TEXT("JUMP") },
			{ AChaosPlayerController::KeyName(Keys.Interact).ToUpper(), bRunner ? TEXT("INTERACT") : TEXT("CYCLE") },
			{ AChaosPlayerController::KeyName(Keys.Sabotage).ToUpper(), TEXT("SABOTAGE") },
		};

		const float KeyH = S(30.f);
		const float Y = ScreenH - KeyH - S(18.f);
		const int32 HintCount = bRunner ? 3 : 4;

		// Measure the row so the right-hand player's hints can be right-aligned.
		float RowW = 0.f;
		for (int32 Index = 0; Index < HintCount; ++Index)
		{
			const FVector2D KeySize = MeasureLabel(Hints[Index].Key, KeyH * 0.48f / UIScale, EChaosFontWeight::Bold);
			RowW += FMath::Max(KeyH, KeySize.X + S(16.f)) + S(8.f) + MeasureLabel(Hints[Index].Action, 12.f, EChaosFontWeight::Bold, 200).X + S(22.f);
		}
		float X = PlayerIndex == 0 ? S(22.f) : ScreenW - RowW - S(22.f);
		FillGradient(PlayerIndex == 0 ? 0.f : ScreenW - RowW - S(60.f), Y - S(10.f), RowW + S(60.f), KeyH + S(28.f),
			FLinearColor(0.f, 0.f, 0.01f, PlayerIndex == 0 ? 0.7f : 0.f), FLinearColor(0.f, 0.f, 0.01f, PlayerIndex == 0 ? 0.f : 0.7f));
		for (int32 Index = 0; Index < HintCount; ++Index)
		{
			X += DrawKeyCap(Hints[Index].Key, X, Y, KeyH, Accent) + S(8.f);
			DrawLabel(Hints[Index].Action, X, Y + S(8.f), ChaosStyle::Text, 12.f, EChaosFontWeight::Bold, -1, 200);
			X += MeasureLabel(Hints[Index].Action, 12.f, EChaosFontWeight::Bold, 200).X + S(22.f);
		}
	}
}

void AChaosHUD::DrawWorldMarkers()
{
	AChaosArenaManager* Arena = GM->GetArenaManager();
	if (!Arena)
	{
		return;
	}

	// Player tags.
	for (int32 PlayerIndex = 0; PlayerIndex < ChaosUtil::NumPlayers; ++PlayerIndex)
	{
		AChaosPlayerCharacter* Avatar = GM->GetPlayerCharacter(PlayerIndex);
		if (!Avatar)
		{
			continue;
		}
		const FVector ScreenPos = Project(Avatar->GetActorLocation() + FVector(0.f, 0.f, 150.f));
		DrawChip(FString::Printf(TEXT("P%d"), PlayerIndex + 1), ScreenPos.X, ScreenPos.Y - S(22.f), Avatar->GetRoleColor(), ChaosStyle::Ink, 12.f, 0);
		if (Avatar->IsStunned())
		{
			DrawChip(TEXT("STUNNED"), ScreenPos.X, ScreenPos.Y - S(48.f), ChaosUtil::StunColor, ChaosStyle::Ink, 11.f, 0);
		}
	}

	// Active mechanisms with their remaining time.
	for (AChaosTrapBase* Trap : Arena->GetTraps())
	{
		if (IsValid(Trap) && Trap->IsTrapActive())
		{
			const FVector ScreenPos = Project(Trap->GetIndicatorLocation());
			DrawChip(FString::Printf(TEXT("%s  %.1f"), *ChaosStyle::AbilityShortName(Trap->GetAbilityType()), Trap->GetActiveTimeRemaining()),
				ScreenPos.X, ScreenPos.Y, FLinearColor(0.45f, 0.03f, 0.05f, 0.9f), ChaosStyle::White, 11.f, 0);
		}
	}

	// The mechanism the Saboteur would trigger right now.
	if (GM->GetPhase() == EChaosMatchPhase::RoundActive)
	{
		if (AChaosPlayerCharacter* Saboteur = GM->GetPlayerCharacter(GM->GetSaboteurIndex()))
		{
			const EChaosSabotageAbility Selected = Saboteur->GetAbilityComponent()->GetSelectedAbility();
			if (AChaosTrapBase* Target = Arena->FindTargetTrap(Selected, Saboteur->GetActorLocation()))
			{
				const FVector ScreenPos = Project(Target->GetIndicatorLocation());
				const float Bounce = S(4.f) * FMath::Sin(RealNow() * 6.f);
				const float R = S(16.f);
				const FLinearColor Mag = ChaosUtil::SaboteurColor;
				FillPolygon({ FVector2D(ScreenPos.X - R, ScreenPos.Y - R * 2.4f + Bounce), FVector2D(ScreenPos.X + R, ScreenPos.Y - R * 2.4f + Bounce), FVector2D(ScreenPos.X, ScreenPos.Y - R * 1.3f + Bounce) },
					{ Mag, Mag, Mag });
				DrawLabel(TEXT("TARGET"), ScreenPos.X, ScreenPos.Y - R * 4.f + Bounce, Mag, 11.f, EChaosFontWeight::Bold, 0, 300);
			}
		}
	}

	// Exit status.
	if (AChaosExit* ExitGate = Arena->GetExit())
	{
		const FVector ScreenPos = Project(ExitGate->GetActorLocation() + FVector(0.f, 0.f, 330.f));
		const bool bOpen = ExitGate->IsUnlocked();
		DrawChip(bOpen ? FString(TEXT("EXIT OPEN")) : FString::Printf(TEXT("EXIT  %d/%d"), GM->GetCoresCollected(), GM->GetCoresRequired()),
			ScreenPos.X, ScreenPos.Y, bOpen ? ChaosUtil::SuccessColor : FLinearColor(0.4f, 0.05f, 0.05f, 0.9f), bOpen ? ChaosStyle::Ink : ChaosStyle::White, 12.f, 0);
	}
}

// ---------------------------------------------------------------------------------------------
// Round flow overlays
// ---------------------------------------------------------------------------------------------

void AChaosHUD::DrawRoundIntro()
{
	FillRect(0.f, 0.f, ScreenW, ScreenH, FLinearColor(0.f, 0.f, 0.01f, 0.45f));
	const float CX = ScreenW * 0.5f;
	const float CY = ScreenH * 0.5f;
	const float Appear = EaseIn(PhaseChangeTime, 0.5f);
	const int32 RunnerIndex = GM->GetRunnerIndex();

	DrawLabel(ChaosUtil::GetArenaName(GM->GetSelectedArena()), CX, CY - S(290.f), WithAlpha(ArenaAccent(GM->GetSelectedArena()), Appear), 18.f, EChaosFontWeight::Bold, 0, 500);
	DrawLabel(FString::Printf(TEXT("ROUND %d"), GM->GetRoundNumber()), CX, CY - S(262.f), WithAlpha(ChaosStyle::White, Appear), 88.f, EChaosFontWeight::Bold, 0, 60);

	// Two slanted role banners slide in from opposite sides.
	const float BannerW = S(920.f);
	const float BannerH = S(86.f);
	const float Slant = S(40.f);
	const float LeftX = CX - BannerW + S(30.f) - (1.f - Appear) * ScreenW * 0.6f;
	const float RightX = CX - S(30.f) + (1.f - Appear) * ScreenW * 0.6f;
	const float TopY = CY - S(120.f);
	const float BottomY = CY - S(20.f);
	const FLinearColor Cyan = ChaosUtil::RunnerColor;
	const FLinearColor Mag = ChaosUtil::SaboteurColor;
	FillPolygon({ FVector2D(LeftX, TopY), FVector2D(LeftX + BannerW + Slant, TopY), FVector2D(LeftX + BannerW, TopY + BannerH), FVector2D(LeftX - Slant, TopY + BannerH) },
		{ WithAlpha(Cyan, 0.f), WithAlpha(Cyan, 0.9f), WithAlpha(Cyan, 0.9f), WithAlpha(Cyan, 0.f) });
	FillPolygon({ FVector2D(RightX + Slant, BottomY), FVector2D(RightX + BannerW + Slant, BottomY), FVector2D(RightX + BannerW, BottomY + BannerH), FVector2D(RightX, BottomY + BannerH) },
		{ WithAlpha(Mag, 0.9f), WithAlpha(Mag, 0.f), WithAlpha(Mag, 0.f), WithAlpha(Mag, 0.9f) });
	DrawLabel(TEXT("RUNNER"), LeftX + BannerW - S(30.f), TopY + S(10.f), ChaosStyle::Ink, 40.f, EChaosFontWeight::Bold, 1, 100);
	DrawLabel(FString::Printf(TEXT("PLAYER %d  -  COLLECT THE CORES, REACH THE EXIT"), RunnerIndex + 1), LeftX + BannerW - S(30.f), TopY + S(56.f), ChaosStyle::Ink, 13.f, EChaosFontWeight::Bold, 1, 150);
	DrawLabel(TEXT("SABOTEUR"), RightX + S(50.f), BottomY + S(10.f), ChaosStyle::Ink, 40.f, EChaosFontWeight::Bold, -1, 100);
	DrawLabel(FString::Printf(TEXT("PLAYER %d  -  REWIRE THE ARENA, STOP THE ESCAPE"), GM->GetSaboteurIndex() + 1), RightX + S(50.f), BottomY + S(56.f), ChaosStyle::Ink, 13.f, EChaosFontWeight::Bold, -1, 150);

	if (GM->GetRoundNumber() > 1)
	{
		DrawChip(TEXT("ROLES SWAPPED"), CX, CY + S(90.f), ChaosUtil::WarningColor, ChaosStyle::Ink, 14.f, 0);
	}

	// Countdown digit that punches in every second.
	const float Remaining = GM->GetPhaseTimeRemaining();
	const int32 Count = FMath::Max(1, FMath::CeilToInt(Remaining));
	const float Beat = FMath::Frac(Remaining);
	DrawLabel(FString::FromInt(Count), CX, CY + S(130.f) - S(10.f) * Beat, WithAlpha(ChaosStyle::White, 0.35f + 0.65f * Beat), 110.f + 30.f * Beat, EChaosFontWeight::Bold, 0);
}

void AChaosHUD::DrawRoundResult()
{
	const FChaosRoundSummary& Summary = GM->GetLastRoundSummary();
	const FLinearColor WinColor = Summary.bRunnerWon ? ChaosUtil::RunnerColor : ChaosUtil::SaboteurColor;
	const float Appear = EaseIn(PhaseChangeTime, 0.45f);
	const float CX = ScreenW * 0.5f;
	const float CY = ScreenH * 0.5f;

	FillRect(0.f, 0.f, ScreenW, ScreenH, FLinearColor(0.f, 0.f, 0.01f, 0.5f * Appear));

	// Full-width band in the winner's colour.
	const float BandH = S(330.f) * Appear;
	const float BandY = CY - S(200.f) - BandH * 0.5f + S(165.f);
	FillGradient(0.f, BandY, CX, BandH, FLinearColor(0.f, 0.f, 0.01f, 0.85f), FLinearColor(WinColor.R * 0.25f, WinColor.G * 0.25f, WinColor.B * 0.25f, 0.9f));
	FillGradient(CX, BandY, CX, BandH, FLinearColor(WinColor.R * 0.25f, WinColor.G * 0.25f, WinColor.B * 0.25f, 0.9f), FLinearColor(0.f, 0.f, 0.01f, 0.85f));
	FillGradient(0.f, BandY, CX, S(3.f), WithAlpha(WinColor, 0.f), WinColor);
	FillGradient(CX, BandY, CX, S(3.f), WinColor, WithAlpha(WinColor, 0.f));
	FillGradient(0.f, BandY + BandH - S(3.f), CX, S(3.f), WithAlpha(WinColor, 0.f), WinColor);
	FillGradient(CX, BandY + BandH - S(3.f), CX, S(3.f), WinColor, WithAlpha(WinColor, 0.f));
	if (Appear < 0.6f)
	{
		return;
	}

	float RowY = BandY + S(28.f);
	DrawLabel(FString::Printf(TEXT("ROUND %d COMPLETE"), Summary.RoundNumber), CX, RowY, ChaosStyle::Text, 16.f, EChaosFontWeight::Bold, 0, 500);
	RowY += S(26.f);
	DrawLabel(Summary.bRunnerWon ? TEXT("RUNNER ESCAPES") : TEXT("SABOTEUR HOLDS"), CX, RowY, WinColor, 76.f, EChaosFontWeight::Bold, 0, 40);
	RowY += S(92.f);
	DrawLabel(FString::Printf(TEXT("PLAYER %d WINS THE ROUND  -  %s"), Summary.WinnerIndex + 1, *Summary.Reason.ToUpper()), CX, RowY, ChaosStyle::White, 16.f, EChaosFontWeight::Bold, 0, 120);
	RowY += S(46.f);

	// Points awarded and the running match score.
	for (int32 PlayerIndex = 0; PlayerIndex < ChaosUtil::NumPlayers; ++PlayerIndex)
	{
		const bool bLeft = PlayerIndex == 0;
		const float ColX = bLeft ? CX - S(60.f) : CX + S(60.f);
		const int32 Align = bLeft ? 1 : -1;
		DrawLabel(FString::Printf(TEXT("PLAYER %d"), PlayerIndex + 1), ColX, RowY, ChaosStyle::Muted, 13.f, EChaosFontWeight::Bold, Align, 300);
		DrawLabel(FString::FromInt(GM->GetScore(PlayerIndex)), ColX, RowY + S(18.f), ChaosStyle::White, 44.f, EChaosFontWeight::Bold, Align);
		DrawLabel(FString::Printf(TEXT("+%d"), Summary.PointsAwarded[PlayerIndex]), ColX + (bLeft ? -S(130.f) : S(130.f)), RowY + S(30.f),
			Summary.PointsAwarded[PlayerIndex] > 0 ? ChaosStyle::Gold : ChaosStyle::Muted, 24.f, EChaosFontWeight::Bold, Align);
	}
	DrawLabel(TEXT("VS"), CX, RowY + S(30.f), ChaosStyle::Muted, 20.f, EChaosFontWeight::Bold, 0, 200);

	const float ButtonY = BandY + BandH + S(26.f);
	if (!GM->IsFinalRound())
	{
		DrawChip(FString::Printf(TEXT("NEXT ROUND: ROLES SWAP  -  PLAYER %d RUNS"), GM->GetSaboteurIndex() + 1), CX, ButtonY, ChaosUtil::WarningColor, ChaosStyle::Ink, 13.f, 0);
	}
	DrawMenuList(CX, ButtonY + S(44.f), 380.f, true);
	if (GM->RoundResultAutoContinue > 0.f)
	{
		DrawLabel(FString::Printf(TEXT("Continuing in %ds"), FMath::CeilToInt(GM->GetPhaseTimeRemaining())), CX, ButtonY + S(118.f), ChaosStyle::Muted, 14.f, EChaosFontWeight::Regular, 0);
	}
}

void AChaosHUD::DrawFinalResults()
{
	FillRect(0.f, 0.f, ScreenW, ScreenH, FLinearColor(0.f, 0.f, 0.01f, 0.72f));
	const float CX = ScreenW * 0.5f;
	const float Appear = EaseIn(PhaseChangeTime, 0.6f);
	const int32 Winner = GM->GetMatchWinnerIndex();
	const FLinearColor Headline = Winner == INDEX_NONE ? ChaosUtil::WarningColor : ChaosStyle::Gold;

	DrawLabel(TEXT("MATCH COMPLETE"), CX, S(120.f), WithAlpha(ChaosStyle::Muted, Appear), 18.f, EChaosFontWeight::Bold, 0, 500);
	DrawLabel(Winner == INDEX_NONE ? FString(TEXT("DRAW")) : FString::Printf(TEXT("PLAYER %d WINS"), Winner + 1), CX, S(148.f) + (1.f - Appear) * S(30.f),
		WithAlpha(Headline, Appear), 104.f, EChaosFontWeight::Bold, 0, 40);
	DrawLabel(FString::Printf(TEXT("ARENA  -  %s"), *ChaosUtil::GetArenaName(GM->GetSelectedArena())), CX, S(276.f), WithAlpha(ArenaAccent(GM->GetSelectedArena()), Appear), 15.f, EChaosFontWeight::Bold, 0, 400);

	// Score cards side by side; the winner's card is gold-edged and raised.
	const float CardW = S(380.f);
	const float CardH = S(180.f);
	const float Gap = S(40.f);
	const int32 Best = FMath::Max(1, FMath::Max(GM->GetScore(0), GM->GetScore(1)));
	for (int32 PlayerIndex = 0; PlayerIndex < ChaosUtil::NumPlayers; ++PlayerIndex)
	{
		const bool bWinner = PlayerIndex == Winner;
		const float X = CX + (PlayerIndex == 0 ? -CardW - Gap * 0.5f : Gap * 0.5f);
		const float Y = S(340.f) - (bWinner ? S(16.f) : 0.f);
		DrawPanel(X, Y, CardW, CardH, bWinner ? ChaosStyle::Gold : ChaosStyle::Muted);
		DrawLabel(FString::Printf(TEXT("PLAYER %d"), PlayerIndex + 1), X + S(30.f), Y + S(24.f), ChaosStyle::Text, 18.f, EChaosFontWeight::Bold, -1, 200);
		if (bWinner)
		{
			DrawChip(TEXT("WINNER"), X + CardW - S(30.f), Y + S(22.f), ChaosStyle::Gold, ChaosStyle::Ink, 12.f, 1);
		}
		DrawLabel(FString::FromInt(GM->GetScore(PlayerIndex)), X + S(30.f), Y + S(56.f), bWinner ? ChaosStyle::White : ChaosStyle::Text, 64.f, EChaosFontWeight::Bold);
		DrawBar(X + S(30.f), Y + CardH - S(30.f), CardW - S(60.f), S(6.f), Appear * GM->GetScore(PlayerIndex) / static_cast<float>(Best), bWinner ? ChaosStyle::Gold : ChaosStyle::Muted);
	}

	DrawMenuList(CX, S(590.f), 460.f, true);
}

void AChaosHUD::DrawMessages()
{
	const TArray<FChaosHudMessage>& Messages = GM->GetMessages();
	const float Now = GM->GetNow();
	const float CX = ScreenW * 0.5f;
	float Y = ScreenH - S(120.f);

	for (int32 Index = Messages.Num() - 1; Index >= 0 && Index >= Messages.Num() - 4; --Index)
	{
		const FChaosHudMessage& Message = Messages[Index];
		const float Alpha = FMath::Clamp((Message.ExpireTime - Now) / 0.4f, 0.f, 1.f);
		const FVector2D Size = MeasureLabel(Message.Text, 17.f, EChaosFontWeight::Bold, 40);
		const float W = Size.X + S(44.f);
		const float H = Size.Y + S(12.f);
		const float X = CX - W * 0.5f;
		FillChamfer(X, Y, W, H, S(8.f), FLinearColor(0.01f, 0.012f, 0.02f, 0.82f * Alpha), FLinearColor(0.01f, 0.012f, 0.02f, 0.6f * Alpha));
		FillRect(X, Y + S(8.f), S(4.f), H - S(8.f), WithAlpha(Message.Color, Alpha));
		DrawLabel(Message.Text, CX + S(4.f), Y + S(6.f), WithAlpha(Message.Color, Alpha), 17.f, EChaosFontWeight::Bold, 0, 40);
		Y -= H + S(8.f);
	}
}
