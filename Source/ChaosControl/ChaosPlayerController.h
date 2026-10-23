#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ChaosControlTypes.h"
#include "ChaosPlayerController.generated.h"

class AChaosGameMode;

/** One selectable entry of the current menu. Id is also the HUD hit-box name. */
struct FChaosMenuItem
{
	FName Id;
	FString Label;
	FString Detail;
};

/**
 * The single local PlayerController. It reads the whole keyboard every frame and routes each
 * player's keys to their avatar, which avoids the "only player 0 gets the keyboard" problem of
 * local multiplayer. It also drives keyboard/mouse navigation for every menu screen.
 */
UCLASS(Config = Game)
class CHAOSCONTROL_API AChaosPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AChaosPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Keys for Player 1 (default W/A/S/D, Space, E, Q). Override in DefaultGame.ini. */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Controls")
	FChaosKeyBindings Player1Keys;

	/** Keys for Player 2 (default arrows, Right Ctrl, Enter, Right Shift). */
	UPROPERTY(EditAnywhere, Config, Category = "Chaos|Controls")
	FChaosKeyBindings Player2Keys;

	TArray<FChaosMenuItem> GetMenuItems() const;
	int32 GetMenuSelection() const { return MenuSelection; }
	void SetMenuSelectionById(FName Id);
	void ActivateMenuItem(FName Id);
	bool IsShowingControls() const { return bShowControls; }
	float GetMenuOpenedRealTime() const { return MenuOpenedRealTime; }
	float GetSelectionChangedRealTime() const { return SelectionChangedRealTime; }

	const FChaosKeyBindings& GetKeysForPlayer(int32 PlayerIndex) const;
	static FString KeyName(const FKey& Key);
	FString DescribeMovementKeys(int32 PlayerIndex) const;

private:
	AChaosGameMode* GetChaosGameMode() const;
	FName GetMenuContext() const;
	void HandleMenuInput(AChaosGameMode* GM, FName Context);
	void HandleMenuBack(AChaosGameMode* GM, FName Context);
	void OnMenuSelectionChanged();
	void RouteInputToCharacter(AChaosGameMode* GM, int32 PlayerIndex, const FChaosKeyBindings& Keys);
	void UpdateViewTarget(AChaosGameMode* GM);

	int32 MenuSelection = 0;
	FName LastMenuContext;
	float MenuOpenedRealTime = 0.f;
	float SelectionChangedRealTime = 0.f;
	bool bShowControls = false;
};
