#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "ChaosControlTypes.h"
#include "ChaosHUD.generated.h"

class AChaosGameMode;
class AChaosPlayerController;
class UFont;

enum class EChaosFontWeight : uint8
{
	Light,
	Regular,
	Bold
};

/**
 * Canvas-drawn UI for every screen (PRD FR-11): main menu, arena selection, controls, in-game
 * HUD, round intro and results, final results and pause. Uses the engine's Roboto font family
 * (crisp at any resolution) and chamfered gradient panels. Menu entries are clickable hit boxes
 * and are also navigable from the keyboard through AChaosPlayerController.
 */
UCLASS()
class CHAOSCONTROL_API AChaosHUD : public AHUD
{
	GENERATED_BODY()

public:
	AChaosHUD();

	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;

private:
	// ---- Screens ----------------------------------------------------------------------------
	void DrawMainMenu();
	void DrawArenaSelect();
	void DrawControlsScreen();
	void DrawPauseMenu();
	void DrawGameplayHud();
	void DrawTimerModule();
	void DrawPlayerCard(int32 PlayerIndex, bool bRightSide);
	void DrawWorldMarkers();
	void DrawKeyHints();
	void DrawRoundIntro();
	void DrawRoundResult();
	void DrawFinalResults();
	void DrawMessages();

	// ---- Widgets ----------------------------------------------------------------------------
	void DrawMenuList(float X, float Y, float Width, bool bCentered);
	void DrawButton(const FString& Label, const FString& Detail, float X, float Y, float W, float H, bool bSelected, const FLinearColor& Accent);
	float DrawKeyCap(const FString& Key, float X, float Y, float Height, const FLinearColor& Accent);
	void DrawChip(const FString& Text, float X, float Y, const FLinearColor& Fill, const FLinearColor& TextColor, float Size, int32 Align = -1);

	// ---- Primitives -------------------------------------------------------------------------
	void FillPolygon(const TArray<FVector2D>& Points, const TArray<FLinearColor>& Colors);
	void FillRect(float X, float Y, float W, float H, const FLinearColor& Color);
	void FillGradient(float X, float Y, float W, float H, const FLinearColor& Left, const FLinearColor& Right);
	void FillGradientV(float X, float Y, float W, float H, const FLinearColor& Top, const FLinearColor& Bottom);
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Accent, float Alpha = 1.f, float Cut = 16.f);
	void FillChamfer(float X, float Y, float W, float H, float Cut, const FLinearColor& Left, const FLinearColor& Right);
	void DrawDiamond(float CX, float CY, float Radius, const FLinearColor& Color);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color, int32 Segments = 0);
	float DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float SizePx, EChaosFontWeight Weight = EChaosFontWeight::Regular, int32 Align = -1, int32 Spacing = 0);
	FVector2D MeasureLabel(const FString& Text, float SizePx, EChaosFontWeight Weight = EChaosFontWeight::Regular, int32 Spacing = 0) const;
	FSlateFontInfo MakeFont(float SizePx, EChaosFontWeight Weight, int32 Spacing) const;

	float S(float Value) const { return Value * UIScale; }
	float RealNow() const;
	/** 0..1 ease-out progress of an animation that started at StartTime (real seconds). */
	float EaseIn(float StartTime, float Duration) const;
	FLinearColor ArenaAccent(EChaosArena Arena) const;

	UPROPERTY()
	TObjectPtr<UFont> UIFont;

	AChaosGameMode* GM = nullptr;
	AChaosPlayerController* PC = nullptr;
	float UIScale = 1.f;
	float ScreenW = 1920.f;
	float ScreenH = 1080.f;

	int32 ShownScores[ChaosUtil::NumPlayers] = { 0, 0 };
	float ScoreChangeTime[ChaosUtil::NumPlayers] = { -10.f, -10.f };
	EChaosMatchPhase LastPhase = EChaosMatchPhase::MainMenu;
	float PhaseChangeTime = 0.f;
};
