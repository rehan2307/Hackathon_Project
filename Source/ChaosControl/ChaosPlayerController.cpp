#include "ChaosPlayerController.h"
#include "ChaosControl.h"
#include "ChaosArenaManager.h"
#include "ChaosGameMode.h"
#include "ChaosPlayerCharacter.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ChaosMenu
{
	// Blocks confirm input briefly after a menu appears, so a player still mashing
	// Enter (P2 interact) at the end of a round cannot skip the result screen by accident.
	constexpr float InputGuardSeconds = 0.75f;

	const FName Main(TEXT("Main"));
	const FName Arena(TEXT("Arena"));
	const FName Controls(TEXT("Controls"));
	const FName Pause(TEXT("Pause"));
	const FName Result(TEXT("Result"));
	const FName Final(TEXT("Final"));
}

AChaosPlayerController::AChaosPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bAutoManageActiveCameraTarget = false;
	bShouldPerformFullTickWhenPaused = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	Player1Keys.Up = EKeys::W;
	Player1Keys.Down = EKeys::S;
	Player1Keys.Left = EKeys::A;
	Player1Keys.Right = EKeys::D;
	Player1Keys.Jump = EKeys::SpaceBar;
	Player1Keys.Interact = EKeys::E;
	Player1Keys.Sabotage = EKeys::Q;

	Player2Keys.Up = EKeys::Up;
	Player2Keys.Down = EKeys::Down;
	Player2Keys.Left = EKeys::Left;
	Player2Keys.Right = EKeys::Right;
	Player2Keys.Jump = EKeys::RightControl;
	Player2Keys.Interact = EKeys::Enter;
	Player2Keys.Sabotage = EKeys::RightShift;
}

void AChaosPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AChaosPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	AChaosGameMode* GM = GetChaosGameMode();
	if (!GM)
	{
		return;
	}

	UpdateViewTarget(GM);

	const FName Context = GetMenuContext();
	if (Context != LastMenuContext)
	{
		LastMenuContext = Context;
		MenuSelection = Context == ChaosMenu::Arena ? static_cast<int32>(GM->GetSelectedArena()) : 0;
		MenuOpenedRealTime = GetWorld()->GetRealTimeSeconds();
	}

	if (!Context.IsNone())
	{
		HandleMenuInput(GM, Context);
		return;
	}

	const EChaosMatchPhase CurrentPhase = GM->GetPhase();
	const bool bInRound = CurrentPhase == EChaosMatchPhase::RoundStarting || CurrentPhase == EChaosMatchPhase::RoundActive;
	if (bInRound && (WasInputKeyJustPressed(EKeys::P) || WasInputKeyJustPressed(EKeys::Escape)))
	{
		GM->SetMenuPaused(true);
		return;
	}

	if (GM->IsRoundActive())
	{
		RouteInputToCharacter(GM, 0, Player1Keys);
		RouteInputToCharacter(GM, 1, Player2Keys);
	}
}

void AChaosPlayerController::RouteInputToCharacter(AChaosGameMode* GM, int32 PlayerIndex, const FChaosKeyBindings& Keys)
{
	AChaosPlayerCharacter* Avatar = GM->GetPlayerCharacter(PlayerIndex);
	if (!Avatar)
	{
		return;
	}

	FVector2D Move = FVector2D::ZeroVector;
	if (IsInputKeyDown(Keys.Up))    { Move.Y += 1.f; }
	if (IsInputKeyDown(Keys.Down))  { Move.Y -= 1.f; }
	if (IsInputKeyDown(Keys.Right)) { Move.X += 1.f; }
	if (IsInputKeyDown(Keys.Left))  { Move.X -= 1.f; }
	Avatar->ApplyMoveInput(Move);

	if (WasInputKeyJustPressed(Keys.Jump))
	{
		Avatar->StartJumpInput();
	}
	if (WasInputKeyJustReleased(Keys.Jump))
	{
		Avatar->StopJumpInput();
	}
	if (WasInputKeyJustPressed(Keys.Interact))
	{
		GM->HandleInteract(Avatar);
	}
	if (WasInputKeyJustPressed(Keys.Sabotage))
	{
		GM->HandleSabotage(Avatar);
	}
}

void AChaosPlayerController::UpdateViewTarget(AChaosGameMode* GM)
{
	AChaosArenaManager* Arena = GM->GetArenaManager();
	ACameraActor* ArenaCamera = Arena ? Arena->GetArenaCamera() : nullptr;
	if (ArenaCamera && GetViewTarget() != ArenaCamera)
	{
		SetViewTarget(ArenaCamera);
	}
}

// ---------------------------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------------------------

FName AChaosPlayerController::GetMenuContext() const
{
	const AChaosGameMode* GM = GetChaosGameMode();
	if (!GM)
	{
		return NAME_None;
	}

	if (GM->IsMenuPaused())
	{
		return bShowControls ? ChaosMenu::Controls : ChaosMenu::Pause;
	}

	switch (GM->GetPhase())
	{
	case EChaosMatchPhase::MainMenu:      return bShowControls ? ChaosMenu::Controls : ChaosMenu::Main;
	case EChaosMatchPhase::ArenaSelect:   return ChaosMenu::Arena;
	case EChaosMatchPhase::RoundComplete: return ChaosMenu::Result;
	case EChaosMatchPhase::MatchComplete: return ChaosMenu::Final;
	default:                              return NAME_None;
	}
}

TArray<FChaosMenuItem> AChaosPlayerController::GetMenuItems() const
{
	TArray<FChaosMenuItem> Items;
	const AChaosGameMode* GM = GetChaosGameMode();
	if (!GM)
	{
		return Items;
	}

	const FName Context = GetMenuContext();
	if (Context == ChaosMenu::Main)
	{
		Items.Add({ TEXT("Start"), TEXT("Start Game"), FString::Printf(TEXT("Arena: %s"), *ChaosUtil::GetArenaName(GM->GetSelectedArena())) });
		Items.Add({ TEXT("ArenaSelect"), TEXT("Arena Selection"), FString() });
		Items.Add({ TEXT("Controls"), TEXT("Controls"), FString() });
		Items.Add({ TEXT("Exit"), TEXT("Exit Game"), FString() });
	}
	else if (Context == ChaosMenu::Arena)
	{
		for (int32 Index = 0; Index < ChaosUtil::NumArenas; ++Index)
		{
			const EChaosArena Arena = static_cast<EChaosArena>(Index);
			Items.Add({ FName(*FString::Printf(TEXT("Arena%d"), Index)),
				FString::Printf(TEXT("%d. %s"), Index + 1, *ChaosUtil::GetArenaName(Arena)),
				ChaosUtil::GetArenaDescription(Arena) });
		}
		Items.Add({ TEXT("Back"), TEXT("Back"), FString() });
	}
	else if (Context == ChaosMenu::Controls)
	{
		Items.Add({ TEXT("Back"), TEXT("Back"), FString() });
	}
	else if (Context == ChaosMenu::Pause)
	{
		Items.Add({ TEXT("Resume"), TEXT("Resume"), FString() });
		Items.Add({ TEXT("Restart"), TEXT("Restart Match"), TEXT("Scores reset, same arena") });
		Items.Add({ TEXT("Controls"), TEXT("Controls"), FString() });
		Items.Add({ TEXT("MainMenu"), TEXT("Main Menu"), FString() });
	}
	else if (Context == ChaosMenu::Result)
	{
		Items.Add({ TEXT("Continue"), GM->IsFinalRound() ? TEXT("Continue to Results") : TEXT("Continue"), FString() });
	}
	else if (Context == ChaosMenu::Final)
	{
		Items.Add({ TEXT("PlayAgain"), TEXT("Play Again"), FString::Printf(TEXT("Arena: %s"), *ChaosUtil::GetArenaName(GM->GetSelectedArena())) });
		Items.Add({ TEXT("MainMenu"), TEXT("Return to Main Menu"), FString() });
	}
	return Items;
}

void AChaosPlayerController::HandleMenuInput(AChaosGameMode* GM, FName Context)
{
	const TArray<FChaosMenuItem> Items = GetMenuItems();
	if (Items.Num() == 0)
	{
		return;
	}

	MenuSelection = FMath::Clamp(MenuSelection, 0, Items.Num() - 1);
	const int32 PreviousSelection = MenuSelection;

	if (WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W))
	{
		MenuSelection = (MenuSelection + Items.Num() - 1) % Items.Num();
	}
	if (WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S))
	{
		MenuSelection = (MenuSelection + 1) % Items.Num();
	}
	if (MenuSelection != PreviousSelection)
	{
		OnMenuSelectionChanged();
	}

	if (GetWorld()->GetRealTimeSeconds() - MenuOpenedRealTime < ChaosMenu::InputGuardSeconds)
	{
		return;
	}

	if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::E))
	{
		UE_LOG(LogChaosControl, Log, TEXT("Menu confirm key pressed"));
		ActivateMenuItem(Items[MenuSelection].Id);
	}
	else if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::BackSpace) ||
		(Context == ChaosMenu::Pause && WasInputKeyJustPressed(EKeys::P)))
	{
		HandleMenuBack(GM, Context);
	}
}

void AChaosPlayerController::HandleMenuBack(AChaosGameMode* GM, FName Context)
{
	if (Context == ChaosMenu::Controls)
	{
		bShowControls = false;
	}
	else if (Context == ChaosMenu::Arena)
	{
		GM->CloseArenaSelect();
	}
	else if (Context == ChaosMenu::Pause)
	{
		GM->SetMenuPaused(false);
	}
}

void AChaosPlayerController::OnMenuSelectionChanged()
{
	SelectionChangedRealTime = GetWorld()->GetRealTimeSeconds();
	AChaosGameMode* GM = GetChaosGameMode();
	if (GM && GetMenuContext() == ChaosMenu::Arena && MenuSelection < ChaosUtil::NumArenas)
	{
		// Highlighting an arena rebuilds it behind the menu as a live preview.
		GM->PreviewArena(static_cast<EChaosArena>(MenuSelection));
	}
}

void AChaosPlayerController::SetMenuSelectionById(FName Id)
{
	const TArray<FChaosMenuItem> Items = GetMenuItems();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index].Id == Id && Index != MenuSelection)
		{
			MenuSelection = Index;
			OnMenuSelectionChanged();
			return;
		}
	}
}

void AChaosPlayerController::ActivateMenuItem(FName Id)
{
	AChaosGameMode* GM = GetChaosGameMode();
	if (!GM)
	{
		return;
	}
	UE_LOG(LogChaosControl, Log, TEXT("Menu item activated: %s (context %s)"), *Id.ToString(), *GetMenuContext().ToString());

	if (Id == FName(TEXT("Start")))
	{
		GM->StartMatch();
	}
	else if (Id == FName(TEXT("ArenaSelect")))
	{
		GM->OpenArenaSelect();
	}
	else if (Id == FName(TEXT("Controls")))
	{
		bShowControls = true;
	}
	else if (Id == FName(TEXT("Exit")))
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
	else if (Id == FName(TEXT("Arena0")))
	{
		GM->StartMatchOnArena(EChaosArena::NeonGrid);
	}
	else if (Id == FName(TEXT("Arena1")))
	{
		GM->StartMatchOnArena(EChaosArena::ReactorRun);
	}
	else if (Id == FName(TEXT("Arena2")))
	{
		GM->StartMatchOnArena(EChaosArena::ChaosCore);
	}
	else if (Id == FName(TEXT("Back")))
	{
		HandleMenuBack(GM, GetMenuContext());
	}
	else if (Id == FName(TEXT("Resume")))
	{
		GM->SetMenuPaused(false);
	}
	else if (Id == FName(TEXT("Restart")))
	{
		bShowControls = false;
		GM->RestartMatch();
	}
	else if (Id == FName(TEXT("Continue")))
	{
		GM->ContinueAfterRound();
	}
	else if (Id == FName(TEXT("PlayAgain")))
	{
		GM->PlayAgain();
	}
	else if (Id == FName(TEXT("MainMenu")))
	{
		bShowControls = false;
		GM->ReturnToMainMenu();
	}
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

AChaosGameMode* AChaosPlayerController::GetChaosGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AChaosGameMode>() : nullptr;
}

const FChaosKeyBindings& AChaosPlayerController::GetKeysForPlayer(int32 PlayerIndex) const
{
	return PlayerIndex == 0 ? Player1Keys : Player2Keys;
}

FString AChaosPlayerController::KeyName(const FKey& Key)
{
	return Key.GetDisplayName(false).ToString();
}

FString AChaosPlayerController::DescribeMovementKeys(int32 PlayerIndex) const
{
	const FChaosKeyBindings& Keys = GetKeysForPlayer(PlayerIndex);
	if (Keys.Up == EKeys::Up && Keys.Down == EKeys::Down && Keys.Left == EKeys::Left && Keys.Right == EKeys::Right)
	{
		return TEXT("Arrow keys");
	}
	return FString::Printf(TEXT("%s/%s/%s/%s"), *KeyName(Keys.Up), *KeyName(Keys.Left), *KeyName(Keys.Down), *KeyName(Keys.Right));
}
