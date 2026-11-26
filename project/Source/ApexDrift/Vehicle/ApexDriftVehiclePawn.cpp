#include "Vehicle/ApexDriftVehiclePawn.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Vehicle/ApexCarRegistry.h"
#include "Core/ApexDriftSaveGame.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Vehicle/ApexDriftWheelFront.h"
#include "Vehicle/ApexDriftWheelRear.h"
#include "Vehicle/ApexDriftVFXComponent.h"
#include "Gameplay/ApexDriftScoreComponent.h"
#include "Audio/ApexVehicleAudioComponent.h"
#include "Track/ApexTrackSplineActor.h"
#include "Track/ApexSkidmarkManager.h"
#include "Track/ApexWeatherManager.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Core/ApexPaintRegistry.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace ApexCar
{
	// 物理底盘的轮位（厘米），所有车共用；可视轮位、灯、排气口、相机位在 FApexCarRegistry
	static const FVector WheelRest[4] = {
		FVector( 145.75f, -81.77f, 32.09f),
		FVector( 145.75f,  81.77f, 32.09f),
		FVector(-145.75f, -90.66f, 34.74f),
		FVector(-145.75f,  90.66f, 34.74f),
	};
	static constexpr float PhysRadiusF = 32.1f;
	static constexpr float PhysRadiusR = 34.7f;
	static constexpr float WheelBaseM = 2.915f;
	static constexpr float MaxSteerDeg = 46.0f;
	static constexpr float BaseTorque = 860.0f;
	static const FName WheelBones[4] = { FName("Phys_Wheel_FL"), FName("Phys_Wheel_FR"), FName("Phys_Wheel_BL"), FName("Phys_Wheel_BR") };
}

AApexDriftVehiclePawn::AApexDriftVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	USkeletalMeshComponent* SkelMesh = GetMesh();
	SkelMesh->SetSimulatePhysics(true);
	SkelMesh->SetCollisionProfileName(FName("Vehicle"));
	SkelMesh->SetNotifyRigidBodyCollision(true);
	// Hide dummy physics proxy mesh across main/capture/RT passes while keeping wheel bones ticking continuously
	SkelMesh->SetVisibility(false, false);
	SkelMesh->SetHiddenInGame(true, false);
	SkelMesh->SetRenderInMainPass(false);
	SkelMesh->SetRenderInDepthPass(false);
	SkelMesh->SetCastShadow(false);
	SkelMesh->bVisibleInRayTracing = false;
	SkelMesh->bVisibleInRealTimeSkyCaptures = false;
	SkelMesh->bVisibleInReflectionCaptures = false;
	SkelMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SkmFinder(TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar"));
	if (SkmFinder.Succeeded())
	{
		SkelMesh->SetSkeletalMesh(SkmFinder.Object);
	}
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Vehicles/SportsCar/ABP_SportsCar"));
	if (AnimFinder.Succeeded())
	{
		SkelMesh->SetAnimInstanceClass(AnimFinder.Class);
	}

	auto MakePart = [this](const TCHAR* Name, USceneComponent* Parent) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->bReceivesDecals = false;
		return C;
	};

	ChassisMesh = MakePart(TEXT("ChassisMesh"), SkelMesh);
	// 物理静止时轮子相对车身下垂约 4.6cm，把可视车身压低一点，轮眉间隙更自然
	ChassisMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -2.2f));
	ChassisGlassMesh = MakePart(TEXT("ChassisGlassMesh"), ChassisMesh);
	ChassisGlassMesh->SetCastShadow(false);
	LightsMesh = MakePart(TEXT("LightsMesh"), ChassisMesh);
	LightsMesh->SetCastShadow(false);
	GTWingMesh = MakePart(TEXT("GTWingMesh"), ChassisMesh);

	for (int32 i = 0; i < 4; ++i)
	{
		USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("WheelPivot_%d"), i));
		Pivot->SetupAttachment(ChassisMesh);
		Pivot->SetRelativeLocation(ApexCar::WheelRest[i]);
		WheelPivots.Add(Pivot);
		WheelMeshes.Add(MakePart(*FString::Printf(TEXT("WheelMesh_%d"), i), Pivot));
		CaliperMeshes.Add(MakePart(*FString::Printf(TEXT("CaliperMesh_%d"), i), Pivot));
	}

	auto MakeHead = [this](const TCHAR* Name, float Side) -> USpotLightComponent*
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(Name);
		L->SetupAttachment(ChassisMesh);
		L->SetRelativeRotation(FRotator(-2.6f, Side * 2.5f, 0.0f));
		// 不用平方反比：近处不过曝，光带能铺到 50 米外
		L->bUseInverseSquaredFalloff = false;
		L->LightFalloffExponent = 1.6f;
		L->SetIntensity(5.0f);
		L->SetLightColor(FColor(246, 250, 255));
		L->SetAttenuationRadius(6500.0f);
		L->SetInnerConeAngle(5.0f);
		L->SetOuterConeAngle(30.0f);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		return L;
	};
	HeadlightL = MakeHead(TEXT("HeadlightL"), -1.0f);
	HeadlightR = MakeHead(TEXT("HeadlightR"), 1.0f);

	auto MakeTail = [this](const TCHAR* Name, float Side) -> UPointLightComponent*
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(ChassisMesh);
		L->SetIntensity(30.0f);
		L->SetLightColor(FColor(255, 22, 18));
		L->SetAttenuationRadius(220.0f);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		return L;
	};
	TaillightL = MakeTail(TEXT("TaillightL"), -1.0f);
	TaillightR = MakeTail(TEXT("TaillightR"), 1.0f);

	ChaseSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseSpringArm"));
	ChaseSpringArm->SetupAttachment(SkelMesh);
	ChaseSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	ChaseSpringArm->TargetArmLength = 560.0f;
	ChaseSpringArm->SocketOffset = FVector(0.0f, 0.0f, 110.0f);
	ChaseSpringArm->bDoCollisionTest = true;
	ChaseSpringArm->SetUsingAbsoluteRotation(true);
	ChaseSpringArm->ProbeSize = 14.0f;
	ChaseSpringArm->bEnableCameraLag = true;
	ChaseSpringArm->CameraLagSpeed = 18.0f;
	ChaseSpringArm->CameraLagMaxDistance = 70.0f;
	ChaseSpringArm->bEnableCameraRotationLag = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseSpringArm);
	ChaseCamera->FieldOfView = 82.0f;
	ChaseCamera->bAutoActivate = false;

	HoodCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HoodCamera"));
	HoodCamera->SetupAttachment(ChassisMesh);
	HoodCamera->SetRelativeLocation(FVector(128.0f, 0.0f, 98.0f));
	HoodCamera->SetRelativeRotation(FRotator(-4.5f, 0.0f, 0.0f));
	HoodCamera->FieldOfView = 88.0f;
	HoodCamera->bAutoActivate = false;

	BumperCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BumperCamera"));
	BumperCamera->SetupAttachment(ChassisMesh);
	BumperCamera->SetRelativeLocation(FVector(244.0f, 0.0f, 44.0f));
	BumperCamera->SetRelativeRotation(FRotator(-1.5f, 0.0f, 0.0f));
	BumperCamera->FieldOfView = 92.0f;
	BumperCamera->bAutoActivate = false;

	ShowroomSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ShowroomSpringArm"));
	ShowroomSpringArm->SetupAttachment(SkelMesh);
	// 低机位、地平线压在车顶附近：上方三分之一留给风景，车身在底部停靠栏之上、标题右侧
	ShowroomSpringArm->SetRelativeLocation(FVector(10.0f, 0.0f, 24.0f));
	ShowroomSpringArm->SetRelativeRotation(FRotator(ShowroomPitch, ShowroomYaw, 0.0f));
	ShowroomSpringArm->TargetArmLength = 790.0f;
	ShowroomSpringArm->SocketOffset = FVector(0.0f, -62.0f, 0.0f);
	ShowroomSpringArm->bDoCollisionTest = false;
	ShowroomSpringArm->bInheritPitch = false;
	ShowroomSpringArm->bInheritRoll = false;

	ShowroomCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ShowroomCamera"));
	ShowroomCamera->SetupAttachment(ShowroomSpringArm);
	ShowroomCamera->FieldOfView = 58.0f;
	ShowroomCamera->bAutoActivate = true;

	CineCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CineCamera"));
	CineCamera->SetupAttachment(SkelMesh);
	CineCamera->SetUsingAbsoluteLocation(true);
	CineCamera->SetUsingAbsoluteRotation(true);
	CineCamera->bAutoActivate = false;

	DriftScoreComp = CreateDefaultSubobject<UApexDriftScoreComponent>(TEXT("DriftScoreComp"));
	VehicleAudioComp = CreateDefaultSubobject<UApexVehicleAudioComponent>(TEXT("VehicleAudioComp"));
	DriftVFXComp = CreateDefaultSubobject<UApexDriftVFXComponent>(TEXT("DriftVFXComp"));

	SetupChassisPhysicsAndEngine();
}

void AApexDriftVehiclePawn::SetupChassisPhysicsAndEngine()
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Move)
	{
		return;
	}

	Move->Mass = 1380.0f;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(-4.0f, 0.0f, -22.0f);
	Move->ChassisWidth = 195.0f;
	Move->ChassisHeight = 125.0f;
	Move->DragCoefficient = 0.30f;
	Move->DownforceCoefficient = 0.45f;
	Move->InertiaTensorScale = FVector(1.0f, 1.0f, 1.0f);
	Move->bReverseAsBrake = true;
	Move->bThrottleAsBrake = true;
	Move->bLegacyWheelFrictionPosition = true;
	Move->TorqueControl.Enabled = false;

	Move->WheelSetups.SetNum(4);
	const USkeletalMesh* SkelAsset = GetMesh() ? GetMesh()->GetSkeletalMeshAsset() : nullptr;
	for (int32 i = 0; i < 4; ++i)
	{
		FChaosWheelSetup& WS = Move->WheelSetups[i];
		WS.WheelClass = (i < 2) ? UApexDriftWheelFront::StaticClass() : UApexDriftWheelRear::StaticClass();
		WS.BoneName = ApexCar::WheelBones[i];
		// 物理轮心对齐到新车轮位：补偿 = 目标位置 - 骨骼参考位置
		if (SkelAsset)
		{
			const FReferenceSkeleton& Ref = SkelAsset->GetRefSkeleton();
			int32 Bone = Ref.FindBoneIndex(WS.BoneName);
			if (Bone != INDEX_NONE)
			{
				FTransform Xf = FTransform::Identity;
				while (Bone != INDEX_NONE)
				{
					Xf = Xf * Ref.GetRefBonePose()[Bone];
					Bone = Ref.GetParentIndex(Bone);
				}
				WS.AdditionalOffset = ApexCar::WheelRest[i] - Xf.GetLocation();
			}
		}
	}

	Move->EngineSetup.MaxTorque = ApexCar::BaseTorque;
	Move->EngineSetup.MaxRPM = 7800.0f;
	Move->EngineSetup.EngineIdleRPM = 1000.0f;
	Move->EngineSetup.EngineBrakeEffect = 0.16f;
	Move->EngineSetup.EngineRevUpMOI = 2.5f;
	Move->EngineSetup.EngineRevDownRate = 1400.0f;

	FRichCurve* TCurve = Move->EngineSetup.TorqueCurve.GetRichCurve();
	TCurve->Reset();
	TCurve->AddKey(0.0f, 560.0f);
	TCurve->AddKey(1100.0f, 640.0f);
	TCurve->AddKey(2400.0f, 730.0f);
	TCurve->AddKey(4200.0f, 780.0f);
	TCurve->AddKey(6400.0f, 750.0f);
	TCurve->AddKey(7800.0f, 600.0f);

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::RearWheelDrive;
	Move->DifferentialSetup.FrontRearSplit = 1.0f;

	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.FinalRatio = 3.10f;
	Move->TransmissionSetup.ChangeUpRPM = 6600.0f;
	Move->TransmissionSetup.ChangeDownRPM = 3900.0f;
	Move->TransmissionSetup.GearChangeTime = 0.10f;
	Move->TransmissionSetup.TransmissionEfficiency = 0.94f;
	Move->TransmissionSetup.ForwardGearRatios = { 3.60f, 2.40f, 1.75f, 1.36f, 1.08f, 0.86f };
	Move->TransmissionSetup.ReverseGearRatios = { 3.40f };

	// 转向角随速度的限制在 UpdateHandling 里按抓地极限计算，这里给满
	Move->SteeringSetup.SteeringType = ESteeringType::Ackermann;
	Move->SteeringSetup.AngleRatio = 0.75f;
	FRichCurve* SCurve = Move->SteeringSetup.SteeringCurve.GetRichCurve();
	SCurve->Reset();
	SCurve->AddKey(0.0f, 1.0f);
	SCurve->AddKey(200.0f, 1.0f);
}

void AApexDriftVehiclePawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeVisualsForPreview();
}

const FApexCarInfo& AApexDriftVehiclePawn::Car() const
{
	return FApexCarRegistry::Get(CarIndex);
}

void AApexDriftVehiclePawn::InitializeVisualsForPreview()
{
	if (!bCarApplied)
	{
		int32 Want = CarIndex;
		if (GetWorld() && GetWorld()->IsGameWorld())
		{
			if (const UApexDriftSaveGame* SG = UApexDriftSaveGame::LoadOrCreate()) Want = SG->VehicleIndex;
			FParse::Value(FCommandLine::Get(), TEXT("ApexCar="), Want);
		}
		SetCar(Want);
	}
}

void AApexDriftVehiclePawn::SetCar(int32 InCarIndex)
{
	const int32 Idx = FApexCarRegistry::Wrap(InCarIndex);
	if (bCarApplied && Idx == CarIndex)
	{
		return;
	}
	CarIndex = Idx;
	bCarApplied = true;
	ApplyCarVisuals();
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (Move && HasActorBegunPlay())
	{
		Move->SetMaxEngineTorque(ApexCar::BaseTorque * Car().Accel);
	}
}

void AApexDriftVehiclePawn::ApplyCarVisuals()
{
	const FApexCarInfo& C = Car();
	const FApexCarGeometry& G = C.Geo;
	auto Put = [&C](UStaticMeshComponent* Comp, const TCHAR* Part)
	{
		if (!Comp) return;
		UStaticMesh* SM = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *C.MeshPath(Part), nullptr, LOAD_NoWarn | LOAD_Quiet));
		Comp->EmptyOverrideMaterials();
		Comp->SetStaticMesh(SM);
		Comp->SetVisibility(SM != nullptr);
	};
	Put(ChassisMesh, TEXT("Body"));
	Put(ChassisGlassMesh, TEXT("Glass"));
	Put(LightsMesh, TEXT("Lights"));
	Put(GTWingMesh, TEXT("GTWing"));
	static const TCHAR* Calipers[4] = { TEXT("CaliperF_L"), TEXT("CaliperF_R"), TEXT("CaliperR_L"), TEXT("CaliperR_R") };
	for (int32 i = 0; i < WheelMeshes.Num(); ++i)
	{
		Put(WheelMeshes[i], i < 2 ? TEXT("WheelF") : TEXT("WheelR"));
		if (CaliperMeshes.IsValidIndex(i)) Put(CaliperMeshes[i], Calipers[i]);
		if (WheelPivots.IsValidIndex(i)) WheelPivots[i]->SetRelativeLocation(G.Wheel[i]);
	}

	if (HeadlightL) HeadlightL->SetRelativeLocation(FVector(G.Headlight.X + 12.0f, -G.Headlight.Y, G.Headlight.Z));
	if (HeadlightR) HeadlightR->SetRelativeLocation(FVector(G.Headlight.X + 12.0f, G.Headlight.Y, G.Headlight.Z));
	if (TaillightL) TaillightL->SetRelativeLocation(FVector(G.Taillight.X - 14.0f, -G.Taillight.Y, G.Taillight.Z));
	if (TaillightR) TaillightR->SetRelativeLocation(FVector(G.Taillight.X - 14.0f, G.Taillight.Y, G.Taillight.Z));
	if (HoodCamera) HoodCamera->SetRelativeLocation(G.HoodCam);
	if (BumperCamera) BumperCamera->SetRelativeLocation(G.BumperCam);
	if (ChaseSpringArm) ChaseSpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f + FMath::Max(G.Height - 125.0f, 0.0f) * 0.55f));
	if (ShowroomSpringArm)
	{
		ShowroomSpringArm->SetRelativeLocation(FVector(10.0f, 0.0f, 24.0f + FMath::Max(G.Height - 125.0f, 0.0f) * 0.3f));
		ShowroomSpringArm->TargetArmLength = 790.0f + FMath::Max(G.Height - 125.0f, 0.0f) * 1.2f;
	}

	BodyPaintMID = nullptr;
	TintMIDs.Reset();
	HeadLightMIDs.Reset();
	TailLightMIDs.Reset();
	BrakeLightMIDs.Reset();
	ReverseLightMIDs.Reset();
	SetupVisualMaterials();
	UpdateLightsAndMaterials(0.016f, 0.0f);
	for (int32 i = 0; i < PrevWheelSkidding.Num(); ++i)
	{
		PrevWheelSkidding[i] = false;
		if (SkidmarkManager) SkidmarkManager->BreakWheelTrail(i);
	}
}

void AApexDriftVehiclePawn::EmitExhaustFlames()
{
	if (!DriftVFXComp || !ChassisMesh || !Car().bExhaustFlame) return;
	const FTransform Xf = ChassisMesh->GetComponentTransform();
	for (const FVector& E : Car().Geo.Exhausts)
	{
		// 侧排朝外，尾排朝后
		const bool bSide = E.X > -60.0f;
		const FVector Dir = bSide ? GetActorRightVector() * FMath::Sign(E.Y) : -GetActorForwardVector();
		DriftVFXComp->EmitExhaustFlame(Xf.TransformPosition(E), Dir);
	}
}

FVector AApexDriftVehiclePawn::VisualContact(int32 Wheel, const FVector& PhysContact) const
{
	const FVector D = Car().Geo.Wheel[Wheel] - ApexCar::WheelRest[Wheel];
	return PhysContact + GetActorTransform().TransformVectorNoScale(FVector(D.X, D.Y, 0.0f));
}

void AApexDriftVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMeshComponent* M = GetMesh()) M->SetUseCCD(true);   // 300km/h 撞护墙不穿模
	PrevWheelContacts.Init(FVector::ZeroVector, 4);
	PrevWheelSkidding.Init(false, 4);

	if (GetMesh())
	{
		GetMesh()->OnComponentHit.AddDynamic(this, &AApexDriftVehiclePawn::OnChassisHit);
	}

	InitializeVisualsForPreview();
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetMaxEngineTorque(ApexCar::BaseTorque * Car().Accel);
	}
	BindSceneManagers();

	if (DriftScoreComp && VehicleAudioComp)
	{
		DriftScoreComp->OnDriftAudioCue.AddLambda([this](FName CueName, float)
		{
			if (VehicleAudioComp)
			{
				VehicleAudioComp->PlayUISound(CueName);
			}
		});
	}

	SetShowroomOrbitMode(bShowroomOrbit);
	const FApexWeatherSetup Setup = WeatherManager ? AApexWeatherManager::GetSetup(WeatherManager->MapId, WeatherManager->CurrentWeatherIndex) : FApexWeatherSetup();
	SetHeadlightsEnabled(Setup.TrackLights > 0.3f);
}

void AApexDriftVehiclePawn::SetupVisualMaterials()
{
	if (!ChassisMesh)
	{
		return;
	}

	// 漆面：所有名字含 Paint 的槽共用一个 MID，换色只改参数
	auto AssignPaint = [this](UStaticMeshComponent* Comp)
	{
		if (!Comp || !Comp->GetStaticMesh()) return;
		const TArray<FName> Slots = Comp->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (!Slots[i].ToString().Equals(TEXT("Paint"), ESearchCase::IgnoreCase)) continue;
			if (!BodyPaintMID)
			{
				if (UMaterialInterface* M = Comp->GetMaterial(i))
				{
					BodyPaintMID = UMaterialInstanceDynamic::Create(M, this);
				}
			}
			if (BodyPaintMID) Comp->SetMaterial(i, BodyPaintMID);
		}
	};
	AssignPaint(ChassisMesh);
	AssignPaint(GTWingMesh);

	// 贴图车：名字形如 Tex_*Paint 的槽乘上漆色
	if (ChassisMesh->GetStaticMesh() && TintMIDs.Num() == 0)
	{
		const TArray<FName> Slots = ChassisMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			const FString N = Slots[i].ToString();
			if (!N.StartsWith(TEXT("Tex_")) || !N.EndsWith(TEXT("Paint"))) continue;
			if (UMaterialInterface* M = ChassisMesh->GetMaterial(i))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
				ChassisMesh->SetMaterial(i, MID);
				TintMIDs.Add(MID);
			}
		}
	}

	// 灯具：按槽名分组创建 MID，亮度由 UpdateLightsAndMaterials 驱动
	if (LightsMesh && LightsMesh->GetStaticMesh() && HeadLightMIDs.Num() == 0 && TailLightMIDs.Num() == 0)
	{
		const TArray<FName> Slots = LightsMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			UMaterialInterface* M = LightsMesh->GetMaterial(i);
			if (!M) continue;
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
			LightsMesh->SetMaterial(i, MID);
			const FString N = Slots[i].ToString();
			if (N.Contains(TEXT("Head")) || N.Contains(TEXT("DRL"))) HeadLightMIDs.Add(MID);
			else if (N.Contains(TEXT("Brake"))) BrakeLightMIDs.Add(MID);
			else if (N.Contains(TEXT("Tail"))) TailLightMIDs.Add(MID);
			else if (N.Contains(TEXT("Reverse"))) ReverseLightMIDs.Add(MID);
		}
	}

	ApplyPaintPreset(CurrentPaintPreset);
}

void AApexDriftVehiclePawn::ApplyPaintPreset(int32 PresetIndex)
{
	CurrentPaintPreset = FApexPaintRegistry::Wrap(PresetIndex);
	const FApexPaintInfo& Paint = FApexPaintRegistry::GetPaints()[CurrentPaintPreset];
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : TintMIDs)
	{
		if (MID) MID->SetVectorParameterValue(FName("PaintColor"), Paint.Base);
	}
	if (!BodyPaintMID)
	{
		return;
	}
	BodyPaintMID->SetVectorParameterValue(FName("BaseColor"), Paint.Base);
	BodyPaintMID->SetVectorParameterValue(FName("FlopColor"), Paint.Flop);
	BodyPaintMID->SetScalarParameterValue(FName("Metallic"), Paint.Metallic);
	BodyPaintMID->SetScalarParameterValue(FName("FlakeStrength"), Paint.FlakeStrength);
	BodyPaintMID->SetScalarParameterValue(FName("Roughness"), Paint.Roughness);
}

void AApexDriftVehiclePawn::BindSceneManagers()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AApexTrackSplineActor> It(World); It; ++It) { TrackSplineActor = *It; break; }
	for (TActorIterator<AApexSkidmarkManager> It(World); It; ++It) { SkidmarkManager = *It; break; }
	for (TActorIterator<AApexWeatherManager> It(World); It; ++It) { WeatherManager = *It; break; }
}

void AApexDriftVehiclePawn::ApplyHandlingSettings(EApexHandlingAssist Assist, bool bManual, float InBaseFOV)
{
	HandlingAssist = Assist;
	int32 AssistArg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexAssist="), AssistArg)) HandlingAssist = static_cast<EApexHandlingAssist>(FMath::Clamp(AssistArg, 0, 2));
	bManualGearbox = bManual;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexManual"))) bManualGearbox = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexAuto"))) bManualGearbox = false;
	BaseFOV = FMath::Clamp(InBaseFOV, 68.0f, 105.0f);
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetUseAutomaticGears(!bManualGearbox);
	}
}

void AApexDriftVehiclePawn::SetControlsLocked(bool bLocked)
{
	bControlsLocked = bLocked;
	if (bLocked)
	{
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
		{
			Move->SetThrottleInput(0.0f);
			Move->SetBrakeInput(0.0f);
			Move->SetSteeringInput(0.0f);
			Move->SetHandbrakeInput(true);
			Move->SetTargetGear(1, true);
		}
	}
	else
	{
		if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
		{
			Move->SetBrakeInput(RawBrake);
			Move->SetHandbrakeInput(bRawHandbrake);
			Move->SetThrottleInput(RawThrottle);
			Move->SetSteeringInput(RawSteering);
			if (Move->GetCurrentGear() <= 0)
			{
				Move->SetTargetGear(1, true);
			}
		}
	}
}

void AApexDriftVehiclePawn::SetPausedState(bool bInPaused)
{
	bPaused = bInPaused;
}

void AApexDriftVehiclePawn::SetThrottleInput(float Value)
{
	RawThrottle = FMath::Clamp(Value, 0.0f, 1.0f);
	if (bControlsLocked) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetThrottleInput(RawThrottle);
	}
}

void AApexDriftVehiclePawn::SetBrakeInput(float Value)
{
	RawBrake = FMath::Clamp(Value, 0.0f, 1.0f);
	if (bControlsLocked) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetBrakeInput(RawBrake);
	}
}

void AApexDriftVehiclePawn::SetSteeringInput(float Value)
{
	RawSteering = FMath::Clamp(Value, -1.0f, 1.0f);
	if (bControlsLocked) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetSteeringInput(RawSteering);
	}
}

void AApexDriftVehiclePawn::SetHandbrakeInput(bool bPressed)
{
	bRawHandbrake = bPressed;
	if (bControlsLocked) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetHandbrakeInput(bPressed);
	}
}

void AApexDriftVehiclePawn::ShiftUp()
{
	if (bControlsLocked || !bManualGearbox) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		const int32 Curr = Move->GetCurrentGear();
		Move->SetTargetGear(FMath::Clamp(Curr + 1, -1, 6), false);
	}
}

void AApexDriftVehiclePawn::ShiftDown()
{
	if (bControlsLocked || !bManualGearbox) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		const int32 Curr = Move->GetCurrentGear();
		Move->SetTargetGear(FMath::Clamp(Curr - 1, -1, 6), false);
	}
}

void AApexDriftVehiclePawn::CycleCameraMode()
{
	if (AutoRun.IsActive() && bDirector)
	{
		// 自动漂移中按视角键：先退出自动运镜，停在当前追尾视角
		bDirector = false;
		SetCameraMode(CameraMode);
		return;
	}
	const int32 NextIdx = (static_cast<int32>(CameraMode) + 1) % 4;
	SetCameraMode(static_cast<EApexCameraMode>(NextIdx));
}

void AApexDriftVehiclePawn::SetCameraMode(EApexCameraMode NewMode)
{
	CameraMode = NewMode;
	bCameraYawValid = false;
	if (bShowroomOrbit) return;

	// 切机位那一帧不做运动模糊/时域累积，避免整屏糊一下
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	ChaseCamera->SetActive(CameraMode == EApexCameraMode::DynamicDriftChase || CameraMode == EApexCameraMode::CloseChase);
	HoodCamera->SetActive(CameraMode == EApexCameraMode::HoodCam);
	BumperCamera->SetActive(CameraMode == EApexCameraMode::BumperCam);
	ShowroomCamera->SetActive(false);
	if (CineCamera) CineCamera->SetActive(false);
	CineShot = 0;

	if (CameraMode == EApexCameraMode::CloseChase)
	{
		ChaseSpringArm->SocketOffset = FVector(0.0f, 0.0f, 92.0f);
	}
	else if (CameraMode == EApexCameraMode::DynamicDriftChase)
	{
		ChaseSpringArm->SocketOffset = FVector(0.0f, 0.0f, 110.0f);
	}
}

void AApexDriftVehiclePawn::SetShowroomOrbitMode(bool bEnableShowroom)
{
	bShowroomOrbit = bEnableShowroom;
	ShowroomCamera->SetActive(bEnableShowroom);
	if (bEnableShowroom)
	{
		ChaseCamera->SetActive(false);
		HoodCamera->SetActive(false);
		BumperCamera->SetActive(false);
		if (CineCamera) CineCamera->SetActive(false);
		CineShot = 0;
	}
	else
	{
		SetCameraMode(CameraMode);
	}
}

void AApexDriftVehiclePawn::AddShowroomOrLookDelta(float YawDelta, float PitchDelta)
{
	if (bShowroomOrbit)
	{
		ShowroomYaw += YawDelta * 1.4f;
		ShowroomPitch = FMath::Clamp(ShowroomPitch + PitchDelta * 1.0f, -26.0f, -2.0f);
	}
	else
	{
		LookYawOffset = FMath::Clamp(LookYawOffset + YawDelta * 1.8f, -110.0f, 110.0f);
	}
}

void AApexDriftVehiclePawn::SetLookBack(bool bEnableLookBack)
{
	bLookBack = bEnableLookBack;
}

void AApexDriftVehiclePawn::ToggleHeadlights()
{
	SetHeadlightsEnabled(!bHeadlightsOn);
}

void AApexDriftVehiclePawn::SetHeadlightsEnabled(bool bEnabled)
{
	bHeadlightsOn = bEnabled;
	const bool bNight = WeatherManager && AApexWeatherManager::GetSetup(WeatherManager->MapId, WeatherManager->CurrentWeatherIndex).bNight;
	const float Beam = bNight ? 26.0f : 6.0f;
	if (HeadlightL) HeadlightL->SetIntensity(Beam);
	if (HeadlightR) HeadlightR->SetIntensity(Beam);
	if (HeadlightL) HeadlightL->SetVisibility(bHeadlightsOn);
	if (HeadlightR) HeadlightR->SetVisibility(bHeadlightsOn);
}

void AApexDriftVehiclePawn::ResetHandlingState()
{
	if (AutoRun.IsActive()) EndAutoRun();
	EndDrift();
	ExitAssistTimer = 0.0f;
	WallAssistTimer = 0.0f;
	bSimFree = false;
	SimRate = 0.0f;
	GripBlend = 1.0f;
	SmoothedSteer = 0.0f;
	PathRateDeg = 0.0f;
	TangentAccel = 0.0f;
	bPrevVelYawValid = false;
	TangentAssist = 0.0f;
}

void AApexDriftVehiclePawn::ResetToTrack()
{
	if (!TrackSplineActor)
	{
		BindSceneManagers();
	}
	FTransform ResetXf = GetActorTransform();
	ResetXf.SetRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Quaternion());
	ResetXf.AddToTranslation(FVector(0.0f, 0.0f, 80.0f));

	if (TrackSplineActor && TrackSplineActor->bFreeRoam)
	{
		// 自由场地：原地扶正，从上方找落脚的平面（跳台顶、平台上也行）
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ApexReset), false, this);
		const FVector L = GetActorLocation();
		if (GetWorld()->LineTraceSingleByObjectType(Hit, L + FVector(0, 0, 250.0f), L - FVector(0, 0, 20000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			ResetXf.SetLocation(Hit.ImpactPoint + FVector(0, 0, 60.0f));
		}
	}
	else if (TrackSplineActor)
	{
		float Dist = 0.0f, Lat = 0.0f;
		TrackSplineActor->FindClosestTrackTransform(GetActorLocation(), ResetXf, Dist, Lat);
	}

	SetActorTransform(ResetXf, false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* M = GetMesh())
	{
		M->SetPhysicsLinearVelocity(FVector::ZeroVector);
		M->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->ResetVehicleState();
	}
	ResetHandlingState();
	if (SkidmarkManager)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			SkidmarkManager->BreakWheelTrail(i);
		}
	}
	if (DriftScoreComp)
	{
		DriftScoreComp->BankCurrentDrift();
	}
}

void AApexDriftVehiclePawn::OnChassisHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bControlsLocked || !GetMesh())
	{
		return;
	}
	const float MassKg = FMath::Max(500.0f, GetMesh()->GetMass());
	const float ImpactSpeed = NormalImpulse.Size() / MassKg;
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		// 撞飞道具：只出声，不断连击、不触发撞墙辅助
		if (ImpactSpeed > 12.0f && VehicleAudioComp && GetWorld()->GetTimeSeconds() - LastPropHitTime > 0.06f)
		{
			LastPropHitTime = GetWorld()->GetTimeSeconds();
			VehicleAudioComp->TriggerImpactSound(FMath::Clamp(ImpactSpeed * 3.0f, 170.0f, 420.0f));
		}
		return;
	}
	if (AutoRun.IsActive() && ImpactSpeed > 15.0f)
	{
		AutoRun.NotifyHit(FMath::Abs(Hit.ImpactNormal.Z) < 0.6f && ImpactSpeed > 40.0f, FString::Printf(TEXT("HIT %s 法线(%.2f,%.2f,%.2f) 冲量速度 %.0f"), *GetNameSafe(OtherActor), Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z, ImpactSpeed));
	}
	if (FMath::Abs(Hit.ImpactNormal.Z) < 0.6f && ImpactSpeed > 60.0f)
	{
		WallAssistTimer = 0.45f;
		if (ImpactSpeed > 420.0f) { EndDrift(); bSimFree = false; }
	}
	if (ImpactSpeed < 160.0f)
	{
		return;
	}
	if (VehicleAudioComp)
	{
		VehicleAudioComp->TriggerImpactSound(ImpactSpeed);
	}
	if (DriftScoreComp)
	{
		DriftScoreComp->NotifyWallImpact(ImpactSpeed);
	}
	if (DriftVFXComp && ImpactSpeed > 200.0f)
	{
		DriftVFXComp->EmitImpactSparks(Hit.ImpactPoint, Hit.ImpactNormal, FMath::Clamp(ImpactSpeed / 800.0f, 0.35f, 1.0f));
	}
}

void AApexDriftVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Move)
	{
		return;
	}

	const FVector VelWorld = GetVelocity();
	const FVector VelLocal = GetActorTransform().InverseTransformVectorNoScale(VelWorld);
	const float SpeedKmh = VelWorld.Size() * 0.036f;

	float SlipAngleDeg = 0.0f;
	if (SpeedKmh > 12.0f)
	{
		SlipAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(VelLocal.Y, FMath::Max(FMath::Abs(VelLocal.X), 140.0f)));
	}

	if ((AutoRun.IsActive() || bSimFree) && SpeedKmh > 12.0f)
	{
		SlipAngleDeg = FMath::Clamp(-(bSimFree ? SimPsi : AutoRun.GetPsi()), -90.0f, 90.0f);
	}

	const int32 NumW = Move->GetNumWheels();
	int32 GroundCount = 0;
	for (int32 i = 0; i < NumW; ++i)
	{
		if (Move->GetWheelState(i).bInContact)
		{
			GroundCount++;
		}
	}
	const bool bGrounded = (GroundCount >= 2);

	const float RPM = Move->GetEngineRotationSpeed();
	const int32 Gear = Move->GetCurrentGear();

	const float RPMNorm = FMath::Clamp((RPM - 2400.0f) / 5200.0f, 0.0f, 1.0f);
	const float TargetBoost = (!bControlsLocked && RawThrottle > 0.25f && Car().bTurbo) ? (1.40f * RPMNorm * RawThrottle) : 0.0f;
	if (PrevThrottle > 0.55f && RawThrottle < 0.15f && TurboPressureBar > 0.65f)
	{
		if (VehicleAudioComp) VehicleAudioComp->TriggerBOVFlutter();
		if (RPM > 4600.0f)
		{
			if (VehicleAudioComp && Car().bExhaustFlame) VehicleAudioComp->TriggerExhaustBackfire();
			EmitExhaustFlames();
		}
	}
	if (Gear != PrevGear && Gear > 0 && RPM > 4500.0f && !bControlsLocked)
	{
		if (VehicleAudioComp && Car().bExhaustFlame) VehicleAudioComp->TriggerExhaustBackfire();
		EmitExhaustFlames();
	}
	TurboPressureBar = FMath::FInterpTo(TurboPressureBar, TargetBoost, DeltaSeconds, TargetBoost > TurboPressureBar ? 4.5f : 7.0f);
	PrevThrottle = RawThrottle;
	PrevGear = Gear;

	UpdateHandling(DeltaSeconds, SpeedKmh, SlipAngleDeg, bGrounded);
	UpdateWheelVisuals();
	UpdateCameraSystem(DeltaSeconds, SpeedKmh, SlipAngleDeg);

	// 湿润度只决定听到的是水花声还是干胎声，不参与抓地
	const float Wetness = WeatherManager ? WeatherManager->VisualWetness : 0.0f;
	float MaxSkidIntensity = 0.0f;
	bool bOnCurb = false;
	UpdateVFXAndSkidmarks(DeltaSeconds, MaxSkidIntensity, bOnCurb);
	UpdateLightsAndMaterials(DeltaSeconds, SpeedKmh);

	if (DriftScoreComp)
	{
		DriftScoreComp->UpdateScoring(DeltaSeconds, SpeedKmh, SlipAngleDeg, bGrounded, GetActorLocation(), !bControlsLocked && !bPaused);
	}

	if (VehicleAudioComp)
	{
		VehicleAudioComp->EnginePitch = Car().SoundPitch;
		VehicleAudioComp->EngineGain = Car().SoundVolume;
		VehicleAudioComp->UpdateVehicleAudio(DeltaSeconds, RPM, RawThrottle, SpeedKmh, MaxSkidIntensity, Wetness, bOnCurb, TurboPressureBar, bPaused);
	}

	Telemetry.SpeedKmh = SpeedKmh;
	Telemetry.EngineRPM = RPM;
	Telemetry.MaxRPM = Move->GetEngineMaxRotationSpeed();
	Telemetry.Gear = Gear;
	Telemetry.TurboBoostBar = TurboPressureBar;
	Telemetry.SlipAngleDeg = SlipAngleDeg;
	Telemetry.bHandbrakeActive = bRawHandbrake;
	Telemetry.bManualGearbox = bManualGearbox;
	Telemetry.bHeadlightsOn = bHeadlightsOn;
	Telemetry.CameraMode = CameraMode;
	Telemetry.ThrottleInput = RawThrottle;
	Telemetry.BrakeInput = RawBrake;

	if (DriftScoreComp)
	{
		Telemetry.bIsDrifting = DriftScoreComp->IsDrifting();
		Telemetry.bWrongWay = DriftScoreComp->IsWrongWay();
		Telemetry.CurrentDriftScore = DriftScoreComp->GetCurrentDriftScore();
		Telemetry.TotalScore = DriftScoreComp->GetTotalScore();
		Telemetry.BestSingleDrift = DriftScoreComp->GetBestSingleDrift();
		Telemetry.PeakAngleDeg = DriftScoreComp->GetPeakAngleDeg();
		Telemetry.DriftMultiplier = DriftScoreComp->GetDriftMultiplier();
		Telemetry.MaxDriftCombo = DriftScoreComp->GetMaxComboMultiplier();
		Telemetry.TransitionGraceNormalized = DriftScoreComp->GetTransitionGraceNormalized();
		Telemetry.bInClippingZone = DriftScoreComp->IsInClippingZone();
		Telemetry.ActiveZoneName = DriftScoreComp->GetActiveZoneName();
		Telemetry.CalloutText = DriftScoreComp->GetCalloutText();
		Telemetry.CalloutAlpha = DriftScoreComp->GetCalloutAlpha();
		Telemetry.CurrentLap = DriftScoreComp->GetCurrentLap();
		Telemetry.TotalLaps = DriftScoreComp->GetTotalLaps();
		Telemetry.CurrentLapTime = DriftScoreComp->GetCurrentLapTime();
		Telemetry.BestLapTime = DriftScoreComp->GetBestLapTime();
		Telemetry.SessionTimeRemaining = DriftScoreComp->GetSessionTimeRemaining();
		Telemetry.SessionTime = DriftScoreComp->GetSessionTime();
	}

	if (TrackSplineActor)
	{
		Telemetry.PlayerMinimapPos = TrackSplineActor->ProjectWorldToMinimap(GetActorLocation());
		// 甩尾和 360 时小地图跟行进方向，不跟车头
		const bool bByVel = (AutoRun.IsActive() || bSimFree) && SpeedKmh > 15.0f;
		Telemetry.PlayerHeadingRad = FMath::DegreesToRadians(bByVel ? FMath::RadiansToDegrees(FMath::Atan2(VelWorld.Y, VelWorld.X)) : GetActorRotation().Yaw);
		Telemetry.bAutoRun = AutoRun.IsActive();
		Telemetry.AutoProgress = AutoRun.GetProgress();
		Telemetry.bAutoDirector = bDirector;
	}

	if (DriftVFXComp)
	{
		FVector CamLoc = GetActorLocation() - GetActorForwardVector() * 550.0f + FVector(0.0f, 0.0f, 200.0f);
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				CamLoc = PC->PlayerCameraManager->GetCameraLocation();
			}
		}
		SmokeEmitAccum += DeltaSeconds;
		const float EmitInterval = FMath::Lerp(0.07f, 0.030f, MaxSkidIntensity);
		const float AbsSlipDeg = FMath::Abs(SlipAngleDeg);
		if (MaxSkidIntensity > 0.12f && SpeedKmh > 12.0f && SmokeEmitAccum >= EmitInterval)
		{
			SmokeEmitAccum = 0.0f;
			for (int32 i = 0; i < FMath::Min(4, NumW); ++i)
			{
				const FWheelStatus& WS = Move->GetWheelState(i);
				const float SlipBoost = (i >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f) ? FMath::Clamp((AbsSlipDeg - 11.0f) / 28.0f, 0.0f, 1.0f) : 0.0f;
				const float SkidNorm = FMath::Max(FMath::Clamp((WS.SkidMagnitude + WS.SlipMagnitude * 0.5f) / 480.0f, 0.0f, 1.0f), SlipBoost);
				const bool bWheelSlide = WS.bInContact && (WS.bIsSkidding || WS.bIsSlipping || (bRawHandbrake && i >= 2 && SpeedKmh > 20.0f) || (i >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f));
				if (bWheelSlide && SkidNorm > 0.14f)
				{
					DriftVFXComp->EmitTireSmoke(VisualContact(i, WS.ContactPoint), VelWorld, SkidNorm);
				}
			}
		}
		DriftVFXComp->UpdateParticles(DeltaSeconds, CamLoc);
	}
}

void AApexDriftVehiclePawn::SetAllWheelGrip(float FrontGrip, float RearGrip)
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Move) return;
	if (!FMath::IsNearlyEqual(FrontGrip, AppliedFrontGrip, 0.01f))
	{
		AppliedFrontGrip = FrontGrip;
		Move->SetWheelFrictionMultiplier(0, FrontGrip);
		Move->SetWheelFrictionMultiplier(1, FrontGrip);
	}
	if (!FMath::IsNearlyEqual(RearGrip, AppliedRearGrip, 0.01f))
	{
		AppliedRearGrip = RearGrip;
		Move->SetWheelFrictionMultiplier(2, RearGrip);
		Move->SetWheelFrictionMultiplier(3, RearGrip);
	}
}

// 操控模型：
//  - 抓地态：高附着 + 按抓地极限限制转角 + 轻度车身稳定，正常过弯不会自己甩尾
//  - 漂移态：由手刹（或专业档的大油门）主动触发；用目标侧滑角闭环控制车头朝向，
//    用向心/切向补偿维持弧线与车速；松油门或回正即自然收回
//  - 天气不参与任何计算
namespace
{
	// 全油门目标加速度（m/s2）：0-100 约 2.5s，0-200 约 6s，0-300 约 12s，极速约 365km/h
	float TargetAccelAt(float InKmh, float Accel, float Top)
	{
		const float Kmh = InKmh * 368.0f / FMath::Max(Top, 120.0f);
		static const float V[] = { 0.f, 60.f, 110.f, 160.f, 210.f, 260.f, 310.f, 345.f, 368.f };
		static const float A[] = { 12.f, 12.f, 10.4f, 8.6f, 6.8f, 5.0f, 3.2f, 1.5f, 0.f };
		for (int32 i = 1; i < UE_ARRAY_COUNT(V); ++i)
		{
			if (Kmh < V[i]) return Accel * FMath::Lerp(A[i - 1], A[i], (Kmh - V[i - 1]) / (V[i] - V[i - 1]));
		}
		return -0.8f;
	}
}

void AApexDriftVehiclePawn::UpdateHandling(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg, bool bGrounded)
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	USkeletalMeshComponent* Body = GetMesh();
	if (!Move || !Body)
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.001f, 0.05f);
	const FApexCarInfo& C = Car();

	if (bControlsLocked)
	{
		EndDrift();
		TangentAssist = 0.0f;
		bPrevVelYawValid = false;
		SetAllWheelGrip(3.4f, 3.9f);
		return;
	}

	if (AutoRun.IsActive())
	{
		// 自动漂移：独立分支，不走三档操控
		Body->SetAngularDamping(bGrounded ? 0.05f : 2.6f);
		AutoRun.Drive(*this, Dt, bGrounded);
		return;
	}

	Body->SetAngularDamping(bGrounded ? 0.05f : 2.6f);
	WallAssistTimer = FMath::Max(0.0f, WallAssistTimer - Dt);
	ExitAssistTimer = FMath::Max(0.0f, ExitAssistTimer - Dt);

	// 键盘是 0/1 输入：按速度限制打方向的快慢，回正更快
	{
		const bool bReturning = FMath::Abs(RawSteering) < FMath::Abs(SmoothedSteer) || RawSteering * SmoothedSteer < 0.0f;
		const float HighSpeed = FMath::Clamp(SpeedKmh / 260.0f, 0.0f, 1.0f);
		const float Rate = C.Agility * (bReturning ? 8.0f : FMath::Lerp(5.5f, 2.6f, HighSpeed));
		SmoothedSteer = FMath::FInterpConstantTo(SmoothedSteer, RawSteering, Dt, Rate);
	}

	const FVector VelWorld = GetVelocity();
	const FVector Vel2D(VelWorld.X, VelWorld.Y, 0.0f);
	const float SpeedCms = Vel2D.Size();
	const float SpeedMs = SpeedCms * 0.01f;
	const FVector VelLocal = GetActorTransform().InverseTransformVectorNoScale(VelWorld);
	const bool bForward = VelLocal.X > 0.0f;

	// 速度方向的角速度与切向加速度（低通）
	if (SpeedCms > 250.0f)
	{
		const float VelYaw = FMath::RadiansToDegrees(FMath::Atan2(Vel2D.Y, Vel2D.X));
		if (bPrevVelYawValid)
		{
			const float Rate = FMath::FindDeltaAngleDegrees(PrevVelYawDeg, VelYaw) / Dt;
			PathRateDeg = FMath::FInterpTo(PathRateDeg, FMath::Clamp(Rate, -160.0f, 160.0f), Dt, 10.0f);
			TangentAccel = FMath::FInterpTo(TangentAccel, FMath::Clamp((SpeedCms - PrevSpeedCms) * 0.01f / Dt, -30.0f, 30.0f), Dt, 10.0f);
		}
		PrevVelYawDeg = VelYaw;
		bPrevVelYawValid = true;
	}
	else
	{
		bPrevVelYawValid = false;
		PathRateDeg = 0.0f;
		TangentAccel = 0.0f;
	}
	PrevSpeedCms = SpeedCms;

	const float Beta = (SpeedKmh > 10.0f && bForward) ? SlipAngleDeg : 0.0f; // 速度相对车头的方向角，右正
	const float AbsBeta = FMath::Abs(Beta);
	const bool bSimRaw = (HandlingAssist == EApexHandlingAssist::SimulationRaw);
	const bool bPro = (HandlingAssist == EApexHandlingAssist::DriftPro);

	if (bSimRaw)
	{
		if (UpdateSimSlide(Dt, SpeedKmh, bGrounded, bForward)) return;
	}
	else
	{
		bSimFree = false;
	}

	// ---- 进入漂移：只由玩家主动触发，撞墙、落地造成的侧滑一律走抓地稳定 ----
	if (bGrounded && bForward && SpeedKmh > 28.0f && !bSimRaw)
	{
		const bool bHandEntry = bRawHandbrake && FMath::Abs(RawSteering) > 0.18f;
		if (!bDriftActive)
		{
			bool bEnter = bHandEntry;
			if (!bEnter && bPro)
			{
				// 专业：低速重油满舵才会甩尾
				const bool bPower = RawThrottle > 0.85f && FMath::Abs(RawSteering) > 0.8f && SpeedKmh > 36.0f && SpeedKmh < 105.0f && ExitAssistTimer <= 0.0f;
				PowerOverTimer = bPower ? PowerOverTimer + Dt : 0.0f;
				bEnter = PowerOverTimer > 0.65f;
			}
			if (bEnter)
			{
				bDriftActive = true;
				DriftDir = FMath::Sign(RawSteering);
				DriftTime = 0.0f;
				DriftAngle = FMath::Clamp(-Beta * DriftDir, 8.0f, 30.0f);
				DriftExitTimer = 0.0f;
				PowerOverTimer = 0.0f;
				TangentAssist = 0.0f;
			}
		}
		else if (bHandEntry && RawSteering * DriftDir < -0.3f)
		{
			// 换向连漂：反方向 + 再点一下手刹
			DriftDir = -DriftDir;
			DriftTime = 0.0f;
			DriftAngle = 12.0f;
			DriftExitTimer = 0.0f;
		}
	}

	// ---- 漂移目标角：往弯内打才维持；松方向自然收；反打立刻收。不会自己翻到另一边 ----
	if (bDriftActive)
	{
		DriftTime += Dt;
		const float SteerIn = RawSteering * DriftDir;
		const float MaxAngle = FMath::Min((bPro ? 54.0f : 44.0f) * C.Drift, 57.0f);   // 再大就不容易收住
		float Goal = 0.0f;
		float FallRate = (bPro ? 42.0f : 50.0f) + 1.3f * DriftAngle;   // 度/秒：松方向约 0.6s 收完
		if (SteerIn > 0.12f)
		{
			Goal = FMath::Lerp(24.0f * C.Drift, MaxAngle, FMath::Pow(SteerIn, 0.8f));
		}
		else if (SteerIn < -0.12f)
		{
			FallRate = bPro ? 150.0f : 175.0f;               // 反打：约 0.3s 收完
		}
		if (bRawHandbrake && SteerIn > -0.12f) Goal = FMath::Max(Goal, 32.0f) + 6.0f;
		Goal *= FMath::Lerp(0.30f, 1.0f, FMath::Clamp(RawThrottle * 1.6f, 0.0f, 1.0f));
		Goal *= 1.0f - 0.9f * FMath::Clamp(RawBrake * 1.5f, 0.0f, 1.0f);
		Goal *= FMath::Lerp(1.0f, 0.72f, FMath::Clamp((SpeedKmh - 130.0f) / 110.0f, 0.0f, 1.0f));
		if (RawThrottle < 0.2f || RawBrake > 0.3f) FallRate = FMath::Max(FallRate, 95.0f);
		Goal = FMath::Clamp(Goal, 0.0f, MaxAngle);
		DriftAngle = FMath::FInterpConstantTo(DriftAngle, Goal, Dt, Goal > DriftAngle ? 115.0f : FallRate);

		// 退出：目标角收完且车身基本对正；或目标角已归零但车身迟迟没对正，也交给抓地稳定
		const bool bWantOut = Goal < 1.0f && DriftAngle < 6.0f && !bRawHandbrake;
		DriftExitTimer = bWantOut ? DriftExitTimer + Dt : 0.0f;
		if ((bWantOut && AbsBeta < 11.0f) || DriftExitTimer > 0.35f || SpeedKmh < 18.0f || !bForward)
		{
			EndDrift();
		}
	}
	GripBlend = FMath::FInterpConstantTo(GripBlend, bDriftActive ? 0.0f : 1.0f, Dt, bDriftActive ? 8.0f : 4.5f);

	const float Mass = FMath::Max(500.0f, Body->GetMass());
	const FVector Up = GetActorUpVector();

	// ---- 抓地系数（与天气无关） ----
	if (bSimRaw)
	{
		// 拟真：正常轮胎抓地（甩起来以后由 UpdateSimSlide 接管）；直线拉手刹只锁后轮
		const float FastS = FMath::Clamp((SpeedMs - 40.0f) / 40.0f, 0.0f, 1.0f);
		SetAllWheelGrip(FMath::Lerp(3.1f, 2.4f, FastS), bRawHandbrake ? 1.5f : FMath::Lerp(3.6f, 4.4f, FastS));
	}
	else
	{
		// 高速时前轮略放、后轮收紧：后轴更稳，不打方向就走直线
		const float Fast = FMath::Clamp((SpeedMs - 40.0f) / 40.0f, 0.0f, 1.0f);
		SetAllWheelGrip(FMath::Lerp(1.15f, FMath::Lerp(3.4f, 2.5f, Fast), GripBlend), FMath::Lerp(0.95f, FMath::Lerp(3.9f, 4.6f, Fast), GripBlend));
	}

	// 侧向能力随速度（下压力）增长
	const float LatMax = C.Grip * ((bSimRaw ? 14.0f : 16.5f) + 5.0f * FMath::Clamp(FMath::Square(SpeedMs / 70.0f), 0.0f, 1.3f)) * (1.0f - 0.15f * RawBrake);

	// ---- 转向 ----
	float SteerDeg = 0.0f;
	{
		// 抓地极限允许的转角：阿克曼角 + 轮胎侧偏余量
		const float V2 = FMath::Max(SpeedMs * SpeedMs, 1.0f);
		const float LimitDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(ApexCar::WheelBaseM * LatMax / V2)) + (bSimRaw ? 4.0f : 5.5f), bSimRaw ? 4.5f : 6.0f, ApexCar::MaxSteerDeg);
		const float GripSteer = SmoothedSteer * LimitDeg;
		// 漂移时前轮对准速度方向（自动反打），玩家输入做小幅修正
		const float DriftSteer = FMath::Clamp(Beta + RawSteering * 7.0f, -ApexCar::MaxSteerDeg, ApexCar::MaxSteerDeg);
		// 拟真：甩起来以后反打允许打满，平时按抓地极限给
		const float SimSteer = (RawSteering * Beta > 0.0f) ? FMath::Clamp(SmoothedSteer * FMath::Max(LimitDeg, AbsBeta + 4.0f), -ApexCar::MaxSteerDeg, ApexCar::MaxSteerDeg) : GripSteer;
		SteerDeg = bSimRaw ? SimSteer : FMath::Lerp(DriftSteer, GripSteer, GripBlend);
	}
	Move->SetSteeringInput(FMath::Clamp(SteerDeg / ApexCar::MaxSteerDeg, -1.0f, 1.0f));

	// 被撞得横过来甚至调头：车速还在就把车头拧回行进方向，不让它一路打转
	if (bGrounded && !bForward && SpeedKmh > 35.0f && !bRawHandbrake && Move->GetCurrentGear() >= 0)
	{
		const float VelYaw = FMath::RadiansToDegrees(FMath::Atan2(Vel2D.Y, Vel2D.X));
		const float Err = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, VelYaw);
		FVector Av = Body->GetPhysicsAngularVelocityInDegrees();
		const float Yr = FVector::DotProduct(Av, Up);
		const float Want = FMath::Clamp(Err * 3.2f, -190.0f, 190.0f);
		Av += Up * ((Want - Yr) * (1.0f - FMath::Exp(-9.0f * Dt)));
		Body->SetPhysicsAngularVelocityInDegrees(Av);
		WallAssistTimer = FMath::Max(WallAssistTimer, 0.3f);
	}
	if (!bGrounded || !bForward || SpeedMs < 2.5f)
	{
		TangentAssist = 0.0f;
		return;
	}

	const FVector Dir = Vel2D / FMath::Max(SpeedCms, 1.0f);
	const FVector Right(-Dir.Y, Dir.X, 0.0f);

	// 高速下压，免得过坡发飘
	Body->AddForce(-Up * FMath::Min(0.0009f * SpeedMs * SpeedMs, 6.0f) * 100.0f * Mass);

	// ---- 纵向：抓地态的加速/刹车补偿（拟真档也给，速度才跟得上） ----
	if (!bDriftActive)
	{
		const int32 Gear = Move->GetCurrentGear();
		const float RPM = Move->GetEngineRotationSpeed();
		float Desired = 0.0f;
		bool bActive = false;
		if (RawBrake > 0.05f && SpeedMs > 3.0f)
		{
			Desired = -17.5f * RawBrake;
			bActive = true;
		}
		else if (RawThrottle > 0.05f && !bRawHandbrake && Gear >= 0)
		{
			Desired = TargetAccelAt(SpeedKmh, C.Accel, C.TopKmh) * RawThrottle * (1.0f - 0.22f * FMath::Abs(SmoothedSteer));
			if (bManualGearbox)
			{
				// 手动：转速太低没劲，顶到红线就不再给补偿，需要自己升挡
				Desired *= FMath::Clamp(RPM / 3600.0f, 0.25f, 1.0f);
				if (RPM > Move->GetEngineMaxRotationSpeed() - 120.0f) Desired = FMath::Min(Desired, 0.0f);
			}
			bActive = true;
		}
		if (bActive)
		{
			const bool bAccel = Desired >= 0.0f && RawBrake <= 0.05f;
			// 接近这辆车的极速时允许更大的负补偿，把速度压住
			const float NearTop = FMath::Clamp((SpeedKmh / FMath::Max(C.TopKmh, 120.0f) - 0.82f) / 0.18f, 0.0f, 1.0f);
			TangentAssist = FMath::Clamp(TangentAssist + (Desired - TangentAccel) * 4.5f * Dt, bAccel ? -1.5f - 7.0f * NearTop : -15.0f, bAccel ? 15.0f : 1.0f);
		}
		else
		{
			TangentAssist = FMath::FInterpTo(TangentAssist, 0.0f, Dt, 10.0f);
		}
		Body->AddForce(Dir * TangentAssist * 100.0f * Mass);
	}

	if (SpeedKmh < 15.0f)
	{
		return;
	}

	FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();
	const float YawRate = FVector::DotProduct(AngVel, Up);

	if (bSimRaw)
	{
		// 拟真（抓地态）：允许一定侧滑不管，超出才扶正；手刹甩起来后不走这里
		const float FastYaw = FMath::Clamp((SpeedMs - 35.0f) / 50.0f, 0.0f, 1.0f);
		const float LowSpd = 1.0f - FMath::Clamp((SpeedKmh - 70.0f) / 90.0f, 0.0f, 1.0f);
		const float Free = FMath::Lerp(3.0f, 0.3f, FastYaw) + 9.0f * C.Drift * RawThrottle * FMath::Abs(SmoothedSteer) * LowSpd;
		const float Excess = Beta - FMath::Clamp(Beta, -Free, Free);
		const float MaxYaw = FMath::Min(80.0f, FMath::RadiansToDegrees(LatMax / FMath::Max(SpeedMs, 6.0f)));
		const float TargetRate = SmoothedSteer * MaxYaw;
		const float PathRef = FMath::Lerp(PathRateDeg, TargetRate, FMath::Lerp(0.35f, 1.0f, FastYaw));
		const float Recover = (WallAssistTimer > 0.0f) ? 1.0f : 0.0f;
		const float DesiredYaw = FMath::Clamp(PathRef + (3.4f + 1.6f * Recover) * Excess, -145.0f, 145.0f);
		const float W = FMath::Max3(FMath::Lerp(0.35f, 0.93f, FastYaw), FMath::Clamp(FMath::Abs(Excess) / 9.0f, 0.0f, 0.95f), 0.9f * Recover);
		const float Stiff = FMath::Lerp(10.0f, 48.0f, FastYaw);
		const float NewYaw = FMath::Lerp(YawRate, DesiredYaw, W * (1.0f - FMath::Exp(-Stiff * Dt)));
		AngVel += Up * (NewYaw - YawRate);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);
		const float DesiredLat = SpeedMs * FMath::DegreesToRadians(TargetRate);
		const float MeasuredLat = SpeedMs * FMath::DegreesToRadians(PathRateDeg);
		Body->AddForce(Right * FMath::Clamp(0.55f * (DesiredLat - MeasuredLat), -6.0f, 6.0f) * FMath::Clamp(SpeedKmh / 60.0f, 0.0f, 1.0f) * 100.0f * Mass);
	}
	else if (bDriftActive)
	{
		const float SteerIn = RawSteering * DriftDir; // >0 往弯内打，<0 反打
		const float BetaTarget = -DriftDir * DriftAngle;

		// 车头朝向闭环：dβ/dt = 路径角速度 - 横摆角速度；收角时更快
		const bool bClosing = AbsBeta > DriftAngle + 2.0f;
		const float K = bRawHandbrake ? 4.0f : (bClosing ? 4.8f : 3.2f);
		const float DesiredYaw = FMath::Clamp(PathRateDeg + K * (Beta - BetaTarget), -145.0f, 145.0f);
		const float NewYaw = FMath::Lerp(YawRate, DesiredYaw, 1.0f - FMath::Exp(-10.0f * Dt));
		AngVel += Up * (NewYaw - YawRate);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);

		// 向心：角度越大、往弯内打得越多，弧线越紧
		const float AngleGain = FMath::Clamp(AbsBeta / 30.0f, 0.0f, 1.1f);
		const float DesiredLat = -FMath::Sign(Beta) * 15.0f * FMath::Lerp(1.0f, C.Grip, 0.6f) * AngleGain
			* (0.84f + 0.30f * FMath::Max(SteerIn, -0.6f)) * FMath::Lerp(0.72f, 1.0f, RawThrottle)
			* FMath::Clamp(SpeedKmh / 55.0f, 0.4f, 1.0f);
		const float MeasuredLat = SpeedMs * FMath::DegreesToRadians(PathRateDeg);
		const float LatAssist = FMath::Clamp(0.45f * DesiredLat + 0.75f * (DesiredLat - MeasuredLat), -15.0f, 15.0f);

		// 切向：给油能把漂移车速带到 150km/h 左右，进弯快就慢慢掉，松油减速
		const float PowerAccel = FMath::Lerp(7.0f * C.Accel, -3.2f, FMath::Clamp((SpeedKmh - 60.0f) / 130.0f, 0.0f, 1.0f));
		float DesiredTan = FMath::Lerp(-3.5f, PowerAccel, RawThrottle) - RawBrake * 9.0f;
		if (bRawHandbrake) DesiredTan -= 2.6f;
		TangentAssist = FMath::Clamp(TangentAssist + (DesiredTan - TangentAccel) * 5.0f * Dt, -5.0f, 13.0f);

		Body->AddForce((Right * LatAssist + Dir * TangentAssist) * 100.0f * Mass);
	}
	else
	{
		// 抓地态：按方向盘给目标转弯率，同时把车头拉回速度方向。任何来路的侧滑都会收回来
		const float MaxYaw = FMath::Min(88.0f, FMath::RadiansToDegrees(LatMax / FMath::Max(SpeedMs, 6.0f)));
		const float TargetRate = SmoothedSteer * MaxYaw;
		const float Recover = FMath::Max(ExitAssistTimer > 0.0f ? 1.0f : 0.0f, WallAssistTimer > 0.0f ? 1.0f : 0.0f);
		const float FastYaw = FMath::Clamp((SpeedMs - 35.0f) / 50.0f, 0.0f, 1.0f);
		const float PathRef = FMath::Lerp(PathRateDeg, TargetRate, FMath::Lerp(0.6f, 1.0f, FastYaw));
		const float DesiredYaw = FMath::Clamp(PathRef + (3.8f + 2.0f * Recover) * Beta, -150.0f, 150.0f);
		const float W = FMath::Max(FMath::Clamp(FMath::Lerp(0.45f, 0.92f, FastYaw) + AbsBeta / 16.0f, 0.0f, 0.97f), 0.95f * Recover);
		// 车速越高，轮胎自身的横摆扰动越大，闭环也要越硬
		const float Stiff = FMath::Lerp(11.0f, 46.0f, FastYaw);
		const float NewYaw = FMath::Lerp(YawRate, DesiredYaw, W * (1.0f - FMath::Exp(-Stiff * Dt)));
		AngVel += Up * (NewYaw - YawRate);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);

		const float DesiredLat = SpeedMs * FMath::DegreesToRadians(TargetRate);
		const float MeasuredLat = SpeedMs * FMath::DegreesToRadians(PathRateDeg);
		const float LatAssist = FMath::Clamp(0.85f * (DesiredLat - MeasuredLat), -10.0f, 10.0f) * FMath::Clamp(SpeedKmh / 60.0f, 0.0f, 1.0f);
		Body->AddForce(Right * LatAssist * 100.0f * Mass);
	}
}

float AApexDriftVehiclePawn::TargetAccel(float Kmh, const FApexCarInfo& Info)
{
	return TargetAccelAt(Kmh, Info.Accel, Info.TopKmh);
}

void AApexDriftVehiclePawn::PrepareAutoRun()
{
	if (!TrackSplineActor) BindSceneManagers();
	AutoRun.Prepare(TrackSplineActor, *this, Car());
}

bool AApexDriftVehiclePawn::BeginAutoRun()
{
	EndDrift();
	if (!AutoRun.Begin())
	{
		return false;
	}
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetUseAutomaticGears(false);
	}
	// 车身底盘蹭到路面折面时不要被摩擦拽住（轮胎抓地不受影响）
	if (!AutoPhysMat)
	{
		AutoPhysMat = NewObject<UPhysicalMaterial>(this);
		AutoPhysMat->Friction = 0.0f;
		AutoPhysMat->StaticFriction = 0.0f;
		AutoPhysMat->Restitution = 0.0f;
		AutoPhysMat->bOverrideFrictionCombineMode = true;
		AutoPhysMat->FrictionCombineMode = EFrictionCombineMode::Min;
		AutoPhysMat->bOverrideRestitutionCombineMode = true;
		AutoPhysMat->RestitutionCombineMode = EFrictionCombineMode::Min;
	}
	if (GetMesh()) GetMesh()->SetPhysMaterialOverride(AutoPhysMat);
	return true;
}

void AApexDriftVehiclePawn::EndAutoRun()
{
	AutoRun.Stop();
	if (GetMesh()) GetMesh()->SetPhysMaterialOverride(nullptr);
	EndDrift();
	ExitAssistTimer = 0.55f;
	bSimFree = false;
	SimRate = 0.0f;
	GripBlend = 1.0f;
	SmoothedSteer = 0.0f;
	TangentAssist = 0.0f;
	bPrevVelYawValid = false;
	RawThrottle = RawBrake = RawSteering = 0.0f;
	bRawHandbrake = false;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetUseAutomaticGears(!bManualGearbox);
		Move->SetHandbrakeInput(false);
	}
}

void AApexDriftVehiclePawn::AutoApplyInputs(float Throttle, float Brake, float SteerDeg, bool bHand, float FrontGrip, float RearGrip)
{
	RawThrottle = Throttle;
	RawBrake = Brake;
	RawSteering = FMath::Clamp(SteerDeg / ApexCar::MaxSteerDeg, -1.0f, 1.0f);
	bRawHandbrake = bHand;
	SetAllWheelGrip(FrontGrip, RearGrip);
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetThrottleInput(Throttle);
		Move->SetBrakeInput(0.0f);      // 减速由路线跟踪完成，这里只点亮刹车灯，免得抱死轮子
		Move->SetSteeringInput(RawSteering);
		Move->SetHandbrakeInput(bHand);
	}
}

// 拟真档的自由滑动。车头绕行进方向的转动按惯性走：
//   手刹+方向 给一个持续的甩出力矩，大过轮胎回正力矩，按住就一直转，360、720 都行；
//   松开手刹后只剩轮胎自己的回正（0 度稳定、180 度不稳定）和阻尼：没甩过头就自然回正，
//   甩过头靠惯性越过 180 度，顺势转完整圈再对正。不做任何人为拉回。
//   油门+往弯内打 能把角度维持在 50 度左右；反打随时收。
bool AApexDriftVehiclePawn::UpdateSimSlide(float Dt, float SpeedKmh, bool bGrounded, bool bForward)
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	USkeletalMeshComponent* Body = GetMesh();
	const FApexCarInfo& C = Car();
	const FVector Vel = GetVelocity();
	const FVector Vel2D(Vel.X, Vel.Y, 0.0f);
	const float SpeedMs = Vel2D.Size() * 0.01f;
	const FVector Up = GetActorUpVector();
	const float VelYaw = FMath::RadiansToDegrees(FMath::Atan2(Vel2D.Y, Vel2D.X));
	const float Psi = (SpeedMs > 2.0f) ? FMath::FindDeltaAngleDegrees(VelYaw, GetActorRotation().Yaw) : 0.0f;
	FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();
	const float YawRate = FVector::DotProduct(AngVel, Up);

	if (!bSimFree)
	{
		if (!(bGrounded && bForward && SpeedKmh > 30.0f && bRawHandbrake && FMath::Abs(RawSteering) > 0.18f))
		{
			SimPsi = Psi;
			return false;
		}
		bSimFree = true;
		SimRate = FMath::Clamp(YawRate - PathRateDeg, -60.0f, 60.0f);
		SimSpin = 0.0f;
		SimPsi = Psi;
		SimAirT = 0.0f;
		TangentAssist = 0.0f;
	}

	SimAirT = bGrounded ? 0.0f : SimAirT + Dt;
	const float PrevPsi = SimPsi;
	SimSpin += FMath::FindDeltaAngleDegrees(PrevPsi, Psi);
	SimPsi = Psi;
	const float AbsPsi = FMath::Abs(Psi);
	const bool bCaught = !bRawHandbrake && AbsPsi < 60.0f && (PrevPsi * Psi < 0.0f || (AbsPsi < 5.0f && FMath::Abs(SimRate) < 30.0f));
	if (FMath::Abs(SimSpin) > 325.0f && AbsPsi < 45.0f)
	{
		SimSpin -= FMath::Sign(SimSpin) * 360.0f;
		if (DriftScoreComp) DriftScoreComp->AwardSpin();
	}
	if (bCaught || SpeedKmh < 12.0f || SimAirT > 0.35f)
	{
		bSimFree = false;
		SimRate = 0.0f;
		if (bCaught) WallAssistTimer = FMath::Max(WallAssistTimer, 0.30f);   // 收车那一下扶稳
		return false;
	}
	if (!bGrounded)
	{
		return true;
	}

	// ---- 转动
	const float Sp = FMath::Clamp(SpeedMs / 13.0f, 0.25f, 1.0f);
	const float K = 515.0f * Sp;                                  // 回正力矩峰值（度/秒2）
	const bool bInto = SmoothedSteer * Psi >= 0.0f;               // 往已经甩的方向打
	float Torque = 0.0f;
	if (bRawHandbrake) Torque = SmoothedSteer * K * 1.62f * C.Drift;
	else if (bInto) Torque = SmoothedSteer * K * FMath::Lerp(0.30f, 0.76f, RawThrottle) * C.Drift;
	else Torque = SmoothedSteer * K * 0.85f;                      // 反打
	const float Align = -K * FMath::Sin(FMath::DegreesToRadians(Psi));
	const float Damp = bRawHandbrake ? 1.5f : FMath::Lerp(3.6f, 0.9f, FMath::Clamp((AbsPsi - 55.0f) / 25.0f, 0.0f, 1.0f));
	SimRate += (Torque + Align - Damp * SimRate) * Dt;
	SimRate = FMath::Clamp(SimRate, -345.0f, 345.0f);
	if (!bRawHandbrake && SimRate * Psi < 0.0f)
	{
		// 快转回正前时收住，不要甩过头
		const float Cap = 5.2f * AbsPsi + 22.0f;
		SimRate = FMath::Clamp(SimRate, -Cap, Cap);
	}
	const float DesiredYaw = PathRateDeg + SimRate;
	AngVel += Up * ((DesiredYaw - YawRate) * (1.0f - FMath::Exp(-30.0f * Dt)));
	Body->SetPhysicsAngularVelocityInDegrees(AngVel);

	// ---- 轨迹：推力沿车头方向，横着时产生向心力；倒着时反而减速
	SetAllWheelGrip(0.55f, bRawHandbrake ? 0.35f : 0.5f);
	const float Mass = FMath::Max(500.0f, Body->GetMass());
	const FVector Dir = Vel2D.GetSafeNormal();
	const FVector Right(-Dir.Y, Dir.X, 0.0f);
	const float SinP = FMath::Sin(FMath::DegreesToRadians(Psi));
	const float CosP = FMath::Cos(FMath::DegreesToRadians(Psi));
	const float Lat = SinP * FMath::Lerp(5.0f, 14.0f, RawThrottle) * FMath::Lerp(1.0f, C.Grip, 0.6f) * FMath::Clamp(SpeedMs / 14.0f, 0.3f, 1.0f);
	const float MeasLat = SpeedMs * FMath::DegreesToRadians(PathRateDeg);
	const float LatF = FMath::Clamp(0.5f * Lat + 0.6f * (Lat - MeasLat), -15.0f, 15.0f);
	float Tan = RawThrottle * C.Accel * FMath::Lerp(8.0f, 1.0f, FMath::Clamp((SpeedMs - 15.0f) / 38.0f, 0.0f, 1.0f)) * FMath::Max(CosP, -0.35f)
		- 1.6f * FMath::Abs(SinP) - 0.4f - (bRawHandbrake ? 1.6f : 0.0f) - RawBrake * 9.0f - (1.0f - RawThrottle) * 1.5f;
	TangentAssist = FMath::Clamp(TangentAssist + (Tan - TangentAccel) * 5.0f * Dt, -6.0f, 15.0f);
	Body->AddForce((Right * LatF + Dir * TangentAssist - Up * FMath::Min(0.0009f * SpeedMs * SpeedMs, 5.0f)) * 100.0f * Mass);

	// ---- 前轮：指向行进方向（自动反打的样子），玩家方向做修正
	const float SteerDeg = AbsPsi < 95.0f ? FMath::Clamp(-Psi + SmoothedSteer * 9.0f, -ApexCar::MaxSteerDeg, ApexCar::MaxSteerDeg) : SmoothedSteer * 22.0f;
	Move->SetSteeringInput(FMath::Clamp(SteerDeg / ApexCar::MaxSteerDeg, -1.0f, 1.0f));
	if (AbsPsi > 75.0f && Move->GetCurrentGear() < 1) Move->SetTargetGear(FMath::Max(2, Move->GetTargetGear()), true);   // 倒着滑时别自己挂倒挡
	return true;
}

void AApexDriftVehiclePawn::EndDrift()
{
	if (bDriftActive)
	{
		ExitAssistTimer = 0.55f;
		TangentAssist = FMath::Max(TangentAssist, 0.0f);
	}
	bDriftActive = false;
	DriftAngle = 0.0f;
	DriftExitTimer = 0.0f;
	PowerOverTimer = 0.0f;
}

void AApexDriftVehiclePawn::UpdateWheelVisuals()
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Move) return;
	const int32 Num = FMath::Min3(Move->Wheels.Num(), WheelPivots.Num(), 4);
	for (int32 i = 0; i < Num; ++i)
	{
		const UChaosVehicleWheel* W = Move->Wheels[i];
		if (!W || !WheelPivots[i]) continue;
		const FApexCarGeometry& Geo = Car().Geo;
		FVector Loc = Geo.Wheel[i];
		Loc.Z += FMath::Min(W->GetSuspensionOffset() + 2.2f, 1.4f); // 轮拱间隙有限，压缩行程可视上限
		WheelPivots[i]->SetRelativeLocationAndRotation(Loc, FRotator(0.0f, (i < 2) ? W->GetSteerAngle() : 0.0f, 0.0f));
		if (WheelMeshes[i])
		{
			// 网格外侧朝 +Y：右轮直接用，左轮绕 Z 转 180°，自转方向随之取反
			const bool bLeft = (i % 2 == 0);
			const float SpinK = (i < 2) ? ApexCar::PhysRadiusF / FMath::Max(Geo.RadiusF, 10.0f) : ApexCar::PhysRadiusR / FMath::Max(Geo.RadiusR, 10.0f);
			const FQuat Spin(FVector::RightVector, FMath::DegreesToRadians(-W->GetRotationAngle() * SpinK));
			const FQuat Face(FVector::UpVector, bLeft ? PI : 0.0f);
			WheelMeshes[i]->SetRelativeRotation(Spin * Face);
		}
	}
}

void AApexDriftVehiclePawn::SetCineShot(int32 Shot, float Dur)
{
	CineT = 0.0f;
	CineDur = Dur;
	if (Shot == CineShot) return;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	const bool bWasCine = CineShot > 0;
	CineShot = Shot;
	bCineReset = true;
	CineAngle = 0.0f;
	CineSide = AutoRun.GetCornerDir() != 0.0f ? FMath::Sign(AutoRun.GetCornerDir()) : (CineSeq % 2 ? 1.0f : -1.0f);
	if (Shot > 0)
	{
		ChaseCamera->SetActive(false);
		HoodCamera->SetActive(false);
		BumperCamera->SetActive(false);
		CineCamera->SetActive(true);
	}
	else if (bWasCine)
	{
		SetCameraMode(CameraMode);
	}
}

// 自动漂移的运镜：以追尾为主，漂移和 360 时切到前方低机位、侧面、航拍、低后方、环绕
bool AApexDriftVehiclePawn::UpdateDirector(float Dt)
{
	const bool bOn = AutoRun.IsActive() && bDirector && !bLookBack && !bShowroomOrbit;
	if (!bOn)
	{
		if (CineShot > 0) SetCineShot(0, 0.0f);
		CineT = 0.0f;
		CineDur = 3.0f;
		CineSeq = 0;
		return false;
	}
	CineT += Dt;
	const float AbsPsi = FMath::Abs(AutoRun.GetPsi());
	const bool bSliding = AbsPsi > 16.0f;
	const bool bSpin = AutoRun.IsSpinning();
	if (CineShot == 0)
	{
		// 追尾至少看 3 秒；马上要 360 或正在漂移时切出去
		const bool bSpinSoon = AutoRun.TimeToNextSpin() < 0.35f;
		if (CineT > CineDur && !bSpin && (bSpinSoon || bSliding))
		{
			static const int32 SpinShots[] = { 3, 1, 5, 4 };
			static const int32 SlideShots[] = { 2, 1, 5, 4, 3 };
			const int32 Shot = bSpinSoon ? SpinShots[CineSeq % 4] : SlideShots[CineSeq % 5];
			++CineSeq;
			SetCineShot(Shot, bSpinSoon ? 2.1f : 2.5f);
		}
	}
	else if (CineT > CineDur && !bSpin)
	{
		SetCineShot(0, 2.6f);
	}
	if (CineShot == 0)
	{
		return false;
	}

	const FVector CarLoc = GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
	const float CarLat = AutoRun.GetLateral();
	FVector2D Dir;
	auto Ground = [this](const FVector2D& P, float RefZ) -> float
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ApexCine), false, this);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(P.X, P.Y, RefZ + 260.0f), FVector(P.X, P.Y, RefZ - 900.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Q)) return Hit.ImpactPoint.Z;
		return RefZ - 55.0f;
	};
	float Ahead = 0.0f, Lat = 0.0f, Height = 80.0f, Fov = 62.0f, LookAhead = 0.0f;
	switch (CineShot)
	{
	case 1: Ahead = 9.2f; Lat = CarLat + CineSide * 1.8f; Height = 66.0f; Fov = 56.0f; break;                       // 前方低机位，车冲着镜头滑过来
	case 2: Ahead = 2.6f; Lat = CarLat - CineSide * 5.6f; Height = 78.0f; Fov = 76.0f; break;                        // 弯外侧贴地平移
	case 3: Ahead = -8.6f; Lat = CarLat; Height = 450.0f; Fov = 64.0f; LookAhead = 1.0f; break;                      // 航拍
	case 4: Ahead = -5.6f; Lat = CarLat - CineSide * 1.4f; Height = 52.0f; Fov = 80.0f; LookAhead = 1.5f; break;     // 贴地后方
	default:                                                                                                          // 环绕
		CineAngle += 62.0f * Dt * CineSide;
		Ahead = -7.6f * FMath::Cos(FMath::DegreesToRadians(CineAngle + 40.0f * CineSide));
		Lat = CarLat + 6.0f * FMath::Sin(FMath::DegreesToRadians(CineAngle + 40.0f * CineSide));
		Height = 165.0f;
		Fov = 66.0f;
		break;
	}
	Lat = FMath::Clamp(Lat, -7.2f, 7.2f);
	const FVector2D P = AutoRun.PathPoint(Ahead, Lat, Dir);
	const float WantZ = Ground(P, CarLoc.Z) + Height;
	if (bCineReset)
	{
		CineZ = WantZ;
		bCineReset = false;
	}
	CineZ = FMath::FInterpTo(CineZ, WantZ, Dt, 12.0f);
	FVector Cam(P.X, P.Y, CineZ);
	{
		// 隧道、拱门下别穿顶
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ApexCineTop), false, this);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, CarLoc + FVector(0, 0, 60), Cam, FCollisionObjectQueryParams(ECC_WorldStatic), Q)) Cam = FMath::Lerp(CarLoc, Hit.ImpactPoint, 0.9f);
	}
	FVector Look = CarLoc;
	if (LookAhead > 0.0f)
	{
		FVector2D D2;
		const FVector2D L2 = AutoRun.PathPoint(LookAhead, CarLat, D2);
		Look = FVector(L2.X, L2.Y, CarLoc.Z);
	}
	CamShakeTime += Dt;
	const float Amp = FMath::Clamp(GetVelocity().Size() * 0.036f / 260.0f, 0.0f, 1.0f);
	Look += FVector(0.0f, 0.0f, 1.0f) * FMath::PerlinNoise1D(CamShakeTime * 7.0f) * 3.5f * Amp;
	CineCamera->SetWorldLocationAndRotation(Cam, (Look - Cam).Rotation());
	CineCamera->SetFieldOfView(Fov);
	CineCamera->PostProcessBlendWeight = 1.0f;
	CineCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CineCamera->PostProcessSettings.MotionBlurAmount = 0.3f;
	return true;
}

void AApexDriftVehiclePawn::UpdateCameraSystem(float DeltaSeconds, float SpeedKmh, float SlipAngleDeg)
{
	if (bShowroomOrbit)
	{
		if (!bShowroomFreeze) ShowroomYaw += DeltaSeconds * 5.0f;
		else if (DebugArm > 0.0f) ShowroomSpringArm->TargetArmLength = DebugArm;
		ShowroomSpringArm->SetRelativeRotation(FRotator(ShowroomPitch, ShowroomYaw, 0.0f));
		return;
	}

	if (UpdateDirector(DeltaSeconds))
	{
		bCameraYawValid = false;
		return;
	}
	if (CameraMode == EApexCameraMode::DynamicDriftChase || CameraMode == EApexCameraMode::CloseChase)
	{
		// 速度感：视野随车速拉开，加速时镜头被甩远、刹车时贴近，高速轻微抖动
		const float SpeedNorm = FMath::Clamp(SpeedKmh / 300.0f, 0.0f, 1.15f);
		const float Accel = FMath::Clamp(TangentAccel / 12.0f, -1.2f, 1.0f);
		CamAccel = FMath::FInterpTo(CamAccel, Accel, DeltaSeconds, 3.5f);
		const float TargetFOV = FMath::Min(BaseFOV + FMath::Pow(SpeedNorm, 0.85f) * 24.0f + FMath::Max(CamAccel, 0.0f) * 3.0f, 116.0f);
		ChaseCamera->FieldOfView = FMath::FInterpTo(ChaseCamera->FieldOfView, TargetFOV, DeltaSeconds, 4.0f);
		ChaseCamera->PostProcessBlendWeight = 1.0f;
		ChaseCamera->PostProcessSettings.bOverride_MotionBlurAmount = true;
		ChaseCamera->PostProcessSettings.MotionBlurAmount = FMath::Lerp(0.26f, 0.42f, FMath::Clamp(SpeedNorm, 0.0f, 1.0f));
		{
			CamShakeTime += DeltaSeconds;
			const float Amp = FMath::Square(FMath::Clamp((SpeedKmh - 120.0f) / 200.0f, 0.0f, 1.0f));
			const float T = CamShakeTime;
			ChaseCamera->SetRelativeLocation(FVector(0.0f,
				(FMath::PerlinNoise1D(T * 9.0f) * 1.4f + FMath::PerlinNoise1D(T * 23.0f + 7.0f) * 0.7f) * Amp,
				(FMath::PerlinNoise1D(T * 11.0f + 31.0f) * 1.8f + FMath::PerlinNoise1D(T * 27.0f + 3.0f) * 0.9f) * Amp));
			ChaseCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, FMath::PerlinNoise1D(T * 8.0f + 53.0f) * 0.22f * Amp));
		}

		const float CarYaw = GetActorRotation().Yaw;
		float YawOffset = 0.0f;
		if (bLookBack)
		{
			YawOffset = 180.0f;
		}
		else
		{
			// 漂移时镜头偏向行进方向，看得到弯道出口
			const float Follow = (CameraMode == EApexCameraMode::DynamicDriftChase) ? 0.62f : 0.38f;
			LookYawOffset = FMath::FInterpTo(LookYawOffset, 0.0f, DeltaSeconds, 2.5f);
			YawOffset = FMath::Clamp(SlipAngleDeg * Follow, -32.0f, 32.0f) + LookYawOffset;
		}
		float TargetYaw = CarYaw + YawOffset;
		const bool bFreeCam = AutoRun.IsActive() || bSimFree;
		if (bFreeCam && !bLookBack && SpeedKmh > 20.0f)
		{
			// 自动漂移 / 拟真甩尾：镜头守住行进方向，车在画面里甩和转
			const float Psi = AutoRun.IsActive() ? AutoRun.GetPsi() : SimPsi;
			const FVector Vl = GetVelocity();
			const float PathYaw = AutoRun.IsActive() ? AutoRun.GetPathYaw() : FMath::RadiansToDegrees(FMath::Atan2(Vl.Y, Vl.X));
			const float Want = (AutoRun.IsSpinning() || FMath::Abs(Psi) > 70.0f) ? 0.0f : FMath::Clamp(Psi * 0.36f, -20.0f, 20.0f);
			CamPsi = FMath::FInterpTo(CamPsi, Want, DeltaSeconds, 3.0f);
			TargetYaw = PathYaw + CamPsi + LookYawOffset;
		}
		else
		{
			CamPsi = FMath::FInterpTo(CamPsi, 0.0f, DeltaSeconds, 3.0f);
		}
		if (!bCameraYawValid)
		{
			CameraYaw = TargetYaw;
			bCameraYawValid = true;
		}
		const float Delta = FMath::FindDeltaAngleDegrees(CameraYaw, TargetYaw);
		CameraYaw += Delta * (1.0f - FMath::Exp(-(bLookBack ? 18.0f : 7.5f) * DeltaSeconds));
		const float Pitch = -9.0f + FMath::Clamp(GetActorRotation().Pitch * 0.6f, -10.0f, 10.0f);
		ChaseSpringArm->SetWorldRotation(FRotator(Pitch, CameraYaw, 0.0f));
		const float BaseArm = ((CameraMode == EApexCameraMode::DynamicDriftChase) ? 560.0f : 440.0f) + (Car().Geo.Length - 457.0f) * 0.5f + FMath::Max(Car().Geo.Height - 125.0f, 0.0f) * 0.6f;
		ChaseSpringArm->TargetArmLength = BaseArm - FMath::Min(SpeedNorm, 1.0f) * 70.0f + CamAccel * 55.0f;
	}
}

void AApexDriftVehiclePawn::UpdateVFXAndSkidmarks(float DeltaSeconds, float& OutMaxSkid, bool& bOutOnCurb)
{
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	if (!Move) return;

	OutMaxSkid = 0.0f;
	bOutOnCurb = false;

	const FVector Vel = GetVelocity();
	const FVector VelLocal = GetActorTransform().InverseTransformVectorNoScale(Vel);
	const float SpeedKmh = Vel.Size() * 0.036f;
	const float AbsSlipDeg = (SpeedKmh > 12.0f)
		? ((AutoRun.IsSpinning() || (bSimFree && FMath::Abs(SimPsi) > 70.0f)) ? 70.0f : FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(VelLocal.Y, FMath::Max(FMath::Abs(VelLocal.X), 140.0f)))))
		: 0.0f;
	const int32 NumW = FMath::Min(4, Move->GetNumWheels());

	for (int32 i = 0; i < NumW; ++i)
	{
		const FWheelStatus& WS = Move->GetWheelState(i);
		const float SlipBoost = (i >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f) ? FMath::Clamp((AbsSlipDeg - 11.0f) / 28.0f, 0.0f, 1.0f) : 0.0f;
		const float SkidNorm = FMath::Max(FMath::Clamp((WS.SkidMagnitude + WS.SlipMagnitude * 0.5f) / 480.0f, 0.0f, 1.0f), SlipBoost);
		const bool bSliding = WS.bInContact && (WS.bIsSkidding || WS.bIsSlipping || (bRawHandbrake && i >= 2 && SpeedKmh > 20.0f) || (i >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f)) && SkidNorm > 0.14f;

		if (bSliding)
		{
			OutMaxSkid = FMath::Max(OutMaxSkid, SkidNorm);
		}

		if (WS.bInContact && TrackSplineActor)
		{
			FTransform Dummy;
			float Dist = 0.0f, Lat = 0.0f;
			TrackSplineActor->FindClosestTrackTransform(WS.ContactPoint, Dummy, Dist, Lat);
			const float HalfW = TrackSplineActor->TrackWidthCm * 0.5f;
			if (FMath::Abs(Lat) > HalfW * 0.88f && FMath::Abs(Lat) < HalfW * 1.15f)
			{
				bOutOnCurb = true;
			}
		}

		if (SkidmarkManager && PrevWheelContacts.IsValidIndex(i))
		{
			if (bSliding && SpeedKmh > 14.0f)
			{
				// 高帧率下每帧位移可能不足 8cm，攒够距离再提交，否则低速漂移画不出胎痕
				const FVector Contact = VisualContact(i, WS.ContactPoint);
				const bool bFar = FVector::DistSquared(PrevWheelContacts[i], Contact) > FMath::Square(14.0f);
				if (PrevWheelSkidding[i] && bFar)
				{
					const float HalfW = FMath::Clamp(((i >= 2) ? Car().Geo.WidthR : Car().Geo.WidthF) * 0.6f, 11.0f, 22.0f);
					const float Alpha = FMath::Clamp(SkidNorm * 0.92f, 0.25f, 0.92f);
					SkidmarkManager->AddSkidSegment(i, PrevWheelContacts[i], Contact, FVector::UpVector, HalfW, Alpha);
				}
				if (!PrevWheelSkidding[i] || bFar) PrevWheelContacts[i] = Contact;
				PrevWheelSkidding[i] = true;
			}
			else
			{
				if (PrevWheelSkidding[i])
				{
					SkidmarkManager->BreakWheelTrail(i);
				}
				PrevWheelSkidding[i] = false;
			}
		}
	}
}

void AApexDriftVehiclePawn::UpdateLightsAndMaterials(float DeltaSeconds, float SpeedKmh)
{
	const bool bBraking = (RawBrake > 0.08f) || bRawHandbrake;
	const bool bReversing = (Telemetry.Gear < 0);

	const bool bTailOn = bBraking || bHeadlightsOn;
	for (UPointLightComponent* L : { TaillightL.Get(), TaillightR.Get() })
	{
		if (!L) continue;
		if (L->IsVisible() != bTailOn) L->SetVisibility(bTailOn);
		if (bTailOn) L->SetIntensity(bBraking ? 140.0f : 45.0f);
	}

	auto Drive = [](const TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs, float Value)
	{
		for (const TObjectPtr<UMaterialInstanceDynamic>& MID : MIDs)
		{
			if (MID) MID->SetScalarParameterValue(FName("Intensity"), Value);
		}
	};
	Drive(HeadLightMIDs, bHeadlightsOn ? 22.0f : 3.0f);
	Drive(TailLightMIDs, bBraking ? 16.0f : (bHeadlightsOn ? 6.0f : 0.8f));
	Drive(BrakeLightMIDs, bBraking ? 26.0f : 0.15f);
	Drive(ReverseLightMIDs, bReversing ? 14.0f : 0.05f);
}
