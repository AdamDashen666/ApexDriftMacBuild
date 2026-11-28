#include "Core/ApexDriftPlayerController.h"
#include "Core/ApexDriftSaveGame.h"
#include "Core/ApexGraphicsSettingsManager.h"
#include "Core/ApexKeyBindings.h"
#include "Core/ApexGameInstance.h"
#include "Core/ApexMapRegistry.h"
#include "Core/ApexPaintRegistry.h"
#include "Vehicle/ApexCarRegistry.h"
#include "Vehicle/ApexDriftVehiclePawn.h"
#include "Track/ApexTrackSplineActor.h"
#include "Track/ApexWeatherManager.h"
#include "Track/ApexSkidmarkManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Gameplay/ApexDriftScoreComponent.h"
#include "Audio/ApexVehicleAudioComponent.h"
#include "UI/ApexHudWidget.h"
#include "UI/ApexMenuWidgets.h"
#include "UI/ApexMenuScreens.h"
#include "UI/ApexSettingsScreen.h"
#include "Vehicle/ApexDriftVFXComponent.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerStart.h"
#include "Track/ApexPropField.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"

AApexDriftPlayerController::AApexDriftPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = true;
}

void AApexDriftPlayerController::BuildProgrammaticInput()
{
	if (VehicleIMC)
	{
		return;
	}

	VehicleIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_ApexDrift"));

	auto MakeAction = [&](const TCHAR* Name, EInputActionValueType Type, bool bTriggerPaused = false) -> UInputAction*
	{
		UInputAction* Act = NewObject<UInputAction>(this, Name);
		Act->ValueType = Type;
		Act->bTriggerWhenPaused = bTriggerPaused;
		return Act;
	};

	IA_Throttle   = MakeAction(TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	IA_Brake      = MakeAction(TEXT("IA_Brake"), EInputActionValueType::Axis1D);
	IA_Steering   = MakeAction(TEXT("IA_Steering"), EInputActionValueType::Axis1D);
	IA_Handbrake  = MakeAction(TEXT("IA_Handbrake"), EInputActionValueType::Boolean);
	IA_ShiftUp    = MakeAction(TEXT("IA_ShiftUp"), EInputActionValueType::Boolean);
	IA_ShiftDown  = MakeAction(TEXT("IA_ShiftDown"), EInputActionValueType::Boolean);
	IA_Camera     = MakeAction(TEXT("IA_Camera"), EInputActionValueType::Boolean);
	IA_LookBack   = MakeAction(TEXT("IA_LookBack"), EInputActionValueType::Boolean);
	IA_Reset      = MakeAction(TEXT("IA_Reset"), EInputActionValueType::Boolean);
	IA_Headlights = MakeAction(TEXT("IA_Headlights"), EInputActionValueType::Boolean);
	IA_AutoRun    = MakeAction(TEXT("IA_AutoRun"), EInputActionValueType::Boolean);
	IA_Director   = MakeAction(TEXT("IA_Director"), EInputActionValueType::Boolean);
	IA_Pause      = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean, true);
	IA_Look       = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);

	RebuildKeyMappings();
}

void AApexDriftPlayerController::RebuildKeyMappings()
{
	if (!VehicleIMC)
	{
		return;
	}
	// 键盘主键来自改键表（ApexKeys），方向键、手柄、Esc / P 固定
	using ApexKeys::Get;
	VehicleIMC->UnmapAll();

	// Throttle
	VehicleIMC->MapKey(IA_Throttle, Get(EApexBind::Throttle));
	VehicleIMC->MapKey(IA_Throttle, EKeys::Up);
	VehicleIMC->MapKey(IA_Throttle, EKeys::Gamepad_RightTriggerAxis);

	// Brake / Reverse
	VehicleIMC->MapKey(IA_Brake, Get(EApexBind::Brake));
	VehicleIMC->MapKey(IA_Brake, EKeys::Down);
	VehicleIMC->MapKey(IA_Brake, EKeys::Gamepad_LeftTriggerAxis);

	// Steering (Negate on left keys)
	UInputModifierNegate* NegateMod = NewObject<UInputModifierNegate>(VehicleIMC);
	VehicleIMC->MapKey(IA_Steering, Get(EApexBind::SteerLeft)).Modifiers.Add(NegateMod);
	VehicleIMC->MapKey(IA_Steering, EKeys::Left).Modifiers.Add(NegateMod);
	VehicleIMC->MapKey(IA_Steering, Get(EApexBind::SteerRight));
	VehicleIMC->MapKey(IA_Steering, EKeys::Right);
	UInputModifierDeadZone* StickDead = NewObject<UInputModifierDeadZone>(VehicleIMC);
	StickDead->LowerThreshold = 0.16f;
	StickDead->UpperThreshold = 0.98f;
	StickDead->Type = EDeadZoneType::Axial;
	VehicleIMC->MapKey(IA_Steering, EKeys::Gamepad_LeftX).Modifiers.Add(StickDead);

	// Handbrake
	VehicleIMC->MapKey(IA_Handbrake, Get(EApexBind::Handbrake));
	VehicleIMC->MapKey(IA_Handbrake, EKeys::Gamepad_FaceButton_Bottom);
	VehicleIMC->MapKey(IA_Handbrake, EKeys::Gamepad_FaceButton_Left);

	// Manual Shift
	VehicleIMC->MapKey(IA_ShiftUp, Get(EApexBind::ShiftUp));
	VehicleIMC->MapKey(IA_ShiftUp, EKeys::Gamepad_RightShoulder);
	VehicleIMC->MapKey(IA_ShiftDown, Get(EApexBind::ShiftDown));
	VehicleIMC->MapKey(IA_ShiftDown, EKeys::Gamepad_LeftShoulder);

	// Camera / LookBack / Reset / Lights / Auto / Director / Pause
	VehicleIMC->MapKey(IA_Camera, Get(EApexBind::Camera));
	VehicleIMC->MapKey(IA_Camera, EKeys::Gamepad_FaceButton_Top);
	VehicleIMC->MapKey(IA_LookBack, Get(EApexBind::LookBack));
	VehicleIMC->MapKey(IA_LookBack, EKeys::Gamepad_RightThumbstick);
	VehicleIMC->MapKey(IA_Reset, Get(EApexBind::Reset));
	VehicleIMC->MapKey(IA_Reset, EKeys::Gamepad_Special_Left);
	VehicleIMC->MapKey(IA_Headlights, Get(EApexBind::Headlights));
	VehicleIMC->MapKey(IA_Headlights, EKeys::Gamepad_DPad_Up);
	VehicleIMC->MapKey(IA_AutoRun, Get(EApexBind::AutoRun));
	VehicleIMC->MapKey(IA_AutoRun, EKeys::Gamepad_DPad_Down);
	VehicleIMC->MapKey(IA_Director, Get(EApexBind::Director));
	VehicleIMC->MapKey(IA_Director, EKeys::Gamepad_DPad_Right);
	VehicleIMC->MapKey(IA_Pause, EKeys::Escape);
	VehicleIMC->MapKey(IA_Pause, EKeys::P);
	VehicleIMC->MapKey(IA_Pause, EKeys::Gamepad_Special_Right);

	// Look Orbit
	VehicleIMC->MapKey(IA_Look, EKeys::Mouse2D);
}

void AApexDriftPlayerController::ApplyKeyBindings()
{
	RebuildKeyMappings();
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Sub->RequestRebuildControlMappings();
		}
	}
	if (SaveData)
	{
		SaveData->SaveToDisk();
	}
}

void AApexDriftPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SaveData = UApexDriftSaveGame::LoadOrCreate();
	ApexKeys::Load(SaveData);
	CurrentMapIndex = FApexMapRegistry::FindIndexForWorld(GetWorld());

	// Shipping 不接受命令行地图名，跑分/验证用 -ApexMap=MomijiTouge 指定
	FString MapArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexMap="), MapArg))
	{
		const TArray<FApexMapInfo>& Maps = FApexMapRegistry::GetMaps();
		for (int32 i = 0; i < Maps.Num(); ++i)
		{
			if (Maps[i].Id == FName(*MapArg) && i != CurrentMapIndex)
			{
				if (UApexGameInstance* GI = GetGameInstance<UApexGameInstance>())
				{
					GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(GI, [GI, i]() { GI->TravelToMap(i); }));
				}
			}
		}
	}

	for (TActorIterator<AApexTrackSplineActor> It(GetWorld()); It; ++It) { TrackActor = *It; break; }
	for (TActorIterator<AApexWeatherManager> It(GetWorld()); It; ++It) { WeatherManager = *It; break; }
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		StartGridTransform = (*It)->GetActorTransform();
		bHasStartGrid = true;
		break;
	}

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			BuildProgrammaticInput();
			Sub->AddMappingContext(VehicleIMC, 0);
		}
		CreateWidgets();
	}

	ApplyAndSaveSettings(false);
	if (WeatherManager && SaveData)
	{
		const FName MapId = FApexMapRegistry::GetMaps()[CurrentMapIndex].Id;
		int32 WeatherIdx = SaveData->GetWeatherForMap(MapId);
		FParse::Value(FCommandLine::Get(), TEXT("ApexWeather="), WeatherIdx);
		WeatherManager->ApplyWeatherIndex(WeatherIdx);
	}
	if (VehiclePawn && SaveData)
	{
		VehiclePawn->ApplyPaintPreset(SaveData->PaintPresetIndex);
	}

	ReturnToMainMenu();

	if (UApexGameInstance* GI = GetGameInstance<UApexGameInstance>())
	{
		if (GI->bPendingAutoStart)
		{
			// 等车辆自己的 BeginPlay 跑完再开赛，否则会被它的初始化覆盖回展示镜头
			GI->bPendingAutoStart = false;
			const EApexGameModeType Pending = GI->PendingMode;
			const bool bAuto = GI->bPendingAutoRun;
			GI->bPendingAutoRun = false;
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Pending, bAuto]() { if (bAuto) StartAutoRun(); else StartRaceSession(Pending); }));
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyGameplay")))
	{
		bVerifyGameplayMode = true;
		bExternalDrive = true;
		bShouldPerformFullTickWhenPaused = true;
		PrimaryActorTick.bTickEvenWhenPaused = true;
		VerifyPhase = 1;
		VerifyPhaseStartTime = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("[ApexVerifyGameplay] Enabled automated UI + drift verification sequence."));
	}
}

void AApexDriftPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildProgrammaticInput();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// 油门/刹车/方向/手刹在 PollDrivingInput 里每帧读取当前值
		EIC->BindAction(IA_ShiftUp, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnShiftUp);
		EIC->BindAction(IA_ShiftDown, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnShiftDown);

		EIC->BindAction(IA_Camera, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnCycleCamera);
		EIC->BindAction(IA_LookBack, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnLookBackStart);
		EIC->BindAction(IA_LookBack, ETriggerEvent::Completed, this, &AApexDriftPlayerController::OnLookBackStop);
		EIC->BindAction(IA_Reset, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnResetCar);
		EIC->BindAction(IA_Headlights, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnToggleLights);
		EIC->BindAction(IA_Director, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnToggleDirector);
		EIC->BindAction(IA_AutoRun, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnToggleAuto);
		EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AApexDriftPlayerController::OnTogglePause);
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AApexDriftPlayerController::OnLookAround);
	}
}

void AApexDriftPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	VehiclePawn = Cast<AApexDriftVehiclePawn>(InPawn);
	if (VehiclePawn)
	{
		if (!bHasStartGrid)
		{
			StartGridTransform = VehiclePawn->GetActorTransform();
			bHasStartGrid = true;
		}
		if (SaveData)
		{
			VehiclePawn->ApplyPaintPreset(SaveData->PaintPresetIndex);
			VehiclePawn->ApplyHandlingSettings(SaveData->HandlingAssist, SaveData->bManualTransmission, SaveData->BaseCameraFOV, SaveData->bTractionControl);
			if (VehiclePawn->VehicleAudioComp)
			{
				VehiclePawn->VehicleAudioComp->ApplyVolumeSettings(SaveData->MasterVolume, SaveData->EngineVolume, SaveData->TireVolume, SaveData->SfxVolume);
			}
		}
	}
}

void AApexDriftPlayerController::CreateWidgets()
{
	HudWidget = CreateWidget<UApexHudWidget>(this, UApexHudWidget::StaticClass());
	if (HudWidget) HudWidget->AddToViewport(1);

	MainMenuWidget = CreateWidget<UApexMainMenuWidget>(this, UApexMainMenuWidget::StaticClass());
	if (MainMenuWidget)
	{
		MainMenuWidget->AddToViewport(5);
		MainMenuWidget->InitController(this, SaveData);
	}

	PauseMenuWidget = CreateWidget<UApexPauseMenuWidget>(this, UApexPauseMenuWidget::StaticClass());
	if (PauseMenuWidget)
	{
		PauseMenuWidget->AddToViewport(10);
		PauseMenuWidget->InitController(this);
	}

	SettingsWidget = CreateWidget<UApexSettingsWidget>(this, UApexSettingsWidget::StaticClass());
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(15);
		SettingsWidget->InitController(this, SaveData);
	}

	FinishWidget = CreateWidget<UApexFinishWidget>(this, UApexFinishWidget::StaticClass());
	if (FinishWidget)
	{
		FinishWidget->AddToViewport(12);
	}
}

void AApexDriftPlayerController::UpdateWidgetVisibility()
{
	if (HudWidget)
	{
		const bool bShowHud = (CurrentGameState == EApexGameState::Countdown || CurrentGameState == EApexGameState::Playing);
		HudWidget->SetVisibility(bShowHud ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetVisibility((CurrentGameState == EApexGameState::MainMenu && !bInSettingsOverlay)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (CurrentGameState == EApexGameState::MainMenu) MainMenuWidget->RefreshLabels();
	}
	if (PauseMenuWidget)
	{
		PauseMenuWidget->SetVisibility((CurrentGameState == EApexGameState::Paused && !bInSettingsOverlay)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SettingsWidget)
	{
		SettingsWidget->SetVisibility(bInSettingsOverlay ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bInSettingsOverlay) SettingsWidget->RefreshValues();
	}
	if (FinishWidget)
	{
		FinishWidget->SetVisibility((CurrentGameState == EApexGameState::Finished)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	const bool bNeedCursor = (CurrentGameState != EApexGameState::Playing && CurrentGameState != EApexGameState::Countdown) || bInSettingsOverlay;
	bShowMouseCursor = bNeedCursor;
	if (bNeedCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
}

void AApexDriftPlayerController::PlaceVehicleAtStartGrid()
{
	if (!VehiclePawn) return;

	FTransform TargetXf = StartGridTransform;
	if (!bHasStartGrid && TrackActor && TrackActor->TrackSpline)
	{
		const FVector Loc = TrackActor->TrackSpline->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
		FVector Dir = TrackActor->TrackSpline->GetDirectionAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
		Dir.Z = 0.0f;
		Dir.Normalize();
		TargetXf = FTransform(Dir.Rotation(), Loc + FVector(0.0f, 0.0f, 55.0f), FVector::OneVector);
	}

	VehiclePawn->SetActorTransform(TargetXf, false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* M = VehiclePawn->GetMesh())
	{
		M->SetPhysicsLinearVelocity(FVector::ZeroVector);
		M->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(VehiclePawn->GetVehicleMovement()))
	{
		Move->ResetVehicleState();
	}

	for (TActorIterator<AApexSkidmarkManager> It(GetWorld()); It; ++It)
	{
		(*It)->ClearAllSkidmarks();
	}
	for (TActorIterator<AApexPropField> It(GetWorld()); It; ++It)
	{
		(*It)->ResetAll();
	}
}

void AApexDriftPlayerController::StartRaceSession(EApexGameModeType ModeType)
{
	PlayMenuClickSound();
	if (CurrentGameState != EApexGameState::MainMenu && CurrentGameState != EApexGameState::Finished)
	{
		if (VehiclePawn && VehiclePawn->DriftScoreComp) VehiclePawn->DriftScoreComp->BankCurrentDrift();
		CommitSessionRecord();
	}
	bAutoSession = false;
	bAutoPending = false;
	bAutoWasRunning = false;
	if (VehiclePawn && VehiclePawn->IsAutoRunning()) VehiclePawn->EndAutoRun();
	CurrentMode = ModeType;
	bInSettingsOverlay = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	PlaceVehicleAtStartGrid();

	if (VehiclePawn)
	{
		VehiclePawn->SetShowroomOrbitMode(false);
		VehiclePawn->SetPausedState(false);
		VehiclePawn->SetControlsLocked(true);
		if (VehiclePawn->DriftScoreComp)
		{
			VehiclePawn->DriftScoreComp->ResetSession(CurrentMode, TrackActor);
		}
	}

	CurrentGameState = EApexGameState::Countdown;
	CountdownTimer = 3.2f;
	LastCountdownBeep = 4;
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::RestartCurrentRace()
{
	StartRaceSession(CurrentMode);
}

void AApexDriftPlayerController::ReturnToMainMenu()
{
	PlayMenuClickSound();
	if (CurrentGameState == EApexGameState::Playing || CurrentGameState == EApexGameState::Paused)
	{
		if (VehiclePawn && VehiclePawn->DriftScoreComp) VehiclePawn->DriftScoreComp->BankCurrentDrift();
		CommitSessionRecord();
	}
	bInSettingsOverlay = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (VehiclePawn && VehiclePawn->IsAutoRunning()) VehiclePawn->EndAutoRun();
	bAutoSession = false;
	bAutoPending = false;
	bAutoWasRunning = false;
	PlaceVehicleAtStartGrid();

	if (VehiclePawn)
	{
		VehiclePawn->SetPausedState(false);
		VehiclePawn->SetControlsLocked(true);
		VehiclePawn->SetShowroomOrbitMode(true);
	}

	CurrentGameState = EApexGameState::MainMenu;
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::PauseGame()
{
	if (CurrentGameState != EApexGameState::Playing && CurrentGameState != EApexGameState::Countdown)
	{
		return;
	}
	PlayMenuClickSound();
	CurrentGameState = EApexGameState::Paused;
	bInSettingsOverlay = false;
	if (VehiclePawn) VehiclePawn->SetPausedState(true);
	UGameplayStatics::SetGamePaused(GetWorld(), true);
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::ResumeGame()
{
	if (CurrentGameState != EApexGameState::Paused)
	{
		return;
	}
	PlayMenuClickSound();
	bInSettingsOverlay = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (VehiclePawn) VehiclePawn->SetPausedState(false);
	CurrentGameState = (CountdownTimer > 0.0f) ? EApexGameState::Countdown : EApexGameState::Playing;
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::OpenSettingsMenu()
{
	PlayMenuClickSound();
	StateBeforeSettings = CurrentGameState;
	bInSettingsOverlay = true;
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::CloseSettingsMenu()
{
	PlayMenuClickSound();
	bInSettingsOverlay = false;
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::SetVehicleCar(int32 CarIndex)
{
	PlayMenuClickSound();
	if (SaveData)
	{
		SaveData->CarPaint.FindOrAdd(FApexCarRegistry::Get(SaveData->VehicleIndex).Id) = SaveData->PaintPresetIndex;
		SaveData->VehicleIndex = FApexCarRegistry::Wrap(CarIndex);
		const FApexCarInfo& Info = FApexCarRegistry::Get(SaveData->VehicleIndex);
		const int32* Own = SaveData->CarPaint.Find(Info.Id);
		SaveData->PaintPresetIndex = Own ? *Own : Info.DefaultPaint;
		if (VehiclePawn)
		{
			VehiclePawn->SetCar(SaveData->VehicleIndex);
			VehiclePawn->ApplyPaintPreset(SaveData->PaintPresetIndex);
		}
		SaveData->SaveToDisk();
	}
}

void AApexDriftPlayerController::SetVehiclePaint(int32 PaintIndex)
{
	PlayMenuClickSound();
	if (SaveData)
	{
		SaveData->PaintPresetIndex = FApexPaintRegistry::Wrap(PaintIndex);
		SaveData->CarPaint.Add(FApexCarRegistry::Get(SaveData->VehicleIndex).Id, SaveData->PaintPresetIndex);
		if (VehiclePawn) VehiclePawn->ApplyPaintPreset(SaveData->PaintPresetIndex);
		SaveData->SaveToDisk();
	}
}

void AApexDriftPlayerController::SetWeatherIndex(int32 WeatherIndex)
{
	PlayMenuClickSound();
	const int32 Idx = FMath::Clamp(WeatherIndex, 0, 2);
	if (WeatherManager)
	{
		WeatherManager->ApplyWeatherIndex(Idx);
		if (VehiclePawn)
		{
			VehiclePawn->SetHeadlightsEnabled(AApexWeatherManager::GetSetup(WeatherManager->MapId, Idx).TrackLights > 0.3f);
		}
	}
	if (SaveData)
	{
		SaveData->MapWeather.Add(FApexMapRegistry::GetMaps()[CurrentMapIndex].Id, Idx);
		SaveData->SaveToDisk();
	}
}

int32 AApexDriftPlayerController::GetCurrentWeatherIndex() const
{
	return WeatherManager ? WeatherManager->CurrentWeatherIndex : 0;
}

void AApexDriftPlayerController::PreviewMap(int32 MapIndex)
{
	const int32 Idx = FApexMapRegistry::ClampIndex(MapIndex);
	if (Idx == CurrentMapIndex)
	{
		return;
	}
	PlayMenuClickSound();
	if (SaveData)
	{
		SaveData->MapIndex = Idx;
		SaveData->SaveToDisk();
	}
	if (UApexGameInstance* GI = GetGameInstance<UApexGameInstance>())
	{
		GI->bPendingAutoStart = false;
		GI->TravelToMap(Idx);
	}
}

void AApexDriftPlayerController::StartRaceOnMap(int32 MapIndex, EApexGameModeType ModeType)
{
	const int32 Idx = FApexMapRegistry::ClampIndex(MapIndex);
	if (SaveData)
	{
		SaveData->MapIndex = Idx;
		SaveData->LastMode = ModeType;
		SaveData->SaveToDisk();
	}
	if (Idx == CurrentMapIndex)
	{
		StartRaceSession(ModeType);
		return;
	}
	PlayMenuClickSound();
	if (UApexGameInstance* GI = GetGameInstance<UApexGameInstance>())
	{
		GI->bPendingAutoStart = true;
		GI->PendingMode = ModeType;
		GI->TravelToMap(Idx);
	}
}

void AApexDriftPlayerController::ResetVehicleToTrack()
{
	if (VehiclePawn)
	{
		VehiclePawn->ResetToTrack();
	}
}

void AApexDriftPlayerController::ApplyAndSaveSettings(bool bIncludeVideoMode)
{
	PlayMenuClickSound();
	if (!SaveData) return;

	FApexGraphicsSettingsManager::ApplyAllSettings(GetWorld(), SaveData, bIncludeVideoMode);

	if (VehiclePawn)
	{
		VehiclePawn->ApplyHandlingSettings(SaveData->HandlingAssist, SaveData->bManualTransmission, SaveData->BaseCameraFOV, SaveData->bTractionControl);
		if (VehiclePawn->VehicleAudioComp)
		{
			VehiclePawn->VehicleAudioComp->ApplyVolumeSettings(SaveData->MasterVolume, SaveData->EngineVolume, SaveData->TireVolume, SaveData->SfxVolume);
		}
	}
	SaveData->SaveToDisk();
}

void AApexDriftPlayerController::PlayMenuClickSound()
{
	if (VehiclePawn && VehiclePawn->VehicleAudioComp)
	{
		VehiclePawn->VehicleAudioComp->PlayUISound(FName("Click"));
	}
}

int32 AApexDriftPlayerController::CommitSessionRecord()
{
	if (!SaveData || !VehiclePawn || bAutoSession)
	{
		return 0;   // 自动驾驶参与的一局不计成绩
	}
	const FApexDriftTelemetry& Tel = VehiclePawn->GetTelemetry();
	int32 Flags = 0;
	FApexMapRecord& Rec = SaveData->MapRecords.FindOrAdd(FApexMapRegistry::GetMaps()[CurrentMapIndex].Id);
	if (CurrentMode == EApexGameModeType::FreeDrift && Tel.TotalScore > Rec.BestFreeDrift)
	{
		Rec.BestFreeDrift = Tel.TotalScore;
		Flags |= ApexRecordFlags::Score;
	}
	else if (CurrentMode == EApexGameModeType::DriftAttack3Min && CurrentGameState == EApexGameState::Finished && Tel.TotalScore > Rec.BestAttack)
	{
		Rec.BestAttack = Tel.TotalScore;
		Flags |= ApexRecordFlags::Score;
	}
	if (Tel.BestSingleDrift > SaveData->BestSingleDrift)
	{
		SaveData->BestSingleDrift = Tel.BestSingleDrift;
		Flags |= ApexRecordFlags::SingleDrift;
	}
	SaveData->PeakDriftAngleDeg = FMath::Max(SaveData->PeakDriftAngleDeg, Tel.PeakAngleDeg);
	SaveData->BestComboMultiplier = FMath::Max(SaveData->BestComboMultiplier, Tel.MaxDriftCombo);
	if (Tel.BestLapTime > 0.1f && (Rec.BestLapSec <= 0.0f || Tel.BestLapTime < Rec.BestLapSec))
	{
		Rec.BestLapSec = Tel.BestLapTime;
		Flags |= ApexRecordFlags::Lap;
	}
	SaveData->SaveToDisk();
	return Flags;
}

void AApexDriftPlayerController::OnToggleAuto(const FInputActionValue&)
{
	if (VehiclePawn && (VehiclePawn->IsAutoRunning() || bAutoPending))
	{
		CancelAutoRun();
	}
	else if (CurrentGameState == EApexGameState::Playing || CurrentGameState == EApexGameState::Countdown)
	{
		StartAutoRun();
	}
}

void AApexDriftPlayerController::OnToggleDirector(const FInputActionValue&)
{
	if (VehiclePawn && VehiclePawn->IsAutoRunning()) VehiclePawn->ToggleDirector();
}

void AApexDriftPlayerController::StartAutoRun()
{
	StartRaceSession(EApexGameModeType::FreeDrift);   // 回发车格、倒计时、清零得分
	bAutoSession = true;
	bAutoPending = true;
	bAutoNeutral = false;
	if (VehiclePawn) VehiclePawn->PrepareAutoRun();
}

void AApexDriftPlayerController::StartAutoRunOnMap(int32 MapIndex)
{
	const int32 Idx = FApexMapRegistry::ClampIndex(MapIndex);
	if (Idx == CurrentMapIndex)
	{
		StartAutoRun();
		return;
	}
	PlayMenuClickSound();
	if (SaveData)
	{
		SaveData->MapIndex = Idx;
		SaveData->SaveToDisk();
	}
	if (UApexGameInstance* GI = GetGameInstance<UApexGameInstance>())
	{
		GI->bPendingAutoStart = true;
		GI->bPendingAutoRun = true;
		GI->PendingMode = EApexGameModeType::FreeDrift;
		GI->TravelToMap(Idx);
	}
}

void AApexDriftPlayerController::CancelAutoRun()
{
	bAutoPending = false;
	bAutoWasRunning = false;
	if (VehiclePawn && VehiclePawn->IsAutoRunning()) VehiclePawn->EndAutoRun();
	if (VehiclePawn && VehiclePawn->DriftScoreComp)
	{
		VehiclePawn->DriftScoreComp->ResetSession(CurrentMode, TrackActor);
		VehiclePawn->DriftScoreComp->ShowCallout(TEXT("MANUAL 已接管 · 本局不计成绩"), 2.4f);
	}
}

void AApexDriftPlayerController::TickAutoRun()
{
	if (!VehiclePawn) return;
	if (bAutoPending && CurrentGameState == EApexGameState::Playing)
	{
		bAutoPending = false;
		bAutoWasRunning = VehiclePawn->BeginAutoRun();
		UE_LOG(LogTemp, Display, TEXT("[ApexAuto] begin map=%s ok=%d"), *GetWorld()->GetMapName(), bAutoWasRunning ? 1 : 0);
	}
	if (bAutoWasRunning && !VehiclePawn->IsAutoRunning())
	{
		// 跑完一圈（或卡住）：复位到发车格，交还控制
		bAutoWasRunning = false;
		const bool bDone = VehiclePawn->GetAutoRun().IsFinished();
		const float Lap = VehiclePawn->GetAutoRun().GetStats().LapTime;
		UE_LOG(LogTemp, Display, TEXT("[ApexAuto] end done=%d lap=%.2f"), bDone ? 1 : 0, Lap);
		const int32 Score = VehiclePawn->DriftScoreComp ? VehiclePawn->DriftScoreComp->GetTotalScore() + VehiclePawn->DriftScoreComp->GetCurrentDriftScore() : 0;
		VehiclePawn->EndAutoRun();
		PlaceVehicleAtStartGrid();
		VehiclePawn->ResetHandlingState();
		if (VehiclePawn->DriftScoreComp)
		{
			VehiclePawn->DriftScoreComp->ResetSession(CurrentMode, TrackActor);
			VehiclePawn->DriftScoreComp->ShowCallout(bDone
				? FString::Printf(TEXT("AUTO LAP 一圈完成 %d:%05.2f · %d 分"), FMath::FloorToInt(Lap / 60.0f), FMath::Fmod(Lap, 60.0f), Score)
				: FString(TEXT("AUTO STOP 自动中断，已回到起点")), 5.0f);
		}
		bAutoSession = false;   // 已回到起点、得分清零，接下来是玩家自己的一局
	}
}

void AApexDriftPlayerController::PollDrivingInput()
{
	if (VehiclePawn && !bExternalDrive && (VehiclePawn->IsAutoRunning() || bAutoPending))
	{
		// 自动期间：松开过一次之后，再按任何驾驶键就接管
		if (const UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PlayerInput))
		{
			const bool bAny = FMath::Abs(EPI->GetActionValue(IA_Throttle).Get<float>()) > 0.25f || FMath::Abs(EPI->GetActionValue(IA_Brake).Get<float>()) > 0.25f
				|| FMath::Abs(EPI->GetActionValue(IA_Steering).Get<float>()) > 0.35f || EPI->GetActionValue(IA_Handbrake).Get<bool>();
			if (!bAny) bAutoNeutral = true;
			else if (bAutoNeutral && CurrentGameState == EApexGameState::Playing) CancelAutoRun();
		}
		if (VehiclePawn->IsAutoRunning() || bAutoPending) return;
	}
	if (bExternalDrive || !VehiclePawn || (CurrentGameState != EApexGameState::Playing && CurrentGameState != EApexGameState::Countdown))
	{
		return;
	}
	if (const UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PlayerInput))
	{
		VehiclePawn->SetThrottleInput(EPI->GetActionValue(IA_Throttle).Get<float>());
		VehiclePawn->SetBrakeInput(EPI->GetActionValue(IA_Brake).Get<float>());
		VehiclePawn->SetSteeringInput(EPI->GetActionValue(IA_Steering).Get<float>());
		VehiclePawn->SetHandbrakeInput(EPI->GetActionValue(IA_Handbrake).Get<bool>());
	}
}

void AApexDriftPlayerController::FinishRaceSession()
{
	CurrentGameState = EApexGameState::Finished;
	if (VehiclePawn)
	{
		VehiclePawn->SetControlsLocked(true);
		const int32 RecordFlags = CommitSessionRecord();
		if (FinishWidget)
		{
			FinishWidget->InitAndShow(this, VehiclePawn->GetTelemetry(), CurrentMode, RecordFlags);
		}
	}
	UpdateWidgetVisibility();
}

void AApexDriftPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float RealDelta = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.0005f, 0.25f);
	SmoothedDelta = (SmoothedDelta <= 0.0f) ? RealDelta : FMath::Lerp(SmoothedDelta, RealDelta, 0.05f);

	if (CurrentGameState == EApexGameState::Countdown)
	{
		CountdownTimer -= DeltaSeconds;
		const int32 Step = FMath::CeilToInt(CountdownTimer);
		if (Step != LastCountdownBeep && Step >= 1 && Step <= 3)
		{
			LastCountdownBeep = Step;
			if (VehiclePawn && VehiclePawn->VehicleAudioComp)
			{
				VehiclePawn->VehicleAudioComp->PlayUISound(FName("Countdown"));
			}
		}
		if (CountdownTimer <= 0.0f)
		{
			CountdownTimer = 0.0f;
			CurrentGameState = EApexGameState::Playing;
			if (VehiclePawn)
			{
				VehiclePawn->SetControlsLocked(false);
				if (VehiclePawn->VehicleAudioComp) VehiclePawn->VehicleAudioComp->PlayUISound(FName("Go"));
			}
		}
	}

	{
		// -ApexAutoDrift（“自动漂移.bat”）：进主菜单后自动开跑一圈
		static bool bAutoLaunch = FParse::Param(FCommandLine::Get(), TEXT("ApexAutoDrift"));
		if (bAutoLaunch && CurrentGameState == EApexGameState::MainMenu && GetWorld()->GetTimeSeconds() > 2.5f)
		{
			bAutoLaunch = false;
			StartAutoRun();
		}
	}
	PlayingTime = (CurrentGameState == EApexGameState::Playing || CurrentGameState == EApexGameState::Paused) ? PlayingTime + (CurrentGameState == EApexGameState::Playing ? DeltaSeconds : 0.0f) : 0.0f;
	TickAutoRun();
	PollDrivingInput();

	if (CurrentGameState == EApexGameState::Playing && VehiclePawn && VehiclePawn->DriftScoreComp)
	{
		if (VehiclePawn->DriftScoreComp->IsSessionFinished())
		{
			FinishRaceSession();
		}
	}

	if (HudWidget && VehiclePawn)
	{
		const TArray<FVector2D> EmptyPts;
		const TArray<FVector2D>& Pts = TrackActor ? TrackActor->MinimapNormalizedPoints : EmptyPts;
		FApexDriftTelemetry Tel = VehiclePawn->GetTelemetry();
		Tel.SmoothedFps = (SmoothedDelta > 0.0f) ? 1.0f / SmoothedDelta : 0.0f;
		HudWidget->UpdateTelemetry(Tel, CurrentMode, CurrentGameState, CountdownTimer, Pts);
	}

	if (bVerifyGameplayMode)
	{
		TickVerifyGameplay(DeltaSeconds);
	}
	static const bool bInputTest = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyInput"));
	if (bInputTest)
	{
		bShouldPerformFullTickWhenPaused = true;
		PrimaryActorTick.bTickEvenWhenPaused = true;
		TickVerifyInput();
	}
	static const bool bAutoTest = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyAuto"));
	if (bAutoTest)
	{
		bShouldPerformFullTickWhenPaused = true;
		PrimaryActorTick.bTickEvenWhenPaused = true;
		TickVerifyAuto();
	}
	static const bool bCars = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyCars"));
	if (bCars) TickVerifyCars();
	static const bool bKeysTest = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyKeys"));
	if (bKeysTest)
	{
		bShouldPerformFullTickWhenPaused = true;
		PrimaryActorTick.bTickEvenWhenPaused = true;
		TickVerifyKeys();
	}
	static const bool bPick = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyPick"));
	if (bPick)
	{
		bExternalDrive = true;
		TickVerifyPick();
	}
	static const bool bFlow = FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyFlow"));
	if (bFlow)
	{
		bExternalDrive = true;
		bShouldPerformFullTickWhenPaused = true;
		PrimaryActorTick.bTickEvenWhenPaused = true;
		TickVerifyFlow();
	}
}

void AApexDriftPlayerController::TickVerifyAuto()
{
	static int32 Step = 0;
	static double StepStart = 0.0;
	static TArray<FString> Out;
	static FApexMapRecord Rec0;
	static int32 Single0 = 0;
	static float Peak0 = 0.0f;
	static FVector Grid;
	const double Now = FPlatformTime::Seconds();
	if (StepStart == 0.0) StepStart = Now;
	const double E = Now - StepStart;
	if (!VehiclePawn || !SaveData) return;
	auto Key = [](const FKey& K, bool bDown)
	{
		const FKeyEvent Ev(K, FModifierKeysState(), 0, false, 0, 0);
		if (bDown) FSlateApplication::Get().ProcessKeyDownEvent(Ev); else FSlateApplication::Get().ProcessKeyUpEvent(Ev);
	};
	auto Log = [&](const TCHAR* Name)
	{
		Out.Add(FString::Printf(TEXT("%-26s state=%d 自动=%d 待开=%d 不计成绩=%d 进度=%.1f%% v=%.0f 离起点=%.0fm 分=%d 挡=%d"), Name, (int32)CurrentGameState, VehiclePawn->IsAutoRunning() ? 1 : 0, bAutoPending ? 1 : 0, bAutoSession ? 1 : 0,
			VehiclePawn->GetAutoRun().GetProgress() * 100.0f, VehiclePawn->GetTelemetry().SpeedKmh, FVector::Dist2D(VehiclePawn->GetActorLocation(), Grid) * 0.01f,
			VehiclePawn->GetTelemetry().TotalScore + VehiclePawn->GetTelemetry().CurrentDriftScore, VehiclePawn->GetTelemetry().Gear));
	};
	auto Next = [&]() { ++Step; StepStart = Now; };
	const FName MapId = FApexMapRegistry::GetMaps()[CurrentMapIndex].Id;
	switch (Step)
	{
	case 0:
		if (E > 5.0)
		{
			Rec0 = SaveData->MapRecords.FindRef(MapId);
			Single0 = SaveData->BestSingleDrift;
			Peak0 = SaveData->PeakDriftAngleDeg;
			StartRaceSession(EApexGameModeType::FreeDrift);
			Grid = VehiclePawn->GetActorLocation();
			Next();
		}
		break;
	case 1: if (CurrentGameState == EApexGameState::Playing && E > 0.5) { Key(EKeys::W, true); Next(); } break;
	case 2: if (E > 2.5) { Log(TEXT("自己开 2.5s")); Key(EKeys::F, true); Key(EKeys::F, false); Next(); } break;   // W 还按着
	case 3: if (E > 0.3) { Log(TEXT("按 F（W 未松）")); Key(EKeys::W, false); Next(); } break;
	case 4: if (VehiclePawn->IsAutoRunning() && VehiclePawn->GetAutoRun().GetElapsed() > 6.0f) { Log(TEXT("自动 6s")); PauseGame(); Next(); } break;
	case 5: if (E > 1.0) { Log(TEXT("暂停 1s")); ResumeGame(); Next(); } break;
	case 6: if (E > 1.5) { Log(TEXT("继续 1.5s（应仍在自动）")); Key(EKeys::D, true); Next(); } break;
	case 7: if (E > 0.25) { Log(TEXT("按 D 接管")); Key(EKeys::D, false); Next(); } break;
	case 8: if (E > 1.5) { Log(TEXT("接管 1.5s 后")); Key(EKeys::F, true); Key(EKeys::F, false); Next(); } break;
	case 9: if (VehiclePawn->IsAutoRunning() && VehiclePawn->GetAutoRun().GetElapsed() > 0.5f) { Log(TEXT("再按 F：从头 0.5s")); Next(); } break;
	case 10:
		if (!VehiclePawn->IsAutoRunning()) Next();
		else if (E > 150.0) { Log(TEXT("超时")); Step = 13; }
		break;
	case 11: if (E > 1.5) { Log(TEXT("跑完一圈 1.5s 后")); Key(EKeys::W, true); Next(); } break;
	case 12: if (E > 2.0) { Log(TEXT("自己开 2s")); Key(EKeys::W, false); Next(); } break;
	case 13:
		ReturnToMainMenu();
		{
			const FApexMapRecord R = SaveData->MapRecords.FindRef(MapId);
			Out.Add(FString::Printf(TEXT("存档：自由漂移 %d->%d 最快圈 %.2f->%.2f 单次 %d->%d 最大角 %.1f->%.1f"), Rec0.BestFreeDrift, R.BestFreeDrift, Rec0.BestLapSec, R.BestLapSec, Single0, SaveData->BestSingleDrift, Peak0, SaveData->PeakDriftAngleDeg));
		}
		FFileHelper::SaveStringArrayToFile(Out, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Bench/auto_input.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Next();
		FPlatformMisc::RequestExit(false);
		break;
	default: break;
	}
}

void AApexDriftPlayerController::TickVerifyInput()
{
	static int32 Step = 0;
	static double StepStart = 0.0;
	static TArray<FString> Out;
	const double Now = FPlatformTime::Seconds();
	if (StepStart == 0.0) StepStart = Now;
	const double E = Now - StepStart;
	auto Key = [](const FKey& K, bool bDown)
	{
		const FKeyEvent Ev(K, FModifierKeysState(), 0, false, 0, 0);
		if (bDown) FSlateApplication::Get().ProcessKeyDownEvent(Ev); else FSlateApplication::Get().ProcessKeyUpEvent(Ev);
	};
	auto Log = [&](const TCHAR* Name)
	{
		const FApexDriftTelemetry& T = VehiclePawn->GetTelemetry();
		Out.Add(FString::Printf(TEXT("%-28s state=%d thr=%.2f brk=%.2f steer=%.2f hand=%d drift=%d v=%.0f slip=%.1f"), Name, (int32)CurrentGameState, T.ThrottleInput, T.BrakeInput, VehiclePawn->GetRawSteering(), T.bHandbrakeActive ? 1 : 0, VehiclePawn->IsDriftActive() ? 1 : 0, T.SpeedKmh, T.SlipAngleDeg));
	};
	auto Next = [&]() { ++Step; StepStart = Now; };
	if (!VehiclePawn) return;
	switch (Step)
	{
	case 0: if (E > 5.0) { StartRaceSession(EApexGameModeType::FreeDrift); Next(); } break;
	case 1: if (CurrentGameState == EApexGameState::Playing && E > 0.5) { Key(EKeys::W, true); Next(); } break;
	case 2: if (E > 3.5) { Log(TEXT("按住 W 3.5s")); Key(EKeys::D, true); Next(); } break;
	case 3: if (E > 0.25) { Log(TEXT("W+D")); Key(EKeys::SpaceBar, true); Next(); } break;
	case 4: if (E > 0.35) { Log(TEXT("W+D+空格")); Key(EKeys::SpaceBar, false); Next(); } break;
	case 5: if (E > 0.4) { Log(TEXT("松空格，W+D")); Key(EKeys::D, false); Key(EKeys::A, true); Next(); } break;
	case 6: if (E > 0.6) { Log(TEXT("W+A 反打 0.6s")); Key(EKeys::A, false); Next(); } break;
	case 7: if (E > 0.6) { Log(TEXT("只按 W 0.6s")); Key(EKeys::D, true); Key(EKeys::SpaceBar, true); Next(); } break;
	case 8: if (E > 0.2) { Log(TEXT("再按 D+空格")); PauseGame(); Next(); } break;
	case 9: if (E > 0.5) { Log(TEXT("暂停中")); Key(EKeys::D, false); Key(EKeys::SpaceBar, false); Key(EKeys::W, false); Next(); } break;
	case 10: if (E > 0.5) { ResumeGame(); Next(); } break;
	case 11: if (E > 0.4) { Log(TEXT("暂停时松键，继续后")); Next(); } break;
	case 12: Key(EKeys::W, true); Next(); break;
	case 13: if (E > 1.5) { Log(TEXT("继续后按 W")); Key(EKeys::W, false); Key(EKeys::S, true); Next(); } break;
	case 14: if (E > 0.6) { Log(TEXT("S 刹车")); Key(EKeys::S, false); Next(); } break;
	case 15: if (E > 0.4) { Log(TEXT("全部松开")); Next(); } break;
	case 16:
		FFileHelper::SaveStringArrayToFile(Out, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Bench/input.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Next();
		FPlatformMisc::RequestExit(false);
		break;
	default: break;
	}
}

void AApexDriftPlayerController::TickVerifyCars()
{
	// 菜单里逐辆换车截图：前 3/4（带界面）、后 3/4、侧面、轮子特写
	static int32 Shot = 0;
	static double T0 = 0.0;
	static bool bSet = false;
	const double Now = FPlatformTime::Seconds();
	if (T0 == 0.0) T0 = Now;
	SApexScreen* Menu = MainMenuWidget ? MainMenuWidget->GetScreen() : nullptr;
	if (!Menu || !VehiclePawn || !SaveData) return;
	int32 Only = -1;
	FParse::Value(FCommandLine::Get(), TEXT("ApexOnlyCar="), Only);
	const int32 NShots = 6;
	const int32 CarN = Shot / NShots, View = Shot % NShots;
	const double Wait = (Shot == 0) ? 6.0 : (View == 0 ? 2.2 : 0.9);
	if (CarN >= FApexCarRegistry::Num())
	{
		if (Now - T0 > 1.2) { UE_LOG(LogTemp, Display, TEXT("[ApexCars] DONE")); FPlatformMisc::RequestExit(false); T0 = Now + 1000.0; }
		return;
	}
	if (Only >= 0 && CarN != Only) { ++Shot; T0 = Now; return; }
	if (!bSet)
	{
		bSet = true;
		if (View == 0)
		{
			if (Only < 0 && CarN > 0) Menu->DebugActivate(SApexMainMenu::IdCarNext);
			else SetVehicleCar(CarN);
			UE_LOG(LogTemp, Display, TEXT("[ApexCars] car %d -> %d"), CarN, VehiclePawn->GetCarIndex());
			int32 PaintArg = -1;
			if (FParse::Value(FCommandLine::Get(), TEXT("ApexPaint="), PaintArg)) SetVehiclePaint(PaintArg);
		}
		static const float V[6][3] = { { 142.0f, -5.3f, 790.0f }, { 38.0f, -6.0f, 700.0f }, { 90.0f, -4.0f, 780.0f }, { 152.0f, -22.0f, 620.0f }, { 118.0f, -3.0f, 420.0f }, { -38.0f, -9.0f, 640.0f } };
		VehiclePawn->DebugShowroom(V[View][0], V[View][1], V[View][2], true);
		VehiclePawn->SetHeadlightsEnabled(View == 1 || View == 3);
		VehiclePawn->SetBrakeInput(View == 5 ? 1.0f : 0.0f);
		if (MainMenuWidget) MainMenuWidget->SetVisibility(View == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Now - T0 > Wait)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("car_%d_%d.png"), CarN, View), true, false);
		++Shot;
		T0 = Now;
		bSet = false;
	}
}

void AApexDriftPlayerController::TickVerifyKeys()
{
	// 用真实按键走一遍：设置页改键（互换、取消、拒绝保留键）→ 开车验证新键生效、旧键失效 → 读盘确认存上了 → 恢复默认
	static int32 Step = 0;
	static double StepStart = 0.0;
	static TArray<FString> Out;
	static int32 Fails = 0;
	static uint64 StepFrame = 0;   // 每步至少等 10 帧：刚打完包首次启动要编着色器，帧率很低，只按秒等会在界面刷新前就按键
	const double Now = FPlatformTime::Seconds();
	if (StepStart == 0.0) StepStart = Now;
	const double E = Now - StepStart;
	if (!VehiclePawn || !SaveData) return;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	auto Shot = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Dir / Name, true, false); };
	auto Key = [](const FKey& K, bool bDown)
	{
		const FKeyEvent Ev(K, FModifierKeysState(), 0, false, 0, 0);
		if (bDown) FSlateApplication::Get().ProcessKeyDownEvent(Ev); else FSlateApplication::Get().ProcessKeyUpEvent(Ev);
	};
	auto Tap = [&](const FKey& K) { Key(K, true); Key(K, false); };
	auto Check = [&](bool bOk, const FString& What)
	{
		if (!bOk) ++Fails;
		Out.Add(FString::Printf(TEXT("%s  %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What));
	};
	auto Binds = [&]()
	{
		return FString::Printf(TEXT("油门=%s 刹车=%s 手刹=%s 视角=%s"), *ApexKeys::Name(EApexBind::Throttle), *ApexKeys::Name(EApexBind::Brake), *ApexKeys::Name(EApexBind::Handbrake), *ApexKeys::Name(EApexBind::Camera));
	};
	auto Next = [&]() { ++Step; StepStart = Now; StepFrame = GFrameCounter; };
	auto Ok = [&](double Secs) { return E > Secs && GFrameCounter - StepFrame >= 10; };
	SApexSettings* Set = SettingsWidget ? static_cast<SApexSettings*>(SettingsWidget->GetScreen()) : nullptr;
	const FApexDriftTelemetry& T = VehiclePawn->GetTelemetry();
	switch (Step)
	{
	case 0:
		if (Ok(5.0))
		{
			ApexKeys::ResetAll(SaveData);
			ApplyKeyBindings();
			OpenSettingsMenu();
			Next();
		}
		break;
	case 1: if (Ok(1.0) && Set) { Set->DebugSetTab(3); Next(); } break;
	case 2: if (Ok(1.0)) { Shot(TEXT("keys_01_page.png")); Set->DebugActivate(SApexSettings::IdRow0 + static_cast<int32>(EApexBind::Throttle)); Next(); } break;
	case 3: if (Ok(0.8)) { Shot(TEXT("keys_02_capture.png")); Next(); } break;
	case 4: if (Ok(0.6)) { Tap(EKeys::I); Next(); } break;
	case 5:
		if (Ok(0.6))
		{
			Check(ApexKeys::Get(EApexBind::Throttle) == EKeys::I, TEXT("设置页按 I：油门改成 I  ") + Binds());
			Shot(TEXT("keys_03_bound.png"));
			Set->DebugActivate(SApexSettings::IdRow0 + static_cast<int32>(EApexBind::Brake));
			Next();
		}
		break;
	case 6: if (Ok(0.5)) { Tap(EKeys::I); Next(); } break;   // I 已是油门：应互换
	case 7:
		if (Ok(0.6))
		{
			Check(ApexKeys::Get(EApexBind::Brake) == EKeys::I && ApexKeys::Get(EApexBind::Throttle) == EKeys::S, TEXT("刹车也按 I：两者互换（油门=S）  ") + Binds());
			Shot(TEXT("keys_04_swap.png"));
			Set->DebugActivate(SApexSettings::IdRow0 + static_cast<int32>(EApexBind::Handbrake));
			Next();
		}
		break;
	case 8: if (Ok(0.5)) { Tap(EKeys::Escape); Next(); } break;
	case 9:
		if (Ok(0.5))
		{
			Check(ApexKeys::Get(EApexBind::Handbrake) == EKeys::SpaceBar && SettingsWidget->IsVisible(), TEXT("改手刹时按 Esc：取消、设置页没关  ") + Binds());
			Set->DebugActivate(SApexSettings::IdRow0 + static_cast<int32>(EApexBind::Camera));
			Next();
		}
		break;
	case 10: if (Ok(0.5)) { Tap(EKeys::P); Tap(EKeys::Up); Next(); } break;   // 保留键：应拒绝并继续等
	case 11: if (Ok(0.5)) { Shot(TEXT("keys_05_reserved.png")); Tap(EKeys::LeftShift); Next(); } break;
	case 12:
		if (Ok(0.6))
		{
			Check(ApexKeys::Get(EApexBind::Camera) == EKeys::LeftShift, TEXT("视角先按 P、↑（拒绝）再按左 Shift：视角=左 Shift  ") + Binds());
			Shot(TEXT("keys_06_after.png"));
			CloseSettingsMenu();
			Next();
		}
		break;
	case 13: if (Ok(0.8)) { StartRaceSession(EApexGameModeType::FreeDrift); Next(); } break;
	case 14: if (CurrentGameState == EApexGameState::Playing && Ok(0.5)) { Key(EKeys::S, true); Next(); } break;   // S 现在是油门
	case 15:
		if (Ok(2.0))
		{
			Check(T.ThrottleInput > 0.5f && T.SpeedKmh > 15.0f, FString::Printf(TEXT("按住 S 2 秒：油门 %.2f，车速 %.0f"), T.ThrottleInput, T.SpeedKmh));
			Shot(TEXT("keys_07_hud.png"));
			Key(EKeys::S, false);
			Key(EKeys::W, true);   // W 已经没有绑定
			Next();
		}
		break;
	case 16:
		if (Ok(1.0))
		{
			Check(T.ThrottleInput < 0.05f, FString::Printf(TEXT("按住 W（已解绑）1 秒：油门 %.2f"), T.ThrottleInput));
			Key(EKeys::W, false);
			Key(EKeys::I, true);
			Next();
		}
		break;
	case 17:
		if (Ok(1.0))
		{
			Check(T.BrakeInput > 0.5f, FString::Printf(TEXT("按住 I 1 秒：刹车 %.2f"), T.BrakeInput));
			Key(EKeys::I, false);
			Next();
		}
		break;
	case 18:
		if (Ok(0.5))
		{
			const UApexDriftSaveGame* Disk = Cast<UApexDriftSaveGame>(UGameplayStatics::LoadGameFromSlot(UApexDriftSaveGame::DefaultSlotName, 0));
			const FKey* Thr = Disk ? Disk->KeyBinds.Find(TEXT("Throttle")) : nullptr;
			const FKey* Cam = Disk ? Disk->KeyBinds.Find(TEXT("Camera")) : nullptr;
			Check(Thr && *Thr == EKeys::S && Cam && *Cam == EKeys::LeftShift && !Disk->KeyBinds.Contains(TEXT("Handbrake")),
				FString::Printf(TEXT("读盘：存了 %d 项改键（油门=S、视角=左 Shift，手刹没改不存）"), Disk ? Disk->KeyBinds.Num() : -1));
			ApexKeys::ResetAll(SaveData);
			ApplyKeyBindings();
			Key(EKeys::W, true);
			Next();
		}
		break;
	case 19:
		if (Ok(1.0))
		{
			Check(T.ThrottleInput > 0.5f && ApexKeys::IsAllDefault(), FString::Printf(TEXT("恢复默认后按住 W：油门 %.2f  "), T.ThrottleInput) + Binds());
			Key(EKeys::W, false);
			ReturnToMainMenu();
			Out.Insert(FString::Printf(TEXT("改键验证：%s（失败 %d 项）"), Fails == 0 ? TEXT("全部通过") : TEXT("有失败"), Fails), 0);
			FFileHelper::SaveStringArrayToFile(Out, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Bench/verify_keys.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			Next();
		}
		break;
	case 20: if (Ok(1.0)) { Next(); FPlatformMisc::RequestExit(false); } break;
	default: break;
	}
}

void AApexDriftPlayerController::TickVerifyPick()
{
	// 跨关卡保留进度（换图后是新的控制器，静态变量还在）
	static int32 Step = 0;
	static double StepStart = 0.0;
	static TArray<FString> Log;
	const double Now = FPlatformTime::Seconds();
	if (StepStart == 0.0) StepStart = Now;
	const double E = Now - StepStart;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	auto Next = [&]() { ++Step; StepStart = Now; };
	auto Done = [&](bool bOk)
	{
		Log.Add(bOk ? TEXT("结果：通过") : TEXT("结果：失败"));
		FFileHelper::SaveStringArrayToFile(Log, *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Bench/verify_pick.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		FPlatformMisc::RequestExit(false);
	};
	const int32 NMap = FApexMapRegistry::NumMaps();
	int32 Train = -1;
	for (int32 i = 0; i < NMap; ++i) if (FApexMapRegistry::GetMaps()[i].bFreeRoam) Train = i;
	SApexScreen* Screen = MainMenuWidget ? MainMenuWidget->GetScreen() : nullptr;
	SApexMainMenu* Menu = static_cast<SApexMainMenu*>(Screen);
	if (Step <= NMap)
	{
		if (!Menu) return;
		if (Step == 0)
		{
			if (E > 4.0) { Log.Add(FString::Printf(TEXT("起始地图 %s，共 %d 张，训练场是第 %d 张"), *GetWorld()->GetMapName(), NMap, Train + 1)); Next(); }
			return;
		}
		// 逐张点卡片（走和鼠标点击 / 回车同一条 OnActivate），看选中的是不是这张
		const int32 i = Step - 1;
		if (E > 0.8)
		{
			Menu->DebugActivate(SApexMainMenu::IdMap0 + i);
			const bool bOk = Menu->DebugSelectedMap() == i;
			Log.Add(FString::Printf(TEXT("点第 %d 张（%s）：%s"), i + 1, *FApexMapRegistry::GetMaps()[i].NameZh, bOk ? TEXT("选中") : TEXT("没反应")));
			FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("pick_%d.png"), i + 1), true, false);
			if (!bOk) { Done(false); return; }
			Next();
		}
		return;
	}
	if (Step == NMap + 1)
	{
		if (!Menu || E < 1.0) return;
		Menu->DebugActivate(SApexMainMenu::IdMap0 + Train);
		Menu->DebugActivate(SApexMainMenu::IdStart);
		Log.Add(TEXT("选训练场后点“开始比赛”"));
		Next();
		return;
	}
	// 等加载进训练场
	const bool bInTrain = GetWorld()->GetMapName().Contains(TEXT("TrainingPark")) && FApexMapRegistry::ClampIndex(CurrentMapIndex) == Train;
	if (bInTrain && E > 4.0)
	{
		Log.Add(FString::Printf(TEXT("已进入 %s，状态 %d"), *GetWorld()->GetMapName(), (int32)CurrentGameState));
		FScreenshotRequest::RequestScreenshot(Dir / TEXT("pick_train_loaded.png"), true, false);
		Next();
		if (Step > NMap + 3) Done(true);
		return;
	}
	if (Step == NMap + 3 && E > 1.0) { Done(true); return; }
	if (E > 45.0) { Log.Add(FString::Printf(TEXT("45 秒内没进训练场，当前 %s"), *GetWorld()->GetMapName())); Done(false); }
}

void AApexDriftPlayerController::TickVerifyFlow()
{
	// 跨关卡保留进度
	static int32 Step = 0;
	static double StepStart = 0.0;
	const double Now = FPlatformTime::Seconds();
	if (StepStart == 0.0) StepStart = Now;
	const double E = Now - StepStart;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	auto Shot = [&](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Dir / Name, true, false); };
	auto Next = [&]()
	{
		++Step;
		StepStart = Now;
		UE_LOG(LogTemp, Display, TEXT("[ApexFlow] step %d map=%s state=%d"), Step, *GetWorld()->GetMapName(), (int32)CurrentGameState);
	};
	SApexScreen* Menu = MainMenuWidget ? MainMenuWidget->GetScreen() : nullptr;
	SApexScreen* Pause = PauseMenuWidget ? PauseMenuWidget->GetScreen() : nullptr;
	if (!Menu) return;
	switch (Step)
	{
	case 0: if (E > 4.0) { Shot(TEXT("flow_01_menu.png")); Next(); } break;
	case 1: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdPaint0 + 2); Menu->DebugActivate(SApexMainMenu::IdMode0 + 1); Menu->DebugActivate(SApexMainMenu::IdWeather0 + 1); Next(); } break;
	case 2: if (E > 1.6) { Shot(TEXT("flow_02_menu_paint_mode_weather.png")); Next(); } break;
	case 3: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdNavControls); Next(); } break;
	case 4: if (E > 1.0) { Shot(TEXT("flow_03_controls.png")); Next(); } break;
	case 5: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdCloseControls); Next(); } break;
	case 6: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdNavSettings); Next(); } break;
	case 7: if (E > 1.0) { Shot(TEXT("flow_04_settings.png")); Next(); } break;
	case 8: if (E > 0.6) { CloseSettingsMenu(); Next(); } break;
	case 9: if (E > 0.8) { Menu->DebugActivate(SApexMainMenu::IdMap0 + 1); Next(); } break;
	case 10: if (E > 1.4) { Shot(TEXT("flow_05_select_map2.png")); Next(); } break;
	case 11: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdPreview); Next(); } break;   // 跨图预览
	case 12: if (E > 4.0 && GetWorld()->GetMapName().Contains(TEXT("Momiji"))) { Shot(TEXT("flow_06_preview_map2.png")); Next(); }
		else if (E > 40.0) { UE_LOG(LogTemp, Error, TEXT("[ApexFlow] preview travel failed")); Next(); } break;
	case 13: if (E > 0.8) { Menu->DebugActivate(SApexMainMenu::IdMap0 + 2); Next(); } break;
	case 14: if (E > 1.0) { Menu->DebugActivate(SApexMainMenu::IdMode0 + 1); Menu->DebugActivate(SApexMainMenu::IdStart); Next(); } break;   // 跨图直接开赛
	case 15: if (GetWorld()->GetMapName().Contains(TEXT("Kuroshio")) && CurrentGameState == EApexGameState::Playing && E > 2.0) { Shot(TEXT("flow_07_race_map3.png")); Next(); }
		else if (E > 60.0) { UE_LOG(LogTemp, Error, TEXT("[ApexFlow] race travel failed")); Next(); } break;
	case 16:
		if (VehiclePawn)
		{
			const double P = FMath::Fmod(E, 6.0);
			VehiclePawn->SetThrottleInput(1.0f);
			VehiclePawn->SetHandbrakeInput(E > 4.0 && E < 4.4);
			VehiclePawn->SetSteeringInput(E > 4.0 ? (P < 5.5 ? 0.6f : 0.0f) : 0.0f);
		}
		if (E > 6.2) { Shot(TEXT("flow_08_drift_hud.png")); Next(); }
		break;
	case 17: if (E > 0.6) { PauseGame(); Next(); } break;
	case 18: if (E > 1.0) { Shot(TEXT("flow_09_pause.png")); Next(); } break;
	case 19: if (E > 0.6) { ResumeGame(); Next(); } break;
	case 20: if (E > 0.6) { FinishRaceSession(); Next(); } break;
	case 21: if (E > 2.5) { Shot(TEXT("flow_10_finish.png")); Next(); } break;
	case 22: if (E > 0.6) { ReturnToMainMenu(); Next(); } break;
	case 23: if (E > 2.5) { Shot(TEXT("flow_11_back_to_menu.png")); Next(); } break;
	case 24: if (E > 1.0) { Menu->DebugActivate(SApexMainMenu::IdMap0 + 0); Next(); } break;
	case 25: if (E > 0.6) { Menu->DebugActivate(SApexMainMenu::IdAuto); Next(); } break;   // 跨图自动漂移
	case 26: if (VehiclePawn && VehiclePawn->IsAutoRunning() && VehiclePawn->GetAutoRun().GetElapsed() > 9.0f) { Shot(TEXT("flow_12_auto.png")); Next(); }
		else if (E > 60.0) { UE_LOG(LogTemp, Error, TEXT("[ApexFlow] auto travel failed")); Next(); } break;
	case 27: if (E > 1.0) { ReturnToMainMenu(); Next(); } break;
	case 28: if (E > 1.5) { UE_LOG(LogTemp, Display, TEXT("[ApexFlow] DONE")); Next(); FPlatformMisc::RequestExit(false); } break;
	default: break;
	}
}

void AApexDriftPlayerController::TickVerifyGameplay(float DeltaSeconds)
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - VerifyPhaseStartTime;
	VerifyPhaseSimTime += static_cast<double>(DeltaSeconds);
	const FString ShotDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots"));
	IFileManager::Get().MakeDirectory(*ShotDir, true);

	auto EnterPhase = [&](int32 NextPhase)
	{
		VerifyPhase = NextPhase;
		VerifyPhaseStartTime = Now;
		VerifyPhaseSimTime = 0.0;
		VerifyLastLogSimTime = -1.0;
	};

	if (VehiclePawn)
	{
		const FApexDriftTelemetry& Tel = VehiclePawn->GetTelemetry();
		VerifyMaxSpeedKmh = FMath::Max(VerifyMaxSpeedKmh, Tel.SpeedKmh);
		VerifyMaxGear = FMath::Max(VerifyMaxGear, Tel.Gear);
		VerifyPeakSlipAngleDeg = FMath::Max(VerifyPeakSlipAngleDeg, FMath::Abs(Tel.SlipAngleDeg));
		VerifyPeakCurrentDriftScore = FMath::Max(VerifyPeakCurrentDriftScore, Tel.CurrentDriftScore);
		VerifyBankedTotalScore = FMath::Max(VerifyBankedTotalScore, Tel.TotalScore);
		VerifyMaxCombo = FMath::Max(VerifyMaxCombo, Tel.MaxDriftCombo);
		if (VehiclePawn->GetSkidmarkManager())
		{
			VerifySkidSegments = FMath::Max(VerifySkidSegments, VehiclePawn->GetSkidmarkManager()->GetTotalSkidSegmentCount());
		}
		if (VehiclePawn->DriftVFXComp)
		{
			VerifySmokeEmitted = FMath::Max(VerifySmokeEmitted, VehiclePawn->DriftVFXComp->GetTotalEmittedSmokeCount());
		}
		if (VerifyPhase >= 6 && VerifyPhase <= 9 && (VerifyLastLogSimTime < 0.0 || VerifyPhaseSimTime - VerifyLastLogSimTime >= 0.5))
		{
			VerifyLastLogSimTime = VerifyPhaseSimTime;
			UE_LOG(LogTemp, Display, TEXT("[ApexVerifyGameplay] P%d t=%.2f Speed=%.1f Gear=%d RPM=%.0f Slip=%.1f Score=%d/%d"),
				VerifyPhase, VerifyPhaseSimTime, Tel.SpeedKmh, Tel.Gear, Tel.EngineRPM, Tel.SlipAngleDeg, Tel.CurrentDriftScore, Tel.TotalScore);
		}
	}

	switch (VerifyPhase)
	{
	case 1: // Wait briefly on MainMenu showroom orbit, then screenshot Main Menu UI
		if (Elapsed >= 1.6)
		{
			FScreenshotRequest::RequestScreenshot(ShotDir / TEXT("07_LiveUI_MainMenu.png"), true, false);
			EnterPhase(2);
		}
		break;

	case 2: // Allow screenshot flush, then open Settings and test Paint + Weather + SaveGame persistence
		if (Elapsed >= 0.65)
		{
			OpenSettingsMenu();
			if (SaveData && VehiclePawn)
			{
				const int32 OrigPaint = SaveData->PaintPresetIndex;
				const int32 OrigWeather = GetCurrentWeatherIndex();
				SetVehiclePaint(OrigPaint + 1);
				SetWeatherIndex((OrigWeather + 1) % 3);
				ApplyAndSaveSettings(false);
				UApexDriftSaveGame* Reloaded = UApexDriftSaveGame::LoadOrCreate();
				const FName MapId = FApexMapRegistry::GetMaps()[CurrentMapIndex].Id;
				bVerifySaveGameRoundTrip = (Reloaded != nullptr &&
					Reloaded->PaintPresetIndex == SaveData->PaintPresetIndex &&
					Reloaded->GetWeatherForMap(MapId) == SaveData->GetWeatherForMap(MapId));
				SetVehiclePaint(OrigPaint);
				SetWeatherIndex(OrigWeather);
			}
			EnterPhase(3);
		}
		break;

	case 3: // Capture Settings overlay screenshot
		if (Elapsed >= 0.75)
		{
			FScreenshotRequest::RequestScreenshot(ShotDir / TEXT("08_LiveUI_Settings.png"), true, false);
			EnterPhase(4);
		}
		break;

	case 4: // Close Settings and launch DriftAttack3Min race session
		if (Elapsed >= 0.65)
		{
			CloseSettingsMenu();
			StartRaceSession(EApexGameModeType::DriftAttack3Min);
			EnterPhase(5);
		}
		break;

	case 5: // Wait for 3-2-1-GO countdown to finish
		if (CurrentGameState == EApexGameState::Playing || Elapsed > 8.0)
		{
			EnterPhase(6);
		}
		break;

	case 6: // Full throttle straight launch until target entry speed (~92 km/h) or timeout
		if (VehiclePawn)
		{
			VehiclePawn->SetThrottleInput(1.0f);
			VehiclePawn->SetBrakeInput(0.0f);
			VehiclePawn->SetSteeringInput(0.0f);
			VehiclePawn->SetHandbrakeInput(false);
		}
		if ((VehiclePawn && VehiclePawn->GetTelemetry().SpeedKmh >= 92.0f) || VerifyPhaseSimTime >= 4.4)
		{
			EnterPhase(7);
		}
		break;

	case 7: // Initiate crisp high-speed handbrake drift entry
		if (VehiclePawn)
		{
			VehiclePawn->SetThrottleInput(0.85f);
			VehiclePawn->SetBrakeInput(0.0f);
			VehiclePawn->SetSteeringInput(0.68f);
			VehiclePawn->SetHandbrakeInput(true);
		}
		if (VerifyPhaseSimTime >= 0.35 || (VehiclePawn && FMath::Abs(VehiclePawn->GetTelemetry().SlipAngleDeg) >= 28.0f))
		{
			VerifySustainDriftShotTaken = 0.0;
			EnterPhase(8);
		}
		break;

	case 8: // Sustain drift on power with signed countersteer & capture live HUD + smoke + skidmarks
		if (VehiclePawn)
		{
			const float Slip = VehiclePawn->GetTelemetry().SlipAngleDeg;
			const float SteerCmd = FMath::Clamp((Slip + 28.0f) * 0.032f, -0.75f, 0.45f);
			VehiclePawn->SetHandbrakeInput(false);
			VehiclePawn->SetBrakeInput(0.0f);
			VehiclePawn->SetThrottleInput(0.92f);
			VehiclePawn->SetSteeringInput(SteerCmd);

			if (FParse::Param(FCommandLine::Get(), TEXT("ApexVerifyLookBack"))) VehiclePawn->SetLookBack(VerifyPhaseSimTime > 0.25);
			if (VerifySustainDriftShotTaken < 0.5 && VerifyPhaseSimTime >= 0.95)
			{
				VerifySustainDriftShotTaken = 1.0;
				FScreenshotRequest::RequestScreenshot(ShotDir / TEXT("09_LiveGameplay_DriftHUD.png"), true, false);
			}
		}
		if (VerifyPhaseSimTime >= 2.6)
		{
			EnterPhase(9);
		}
		break;

	case 9: // Straighten smoothly so drift score transition banks into TotalScore
		if (VehiclePawn)
		{
			const float Slip = VehiclePawn->GetTelemetry().SlipAngleDeg;
			VehiclePawn->SetHandbrakeInput(false);
			VehiclePawn->SetBrakeInput(0.0f);
			VehiclePawn->SetThrottleInput(0.25f);
			VehiclePawn->SetSteeringInput(FMath::Clamp(Slip * 0.02f, -0.4f, 0.4f));
		}
		if (VerifyPhaseSimTime >= 2.2)
		{
			PauseGame();
			EnterPhase(10);
		}
		break;

	case 10: // Capture Pause Menu UI overlay screenshot
		if (Elapsed >= 0.65)
		{
			FScreenshotRequest::RequestScreenshot(ShotDir / TEXT("10_LiveUI_PauseMenu.png"), true, false);
			EnterPhase(11);
		}
		break;

	case 11: // Resume, reset vehicle cleanly back to track, and verify reset state
		if (Elapsed >= 0.65)
		{
			ResumeGame();
			if (VehiclePawn)
			{
				VehiclePawn->SetThrottleInput(0.0f);
				VehiclePawn->SetSteeringInput(0.0f);
				VehiclePawn->SetHandbrakeInput(false);
				ResetVehicleToTrack();
			}
			EnterPhase(12);
		}
		break;

	case 12:
		if (Elapsed >= 0.35)
		{
			if (VehiclePawn)
			{
				const float PostSpeed = VehiclePawn->GetTelemetry().SpeedKmh;
				float DistAlong = 0.0f, LatOffset = 9999.0f;
				FTransform Xf;
				if (TrackActor)
				{
					TrackActor->FindClosestTrackTransform(VehiclePawn->GetActorLocation(), Xf, DistAlong, LatOffset);
				}
				const bool bUpright = (VehiclePawn->GetActorUpVector().Z > 0.90f);
				bVerifyResetSucceeded = (PostSpeed < 12.0f && FMath::Abs(LatOffset) < 300.0f && bUpright);
			}
			WriteVerifyReportAndExit();
			VerifyPhase = 13;
		}
		break;

	default:
		break;
	}
}

void AApexDriftPlayerController::WriteVerifyReportAndExit()
{
	const FString ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VerificationShots/functional_verify.json"));
	const FString Json = FString::Printf(
		TEXT("{\n")
		TEXT("  \"maxSpeedKmh\": %.2f,\n")
		TEXT("  \"maxGear\": %d,\n")
		TEXT("  \"peakSlipAngleDeg\": %.2f,\n")
		TEXT("  \"peakCurrentDriftScore\": %d,\n")
		TEXT("  \"bankedTotalScore\": %d,\n")
		TEXT("  \"maxCombo\": %.2f,\n")
		TEXT("  \"bResetSucceeded\": %s,\n")
		TEXT("  \"bSaveGameRoundTrip\": %s,\n")
		TEXT("  \"bSkidmarksGenerated\": %s,\n")
		TEXT("  \"skidSegments\": %d,\n")
		TEXT("  \"bSmokeEmitted\": %s,\n")
		TEXT("  \"smokeParticlesEmitted\": %d\n")
		TEXT("}\n"),
		VerifyMaxSpeedKmh,
		VerifyMaxGear,
		VerifyPeakSlipAngleDeg,
		VerifyPeakCurrentDriftScore,
		VerifyBankedTotalScore,
		VerifyMaxCombo,
		bVerifyResetSucceeded ? TEXT("true") : TEXT("false"),
		bVerifySaveGameRoundTrip ? TEXT("true") : TEXT("false"),
		(VerifySkidSegments > 0) ? TEXT("true") : TEXT("false"),
		VerifySkidSegments,
		(VerifySmokeEmitted > 0) ? TEXT("true") : TEXT("false"),
		VerifySmokeEmitted
	);
	FFileHelper::SaveStringToFile(Json, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("[ApexVerifyGameplay] Functional verify report saved to %s:\n%s"), *ReportPath, *Json);
	FPlatformMisc::RequestExit(false);
}

// =========================================================================
// Input Handlers
// =========================================================================

void AApexDriftPlayerController::OnShiftUp(const FInputActionValue&)
{
	if (VehiclePawn) VehiclePawn->ShiftUp();
}

void AApexDriftPlayerController::OnShiftDown(const FInputActionValue&)
{
	if (VehiclePawn) VehiclePawn->ShiftDown();
}

void AApexDriftPlayerController::OnCycleCamera(const FInputActionValue&)
{
	if (VehiclePawn && CurrentGameState == EApexGameState::Playing) VehiclePawn->CycleCameraMode();
}

void AApexDriftPlayerController::OnLookBackStart(const FInputActionValue&)
{
	if (VehiclePawn && CurrentGameState == EApexGameState::Playing) VehiclePawn->SetLookBack(true);
}

void AApexDriftPlayerController::OnLookBackStop(const FInputActionValue&)
{
	if (VehiclePawn) VehiclePawn->SetLookBack(false);
}

void AApexDriftPlayerController::OnResetCar(const FInputActionValue&)
{
	if (!VehiclePawn || CurrentGameState != EApexGameState::Playing) return;
	const double Now = FPlatformTime::Seconds();
	if (IsFreeRoamMap() && Now - LastResetPress < 1.2)
	{
		// 训练场连按两次：回起点并复原所有道具
		LastResetPress = -10.0;
		VehiclePawn->ResetToTrack();
		PlaceVehicleAtStartGrid();
		return;
	}
	LastResetPress = Now;
	VehiclePawn->ResetToTrack();
}

bool AApexDriftPlayerController::IsFreeRoamMap() const
{
	return FApexMapRegistry::GetMaps()[FApexMapRegistry::ClampIndex(CurrentMapIndex)].bFreeRoam;
}

void AApexDriftPlayerController::OnToggleLights(const FInputActionValue&)
{
	if (VehiclePawn) VehiclePawn->ToggleHeadlights();
}

void AApexDriftPlayerController::OnTogglePause(const FInputActionValue&)
{
	if (bInSettingsOverlay)
	{
		CloseSettingsMenu();
		return;
	}
	if (CurrentGameState == EApexGameState::Playing || CurrentGameState == EApexGameState::Countdown)
	{
		PauseGame();
	}
	else if (CurrentGameState == EApexGameState::Paused)
	{
		ResumeGame();
	}
}

void AApexDriftPlayerController::OnLookAround(const FInputActionValue& Value)
{
	if (!VehiclePawn) return;
	const FVector2D Delta = Value.Get<FVector2D>();
	if (CurrentGameState == EApexGameState::MainMenu && !bInSettingsOverlay)
	{
		if (IsInputKeyDown(EKeys::LeftMouseButton) || IsInputKeyDown(EKeys::RightMouseButton))
		{
			VehiclePawn->AddShowroomOrLookDelta(Delta.X, Delta.Y);
		}
	}
	else if (CurrentGameState == EApexGameState::Playing)
	{
		VehiclePawn->AddShowroomOrLookDelta(Delta.X, Delta.Y);
	}
}
