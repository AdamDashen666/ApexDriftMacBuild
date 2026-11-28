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

// 拟真档的轮胎模型参数。车身坐标：X 向前、Y 向右，单位米；横摆角速度向右转为正
namespace ApexSim
{
	constexpr float G = 9.81f;
	constexpr float CoMX = -0.04f;           // 质心在车身原点后 4cm（与 CenterOfMassOverride 一致）
	constexpr float CoMH = 0.34f;            // 算载荷转移用的质心高度（低趴超跑）：高速带下压过弯到 1.5g 以上时，太高会把内侧轮压空
	constexpr float RollFront = 0.64f;       // 横向载荷转移由前轴扛的比例：前轴多扛一点，到极限先推头而不是先甩尾
	// 峰值附着系数（运动轮胎）：全油门的加速度曲线和其他档一样猛（180km/h 时约 0.9g），附着低了 180 还在空转、一打方向就甩；
	// 现在约 140km/h 以下全油门才会让后轮空转（这么大马力的后驱车本来就这样），侧向后轮比前轮高一点，到极限先推头
	constexpr float MuLatF = 1.32f, MuLatR = 1.50f;
	constexpr float MuLongF = 1.40f, MuLongR = 1.55f;
	// 侧偏刚度：后轮比前轮“硬”（峰值侧偏角更小）。重刹时载荷往前移，前轮刚度跟着变大，
	// 后轮余量不够车就从推头变成甩尾（第一版 230km/h 带刹车入弯直接打转）；现在重刹仍是推头
	constexpr float PeakAlphaF = 0.150f;     // tan(前轮峰值侧偏角约 8.5°)
	constexpr float PeakAlphaR = 0.105f;     // tan(后轮峰值侧偏角约 6°)
	constexpr float PeakKappa = 0.11f;       // 峰值滑移率
	constexpr float SlideLatF = 0.80f;       // 越过峰值后侧向只剩峰值的多少（横着滑）：前 80%
	constexpr float SlideLatR = 0.72f;       // 后轮峰值高、滑动后掉得多：到极限前很稳，一旦滑开仍能漂（滑动摩擦约 1.1g，和峰值低时一样）
	constexpr float SlideLong = 0.86f;       // 纵向（空转 / 抱死）剩 86%
	constexpr float LoadSens = 0.16f;        // 载荷越大，单位载荷的附着越低
	constexpr float YawK = 1.50f;            // 横摆转动惯量 Iz = 质量 × 1.5 m2
	constexpr float RearInertia = 45.0f;     // 后轴两轮加传动折算到胎面的等效质量（kg）
	constexpr float BrakeAccel = 13.0f;      // 满刹车的制动力（折算成 m/s2），前 78% 后 22%
	constexpr float BrakeFront = 0.78f;
	// 后轮制动最多用掉后轮附着的 60%，弯中再按后轮侧向占用减（真车的制动力分配 / 弯道制动控制）：
	// 后轮始终留着侧向力，直线重刹、高速带刹车入弯都是推头而不是甩尾
	constexpr float RearBrakeCap = 0.60f;
	constexpr float RearRadius = 0.347f;
	// Chaos 自己的轮胎摩擦清零、不给它油门和刹车：它只管悬架、碰撞、轮子跟着地面转（转速、自动换挡照旧），
	// 车身受的轮胎力全部由下面的模型出。实测只把摩擦降到 10%、扭矩降到 6% 时 Chaos 仍能推出约 8m/s2，两套轮胎打架
	constexpr float ChaosGrip = 0.0f;

	// 调参用：命令行可临时覆盖（-ApexSimSlideF= -ApexSimSlideR= -ApexSimDecay= -ApexSimTcsSlip=），正式游玩用默认值
	struct FTune { float SlideF = SlideLatF; float SlideR = SlideLatR; float Decay = 1.4f; float TcsSlip = 0.3f; float TcsHand = 0.1f; bool bOldTcs = false; float SpinBase = 1.2f; float SpinGain = 0.7f; float SpinGainHand = 2.0f; };
	inline const FTune& Tune()
	{
		static const FTune T = []
		{
			FTune R;
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimSpinBase="), R.SpinBase);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimSpinGain="), R.SpinGain);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimSpinGainHand="), R.SpinGainHand);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimTcsHand="), R.TcsHand);
			R.bOldTcs = FParse::Param(FCommandLine::Get(), TEXT("ApexSimOldTCS"));   // 对比：v2.6 的“一横就放开”
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimSlideF="), R.SlideF);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimSlideR="), R.SlideR);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimDecay="), R.Decay);
			FParse::Value(FCommandLine::Get(), TEXT("ApexSimTcsSlip="), R.TcsSlip);
			return R;
		}();
		return T;
	}

	// 合成滑移 S（峰值处为 1）对应的附着利用率：峰值前正弦上升，越过峰值衰减到滑动摩擦
	inline float Curve(float S, float Slide)
	{
		return S <= 1.0f ? FMath::Sin(UE_HALF_PI * S) : Slide + (1.0f - Slide) * FMath::Exp(-(S - 1.0f) * Tune().Decay);
	}

	// 一个轮胎的纵向 / 侧向力（N）。Sx：胎面比路面快多少（驱动为正，抱死为负），Sy：接地点侧向速度（右正），
	// Vl：接地点沿轮胎方向的速度。按峰值归一的合成滑移算大小、按滑移方向分配：摩擦圆、越过峰值变滑动都在这里
	inline void Tire(float Sx, float Sy, float Vl, float CapX, float CapY, float PeakA, float SlideLat, float& OutFx, float& OutFy, float& OutS)
	{
		const float Vref = FMath::Max(FMath::Abs(Vl), 3.0f);
		const float Kn = Sx / Vref / PeakKappa;
		const float An = Sy / Vref / PeakA;
		const float S = FMath::Sqrt(Kn * Kn + An * An);
		OutS = S;
		if (S < 1e-4f)
		{
			OutFx = 0.0f;
			OutFy = 0.0f;
			return;
		}
		const float F = Curve(S, FMath::Lerp(SlideLong, SlideLat, (An * An) / (S * S))) / S;
		OutFx = CapX * F * Kn;
		OutFy = -CapY * F * An;
	}
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
	if (HasActorBegunPlay())
	{
		ApplyEngineTorque();
	}
}

void AApexDriftVehiclePawn::ApplyEngineTorque()
{
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetMaxEngineTorque(ApexCar::BaseTorque * Car().Accel);
	}
}

void AApexDriftVehiclePawn::SetSimChassis(bool bOn)
{
	if (bOn == bSimChassis)
	{
		return;
	}
	bSimChassis = bOn;
	AppliedFrontGrip = -1.0f;
	AppliedRearGrip = -1.0f;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetThrottleInput(bOn ? 0.0f : RawThrottle);
		Move->SetBrakeInput(bOn ? 0.0f : RawBrake);
		// Chaos 的自动挡按它自己的轮速（跟着地面转）换挡，看不到后轮空转：漂移时一直卡在一挡红线、油门再大也甩不动。
		// 拟真档改由轮胎模型按后轮实际转速换挡
		Move->SetUseAutomaticGears(!bManualGearbox && !bOn);
	}
	bSimPrevValid = false;
	SimSpin = 0.0f;
	SimCalmT = 0.0f;
	if (bOn)
	{
		EndDrift();
		TangentAssist = 0.0f;
		SimThrottle = RawThrottle;
		SimRearSpeed = FMath::Max(0.0f, GetActorTransform().InverseTransformVectorNoScale(GetVelocity()).X * 0.01f);
	}
	else
	{
		bSimFree = false;
		bSimActive = false;
		SimRPM = 0.0f;
		for (float& S : SimSkid) S = 0.0f;
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
	ApplyEngineTorque();
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

void AApexDriftVehiclePawn::ApplyHandlingSettings(EApexHandlingAssist Assist, bool bManual, float InBaseFOV, bool bTractionControl)
{
	HandlingAssist = Assist;
	bSimTCS = bTractionControl;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexSimNoTCS"))) bSimTCS = false;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexSimTCS"))) bSimTCS = true;
	int32 AssistArg = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ApexAssist="), AssistArg)) HandlingAssist = static_cast<EApexHandlingAssist>(FMath::Clamp(AssistArg, 0, 2));
	bManualGearbox = bManual;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexManual"))) bManualGearbox = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("ApexAuto"))) bManualGearbox = false;
	BaseFOV = FMath::Clamp(InBaseFOV, 68.0f, 105.0f);
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetUseAutomaticGears(!bManualGearbox && !bSimChassis);   // 拟真档的自动挡由轮胎模型按后轮转速换
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
		Move->SetThrottleInput(bSimChassis ? 0.0f : RawThrottle);   // 拟真档的驱动由轮胎模型出
	}
}

void AApexDriftVehiclePawn::SetBrakeInput(float Value)
{
	RawBrake = FMath::Clamp(Value, 0.0f, 1.0f);
	if (bControlsLocked) return;
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetBrakeInput(bSimChassis ? 0.0f : RawBrake);
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
	SimSpin = 0.0f;
	SimCalmT = 0.0f;
	SimRearSpeed = 0.0f;
	SimThrottle = 0.0f;
	bSimPrevValid = false;
	SimAccel = FVector2D::ZeroVector;
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
	// ResetVehicleState 会按默认配置重建轮子和发动机：摩擦回到 3.4/3.9、扭矩回到基础值。
	// 缓存的“已设置”值作废，下一帧重新下发（拟真档一直要 0 摩擦，不作废就永远不会再设，Chaos 轮胎满抓地和轮胎模型打架）
	AppliedFrontGrip = -1.0f;
	AppliedRearGrip = -1.0f;
	ApplyEngineTorque();
	if (UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		Move->SetUseAutomaticGears(!bManualGearbox && !bSimChassis);   // 同理：重建后自动挡开关回到默认
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

	// 拟真档：后轮空转时发动机转速跟着冲高（Chaos 拿不到油门，它的转速只跟车速）
	const float RPM = (bSimActive && SimRPM > 0.0f) ? FMath::Max(SimRPM, Move->GetEngineRotationSpeed()) : Move->GetEngineRotationSpeed();
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
				bool bWheelSlide = false;
				const float SkidNorm = WheelSkid(i, WS, AbsSlipDeg, SpeedKmh, bWheelSlide);
				if (bWheelSlide)
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
	bSimActive = false;
	SetSimChassis(HandlingAssist == EApexHandlingAssist::SimulationRaw && !bControlsLocked && !AutoRun.IsActive());

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
	const bool bPro = (HandlingAssist == EApexHandlingAssist::DriftPro);

	if (HandlingAssist == EApexHandlingAssist::SimulationRaw)
	{
		// 拟真档：整套交给轮胎模型，不进下面的漂移状态机、车身稳定和“倒着走就拧回来”
		UpdateSimPhysics(Dt, SpeedKmh, bGrounded);
		return;
	}
	bSimFree = false;

	// ---- 进入漂移：只由玩家主动触发，撞墙、落地造成的侧滑一律走抓地稳定 ----
	if (bGrounded && bForward && SpeedKmh > 28.0f)
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
	// 高速时前轮略放、后轮收紧：后轴更稳，不打方向就走直线
	const float Fast = FMath::Clamp((SpeedMs - 40.0f) / 40.0f, 0.0f, 1.0f);
	SetAllWheelGrip(FMath::Lerp(1.15f, FMath::Lerp(3.4f, 2.5f, Fast), GripBlend), FMath::Lerp(0.95f, FMath::Lerp(3.9f, 4.6f, Fast), GripBlend));

	// 侧向能力随速度（下压力）增长
	const float LatMax = C.Grip * (16.5f + 5.0f * FMath::Clamp(FMath::Square(SpeedMs / 70.0f), 0.0f, 1.3f)) * (1.0f - 0.15f * RawBrake);

	// ---- 转向 ----
	float SteerDeg = 0.0f;
	{
		// 抓地极限允许的转角：阿克曼角 + 轮胎侧偏余量
		const float V2 = FMath::Max(SpeedMs * SpeedMs, 1.0f);
		const float LimitDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(ApexCar::WheelBaseM * LatMax / V2)) + 5.5f, 6.0f, ApexCar::MaxSteerDeg);
		const float GripSteer = SmoothedSteer * LimitDeg;
		// 漂移时前轮对准速度方向（自动反打），玩家输入做小幅修正
		const float DriftSteer = FMath::Clamp(Beta + RawSteering * 7.0f, -ApexCar::MaxSteerDeg, ApexCar::MaxSteerDeg);
		SteerDeg = FMath::Lerp(DriftSteer, GripSteer, GripBlend);
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

	// ---- 纵向：抓地态的加速/刹车补偿，速度才跟得上 ----
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

	if (bDriftActive)
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
	SimSpin = 0.0f;
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

// 拟真档：轮胎物理。四个轮胎各自按接地点速度算侧偏角和滑移率，按悬架载荷 + 载荷转移算附着，
// 纵向和侧向共用一个摩擦圆，越过峰值就变成滑动摩擦；后轮另有转速状态：油门大于附着就空转（甩尾），
// 手刹直接抱死。车身的力和横摆全部来自这四个轮胎，抓地、漂移、横过来、倒着滑是同一套公式，
// 没有任何“把车头摆正”的控制：甩出去要自己反打、收油救回来，倒着滑也能一直倒着滑。
void AApexDriftVehiclePawn::UpdateSimPhysics(float Dt, float SpeedKmh, bool bGrounded)
{
	using namespace ApexSim;
	UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	USkeletalMeshComponent* Body = GetMesh();
	const FApexCarInfo& C = Car();
	bSimActive = true;
	SetAllWheelGrip(ChaosGrip, ChaosGrip);

	const FTransform Xf = GetActorTransform();
	const FVector Up = GetActorUpVector();
	const FVector VelW = GetVelocity();
	const FVector VL = Xf.InverseTransformVectorNoScale(VelW) * 0.01f;   // 车身坐标速度 m/s
	FVector AngVel = Body->GetPhysicsAngularVelocityInDegrees();
	const float R = FMath::DegreesToRadians(FVector::DotProduct(AngVel, Up));   // 横摆角速度 rad/s，向右转为正
	const float Mass = FMath::Max(500.0f, Body->GetMass());
	const float Iz = Mass * YawK / FMath::Clamp(C.Agility, 0.6f, 1.6f);
	const float V = FVector2D(VL.X, VL.Y).Size();
	const int32 Gear = Move->GetCurrentGear();

	// 轮位（相对质心）
	float Px[4], Py[4];
	for (int32 i = 0; i < 4; ++i)
	{
		Px[i] = ApexCar::WheelRest[i].X * 0.01f - CoMX;
		Py[i] = ApexCar::WheelRest[i].Y * 0.01f;
	}
	const float A = Px[0], B = -Px[2], L = A + B;
	const float TrackF = Py[1] - Py[0], TrackR = Py[3] - Py[2];

	// 键盘油门是 0/1：0.25 秒推满，松得更快（只是输入平滑，不限制出力）
	SimThrottle = FMath::FInterpConstantTo(SimThrottle, RawThrottle, Dt, RawThrottle > SimThrottle ? 4.0f : 8.0f);

	// 车身坐标加速度（含转动项），低通后算载荷转移
	if (bSimPrevValid)
	{
		const float Ax = (VL.X - SimPrevVel.X) / Dt - R * VL.Y;
		const float Ay = (VL.Y - SimPrevVel.Y) / Dt + R * VL.X;
		const float K = 1.0f - FMath::Exp(-Dt / 0.07f);
		SimAccel.X += (FMath::Clamp(Ax, -25.0f, 25.0f) - SimAccel.X) * K;
		SimAccel.Y += (FMath::Clamp(Ay, -25.0f, 25.0f) - SimAccel.Y) * K;
	}
	SimPrevVel = VL;
	bSimPrevValid = true;

	// ---- 只统计不干预：车头相对行进方向的角度、是否横着滑、转满一圈记 360 ----
	{
		const float Psi = V > 2.0f ? FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(VelW.Y, VelW.X)), GetActorRotation().Yaw) : 0.0f;
		const float AbsPsi = FMath::Abs(Psi);
		if (bSimFree) SimSpin += FMath::FindDeltaAngleDegrees(SimPsi, Psi);
		SimPsi = Psi;
		const bool bReversing = Gear < 0 && VL.X < 0.5f;   // 挂倒挡倒车不算滑
		if (V > 4.0f && AbsPsi > (bSimFree ? 10.0f : 22.0f) && !bReversing)
		{
			bSimFree = true;
			SimCalmT = 0.0f;
		}
		else if (bSimFree)
		{
			SimCalmT += Dt;
			if (SimCalmT > 0.4f || V < 3.0f || bReversing)
			{
				bSimFree = false;
				SimSpin = 0.0f;
			}
		}
		if (FMath::Abs(SimSpin) > 325.0f && AbsPsi < 45.0f)
		{
			SimSpin -= FMath::Sign(SimSpin) * 360.0f;
			if (DriftScoreComp) DriftScoreComp->AwardSpin();
		}
	}

	// ---- 方向：只按玩家输入。车速越高可用转角越小（键盘满舵也不至于一下打过轮胎极限）；
	//      车已经横过来时，往前轴实际行进的方向打（反打）可以打到前轮对准行进方向再多一点，但绝不自己反打 ----
	float SteerDeg;
	{
		const float MaxWheel = 38.0f;
		const float Vs = FMath::Max(V, 1.0f);
		// 余量：低速 7°（前轮峰值侧偏角 8.5° 以内），高速收到 3°，满舵时前轮不到峰值、后轮不会先饱和；
		// 重刹时前轴载荷大、后轴轻，余量再收一点（只是方向打不了那么多，不会替你转车头）
		const float Margin = (V < 70.0f ? FMath::Lerp(7.0f, 3.0f, FMath::Clamp((V - 25.0f) / 45.0f, 0.0f, 1.0f)) : FMath::Lerp(3.0f, 2.0f, FMath::Clamp((V - 70.0f) / 25.0f, 0.0f, 1.0f))) * FMath::Lerp(1.0f, 0.6f, FMath::Clamp((Gear < 0 ? RawThrottle : RawBrake) * FMath::Clamp((V - 15.0f) / 25.0f, 0.0f, 1.0f), 0.0f, 1.0f));
		float Limit = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(L * MuLatF * C.Grip * G / (Vs * Vs))) + Margin, 1.5f, MaxWheel);
		if (VL.X > 0.5f)
		{
			const float FrontAng = FMath::RadiansToDegrees(FMath::Atan2(VL.Y + A * R, VL.X));   // 前轴往哪走（相对车头，右正）
			if (SmoothedSteer * FrontAng > 0.0f) Limit = FMath::Max(Limit, FMath::Min(FMath::Abs(FrontAng) + 8.0f, MaxWheel));
		}
		else
		{
			Limit = MaxWheel * 0.8f;   // 倒车、倒着滑：方向随便打
		}
		SteerDeg = SmoothedSteer * Limit;
		SimSteerLimit = Limit;
		Move->SetSteeringInput(FMath::Clamp(SteerDeg / ApexCar::MaxSteerDeg, -1.0f, 1.0f));
	}

	// ---- 传动：挡位决定红线时的胎面速度；倒挡时刹车键是油门、油门键是刹车 ----
	// Chaos 拿不到刹车输入就不会自己挂倒挡：停住后按住刹车挂倒挡，倒挡里按油门挂回一挡
	// 同理空挡起步也不会自己挂一挡（只在真的停在空挡、没有换挡进行中时挂，不打断自动升挡）
	// （手动挡也一样：停住按住刹车挂倒挡，和原来 Chaos 的行为一致）
	{
		if (Gear >= 0 && FMath::Abs(VL.X) < 1.0f && RawBrake > 0.3f && RawThrottle < 0.1f) Move->SetTargetGear(-1, true);
		else if (Gear < 0 && RawThrottle > 0.3f && VL.X > -1.0f) Move->SetTargetGear(1, true);
		else if (Gear == 0 && Move->GetTargetGear() == 0 && RawThrottle > 0.1f && RawBrake < 0.3f && (!bManualGearbox || FMath::Abs(VL.X) < 1.5f)) Move->SetTargetGear(1, true);   // 手动挡只在停住时（和 Chaos 原来一样）
	}
	// 自动挡：按后轮实际转速（空转时比车速快）升降挡，和真车的自动挡一样漂移时会升挡
	SimShiftT += Dt;
	if (!bManualGearbox && Gear > 0 && Move->TransmissionSetup.ForwardGearRatios.IsValidIndex(Gear - 1))
	{
		const TArray<float>& Gr = Move->TransmissionSetup.ForwardGearRatios;
		const float RpmNow = FMath::Abs(SimRearSpeed) * Gr[Gear - 1] * Move->TransmissionSetup.FinalRatio * 60.0f / (UE_TWO_PI * RearRadius);
		if (SimShiftT > 0.35f && RpmNow > Move->TransmissionSetup.ChangeUpRPM && Gear < Gr.Num())
		{
			Move->SetTargetGear(Gear + 1, false);
			SimShiftT = 0.0f;
		}
		else if (SimShiftT > 0.6f && SimHandT > 0.5f && Gear > 1 && RpmNow < Move->TransmissionSetup.ChangeDownRPM && RpmNow * Gr[Gear - 2] / Gr[Gear - 1] < Move->GetEngineMaxRotationSpeed() * 0.9f)
		{
			Move->SetTargetGear(Gear - 1, false);
			SimShiftT = 0.0f;
		}
	}
	float Ratio = 0.0f;
	if (Gear > 0 && Move->TransmissionSetup.ForwardGearRatios.IsValidIndex(Gear - 1)) Ratio = Move->TransmissionSetup.ForwardGearRatios[Gear - 1] * Move->TransmissionSetup.FinalRatio;
	else if (Gear < 0 && Move->TransmissionSetup.ReverseGearRatios.Num() > 0) Ratio = Move->TransmissionSetup.ReverseGearRatios[0] * Move->TransmissionSetup.FinalRatio;
	const float RedLine = Move->GetEngineMaxRotationSpeed();
	const float ToRPM = Ratio * 60.0f / (UE_TWO_PI * RearRadius);              // 胎面速度 m/s → 发动机转速
	const float WsMax = Ratio > 0.0f ? RedLine / ToRPM : 0.0f;                 // 红线对应的胎面速度
	const float ThrIn = Gear < 0 ? RawBrake : SimThrottle;
	const float BrkIn = Gear < 0 ? RawThrottle : RawBrake;
	const float BrakeF = Mass * ApexSim::BrakeAccel * BrkIn;   // 自动漂移的匿名命名空间里也有 BrakeAccel，Mac 的 unity 编译会撞名，写全
	auto DriveForce = [&](float Ws) -> float
	{
		if (Ratio <= 0.0f) return 0.0f;
		const float Dir = Gear > 0 ? 1.0f : -1.0f;
		const float Wd = Ws * Dir;                                           // 顺着挡位方向的胎面速度
		float F;
		if (ThrIn > 0.02f)
		{
			if (Gear > 0)
			{
				// 与其他档同一条全油门加速度曲线（按后轮转速取值，空转越快劲越小）+ 抵消风阻和车身阻尼
				// （实测阻力约 0.95v² N：只按 Chaos 风阻系数算的一半时，24 秒只到 329km/h，其他档约 367）
				F = Mass * TargetAccelAt(FMath::Max(Wd, 0.0f) * 3.6f, C.Accel, C.TopKmh) + 0.95f * FMath::Square(FMath::Max(VL.X, 0.0f));
				if (bManualGearbox) F *= FMath::Clamp(Wd * ToRPM / 3600.0f, 0.25f, 1.0f);   // 手动挡：转速太低没劲
				F *= FMath::Clamp((WsMax - Wd) / 1.5f, 0.0f, 1.0f);                       // 红线断油
			}
			else
			{
				F = Mass * 4.5f * FMath::Clamp((12.0f - Wd) / 1.5f, 0.0f, 1.0f);           // 倒车最快约 43km/h
			}
			F *= ThrIn;
		}
		else
		{
			F = -Mass * 0.9f * FMath::Sqrt(Ratio / 11.16f) * FMath::Clamp(Wd / 8.0f, -1.0f, 1.0f);   // 松油：发动机制动，挡越高越弱
		}
		return F * Dir;
	};

	// ---- 载荷：车重 + 下压按静态比例分配，再加纵向 / 横向载荷转移；离地的轮子不出力 ----
	// （Chaos 的 SpringForce 加起来只有车重的四成左右、落地瞬间又冲到两倍，单位对不上，只用它的接地标志）
	bool In[4];
	int32 NIn = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		In[i] = Move->GetWheelState(i).bInContact;
		NIn += In[i] ? 1 : 0;
	}
	if (NIn == 0)
	{
		// 四轮离地：后轮照样被油门带着转、被刹车 / 手刹停住，车身不受轮胎力
		SimRearSpeed = bRawHandbrake ? 0.0f : SimRearSpeed + Dt * DriveForce(SimRearSpeed) / RearInertia;
		SimRPM = Ratio > 0.0f ? FMath::Clamp(FMath::Abs(SimRearSpeed) * ToRPM, 900.0f, RedLine) : 0.0f;
		for (float& S : SimSkid) S = 0.0f;
		return;
	}
	const float Down = FMath::Min(0.0009f * V * V, 6.0f);
	const float N = Mass * (G + Down) * (NIn >= 3 ? 1.0f : (NIn == 2 ? 0.85f : 0.5f));
	const float Scale = N / (Mass * G);
	const float DLong = Mass * SimAccel.X * CoMH / L * Scale;                   // 加速时后轴加载
	const float DLatF = Mass * SimAccel.Y * CoMH / TrackF * RollFront * Scale;  // 右转时左侧（外侧）加载
	const float DLatR = Mass * SimAccel.Y * CoMH / TrackR * (1.0f - RollFront) * Scale;
	float Fz[4] = {
		N * B / L * 0.5f - DLong * 0.5f + DLatF,
		N * B / L * 0.5f - DLong * 0.5f - DLatF,
		N * A / L * 0.5f + DLong * 0.5f + DLatR,
		N * A / L * 0.5f + DLong * 0.5f - DLatR };
	float Masked = 0.0f;
	for (int32 i = 0; i < 4; ++i)
	{
		Fz[i] = In[i] ? FMath::Max(Fz[i], 0.02f * N) : 0.0f;
		Masked += Fz[i];
	}
	for (int32 i = 0; i < 4; ++i) Fz[i] *= N / FMath::Max(Masked, 1.0f);

	auto Caps = [&](int32 i, float MuX, float MuY, float& OutCapX, float& OutCapY)
	{
		const float Fz0 = Mass * G * (i < 2 ? B : A) / L * 0.5f;
		const float Sens = FMath::Clamp(1.0f - LoadSens * (Fz[i] / Fz0 - 1.0f), 0.75f, 1.15f);
		OutCapX = MuX * C.Grip * Sens * Fz[i];
		OutCapY = MuY * C.Grip * Sens * Fz[i];
	};

	float Fx = 0.0f, Fy = 0.0f, Mz = 0.0f;
	auto AddWheel = [&](int32 i, float FLong, float FLat, float Cs, float Sn)
	{
		const float Fxw = FLong * Cs - FLat * Sn;
		const float Fyw = FLong * Sn + FLat * Cs;
		Fx += Fxw;
		Fy += Fyw;
		Mz += Px[i] * Fyw - Py[i] * Fxw;
	};
	// 数值稳定：一帧内一个轮子的力不超过把它接地点相对速度刹停所需（低速、静止时不抖）
	auto StopClamp = [&](float F, float Vrel, float Lever) -> float
	{
		const float Lim = 0.22f * FMath::Abs(Vrel) / (Dt * (1.0f / Mass + Lever * Lever / Iz));
		return FMath::Clamp(F, -Lim, Lim);
	};

	// ---- 前轮：自由滚动，带 ABS 的刹车按摩擦圆分走纵向附着 ----
	const float Delta = FMath::DegreesToRadians(SteerDeg);
	const float Cd = FMath::Cos(Delta), Sd = FMath::Sin(Delta);
	for (int32 i = 0; i < 2; ++i)
	{
		SimSkid[i] = 0.0f;
		if (Fz[i] <= 0.0f) continue;
		const float Wx = VL.X - R * Py[i], Wy = VL.Y + R * Px[i];
		const float Vl = Wx * Cd + Wy * Sd, Vt = -Wx * Sd + Wy * Cd;
		float CapX, CapY, FLong, FLat, S;
		Caps(i, MuLongF, MuLatF, CapX, CapY);
		ApexSim::Tire(0.0f, Vt, Vl, CapX, CapY, PeakAlphaF, Tune().SlideF, FLong, FLat, S);
		if (BrakeF > 0.0f && FMath::Abs(Vl) > 0.05f)
		{
			const float U = FMath::Min(BrakeF * BrakeFront * 0.5f / FMath::Max(CapX, 1.0f), 0.95f);
			FLong = StopClamp(-FMath::Sign(Vl) * U * CapX, Vl, Py[i]);
			FLat *= FMath::Sqrt(1.0f - U * U);
		}
		FLat = StopClamp(FLat, Vt, Px[i]);
		SimSkid[i] = FMath::Clamp((S - 1.2f) / 2.0f, 0.0f, 1.0f);
		AddWheel(i, FLong, FLat, Cd, Sd);
	}

	// ---- 后轮：两轮共用一个胎面速度（限滑差速），按发动机、刹车和地面反力积分；手刹抱死 ----
	{
		float VlR[2], VtR[2], CapX[2], CapY[2];
		for (int32 j = 0; j < 2; ++j)
		{
			const int32 i = 2 + j;
			VlR[j] = VL.X - R * Py[i];
			VtR[j] = VL.Y + R * Px[i];
			Caps(i, MuLongR, MuLatR, CapX[j], CapY[j]);
		}
		auto RearLong = [&](float Ws, float* OutFl, float* OutFt, float* OutS) -> float
		{
			float Sum2 = 0.0f;
			for (int32 j = 0; j < 2; ++j)
			{
				float Fl = 0.0f, Ft = 0.0f, S = 0.0f;
				if (Fz[2 + j] > 0.0f) ApexSim::Tire(Ws - VlR[j], VtR[j], VlR[j], CapX[j], CapY[j], PeakAlphaR, Tune().SlideR, Fl, Ft, S);
				if (OutFl) { OutFl[j] = Fl; OutFt[j] = Ft; OutS[j] = S; }
				Sum2 += Fl;
			}
			return Sum2;
		};
		const int32 NSub = FMath::Clamp(FMath::CeilToInt(Dt / 0.0015f), 4, 16);
		const float H = Dt / NSub;
		const float RearBrake = FMath::Min(BrakeF * (1.0f - BrakeFront), RearBrakeCap * (CapX[0] + CapX[1]) * FMath::Sqrt(FMath::Max(0.1f, 1.0f - FMath::Square(SimRearLatUse))));
		// 牵引力控制（和地平线默认一样）：只限油门，从不转车头。按后轮实际的侧偏角算附着圆里还剩多少纵向，
		// 驱动力只给到合成滑移为峰值 90% 的那么多：直线几乎全给，弯里后轮越接近侧滑给得越少，后轮就不会被油门踩空把车尾甩出去。
		// （按侧向“力”算不行：轮胎越过峰值后力反而变小，看着还有余量，实际已经在滑）
		// 后轮已经横着滑（压路肩、蹭墙、收油甩尾）时也不放开，只许一点空转（TcsSlip × 峰值滑移率）：
		// 第一版车身一过 12° 就整个放开，正好在最需要的时候失效，按着油门一滑就转圈。
		// 只有拉着手刹时不限（手刹甩尾、360 照做；松手刹后立刻恢复，键盘乱点手刹再按油门的打转从 8 次 / 90 秒降到 6 次）；想用油门漂移就在设置里关掉
		SimHandT = bRawHandbrake ? 0.0f : SimHandT + Dt;
		const bool bTCS = Tune().bOldTcs ? (bSimTCS && !bSimFree && FMath::Abs(SimPsi) < 12.0f && SimHandT > 0.6f && Gear > 0) : (bSimTCS && SimHandT > Tune().TcsHand && Gear > 0);
		float TcsCap = 1e9f;
		if (bTCS)
		{
			TcsCap = 0.0f;
			constexpr float Sc = 0.9f;
			for (int32 j = 0; j < 2; ++j)
			{
				if (Fz[2 + j] <= 0.0f) continue;
				const float An = FMath::Abs(VtR[j]) / FMath::Max(FMath::Abs(VlR[j]), 3.0f) / PeakAlphaR;
				const float Kn = FMath::Max(FMath::Sqrt(FMath::Max(0.0f, Sc * Sc - An * An)), Tune().bOldTcs ? 0.0f : Tune().TcsSlip);
				const float S = FMath::Sqrt(Kn * Kn + An * An);
				TcsCap += CapX[j] * ApexSim::Curve(S, FMath::Lerp(SlideLong, Tune().SlideR, An * An / (S * S))) * Kn / S;
			}
		}
		SimDbgTcs = bTCS ? TcsCap : -1.0f;
		// 关掉牵引力控制时后轮空转的上限：车身正着时只许中等空转，车尾横得越多许得越多（油门照样能甩尾、保持漂移）。
		// 不限的话后轮会转到比车速快几倍（驱动力按胎面速度查表、升挡也不减力），侧向抓地全没了，直线全油门都会转圈。
		// 拉手刹起的横滑（玩家自己要漂、要原地转圈）许得多；出弯给油、被撞歪这种不是自己要的横滑许得少，能救回来
		if (bRawHandbrake) bSimHandSlide = true;
		else if (!bSimFree && SimHandT > 1.0f) bSimHandSlide = false;
		const float SpinGain = bSimHandSlide ? Tune().SpinGainHand : Tune().SpinGain;
		float WsLim = 1e9f;
		if (!bTCS && Gear > 0 && !bRawHandbrake && Tune().SpinBase < 50.0f)
		{
			WsLim = -1e9f;
			for (int32 j = 0; j < 2; ++j)
			{
				if (Fz[2 + j] <= 0.0f) continue;
				const float Vref = FMath::Max(FMath::Abs(VlR[j]), 3.0f);
				const float An = FMath::Abs(VtR[j]) / Vref / PeakAlphaR;
				WsLim = FMath::Max(WsLim, VlR[j] + (Tune().SpinBase + SpinGain * An) * PeakKappa * Vref);
			}
			if (WsLim < -1e8f) WsLim = 1e9f;
		}
		float Ws = SimRearSpeed;
		float AccFl[2] = { 0.0f, 0.0f }, AccFt[2] = { 0.0f, 0.0f }, MaxS[2] = { 0.0f, 0.0f };
		for (int32 k = 0; k < NSub; ++k)
		{
			if (bRawHandbrake) Ws = 0.0f;
			float Fl[2], Ft[2], S[2];
			const float FlSum = RearLong(Ws, Fl, Ft, S);
			for (int32 j = 0; j < 2; ++j)
			{
				AccFl[j] += Fl[j] / NSub;
				AccFt[j] += Ft[j] / NSub;
				MaxS[j] = FMath::Max(MaxS[j], S[j]);
			}
			if (bRawHandbrake) continue;
			// 半隐式：按当地刚度放大惯量，低速时轮胎很“硬”也不会来回震荡
			const float Stiff = FMath::Max(0.0f, (RearLong(Ws + 0.05f, nullptr, nullptr, nullptr) - FlSum) / 0.05f);
			float Drive = FMath::Min(DriveForce(Ws), TcsCap);
			if (Ws >= WsLim && Drive > 0.0f) Drive = 0.0f;   // 超过空转上限就断油（像断油限速器），让地面把轮子拖回来
			float Wn = Ws + H * (Drive - FlSum) / (RearInertia + H * Stiff);
			if (RearBrake > 0.0f)
			{
				const float Dec = H * RearBrake / RearInertia;
				Wn = Wn > 0.0f ? FMath::Max(0.0f, Wn - Dec) : FMath::Min(0.0f, Wn + Dec);   // 刹车只让轮子停下，不会倒转
			}
			if (Ws < WsLim && Wn > WsLim) Wn = WsLim;
			Ws = Wn;
		}
		SimRearSpeed = bRawHandbrake ? 0.0f : Ws;
		SimRearLatUse = FMath::Clamp(FMath::Max(FMath::Abs(AccFt[0]) / FMath::Max(CapY[0], 1.0f), FMath::Abs(AccFt[1]) / FMath::Max(CapY[1], 1.0f)), 0.0f, 1.0f);
		// 差速器左右等分扭矩：不按载荷分配。按载荷分时，稍一转弯外侧后轮载荷大、推得更狠，
		// 车会越转越急（第一版 310km/h 直线自己打转就是这个）
		if (!bRawHandbrake && Fz[2] > 0.0f && Fz[3] > 0.0f)
		{
			AccFl[0] = AccFl[1] = 0.5f * (AccFl[0] + AccFl[1]);
		}
		for (int32 j = 0; j < 2; ++j)
		{
			const int32 i = 2 + j;
			SimSkid[i] = Fz[i] > 0.0f ? (bRawHandbrake && V > 5.0f ? 1.0f : FMath::Clamp((MaxS[j] - 1.2f) / 2.0f, 0.0f, 1.0f)) : 0.0f;
			if (Fz[i] <= 0.0f) continue;
			AddWheel(i, bRawHandbrake ? StopClamp(AccFl[j], VlR[j], Py[i]) : AccFl[j], StopClamp(AccFt[j], VtR[j], Px[i]), 1.0f, 0.0f);
		}
	}
	SimRPM = Ratio > 0.0f ? FMath::Clamp(FMath::Abs(SimRearSpeed) * ToRPM, 900.0f, RedLine) : 0.0f;

	static const bool bSimDbg = FParse::Param(FCommandLine::Get(), TEXT("ApexBenchSim")) || FParse::Param(FCommandLine::Get(), TEXT("ApexSimDebug"));
	if (bSimDbg)
	{
		SimDebug = FString::Printf(TEXT("N/mg=%.2f Fz=%.0f/%.0f/%.0f/%.0f Fx=%.0f Fy=%.0f Mz=%.0f Ws=%.1f vx=%.1f vy=%.1f 驱动=%.0f 轮=%d ax=%.1f ay=%.1f 转角=%.1f 后侧用=%.2f TCS=%.0f"),
			N / (Mass * G), Fz[0], Fz[1], Fz[2], Fz[3], Fx, Fy, Mz, SimRearSpeed, VL.X, VL.Y, DriveForce(SimRearSpeed), NIn, SimAccel.X, SimAccel.Y, SteerDeg, SimRearLatUse, SimDbgTcs);
	}
	// ---- 施力：平移力加在质心，横摆按轮胎力矩积分（不覆盖碰撞等带来的角速度） ----
	static const bool bNoTire = FParse::Param(FCommandLine::Get(), TEXT("ApexSimNoTire"));   // 调试：只看 Chaos 自己出多少力
	if (!bNoTire)
	{
		Body->AddForce(Xf.TransformVectorNoScale(FVector(Fx, Fy, 0.0f)) * 100.0f);
		AngVel += Up * FMath::RadiansToDegrees(Mz / Iz * Dt);
		Body->SetPhysicsAngularVelocityInDegrees(AngVel);
	}
	// 高速下压（与其他档一致），过坡不发飘
	Body->AddForce(-Up * FMath::Min(0.0009f * V * V, 6.0f) * 100.0f * Mass);
}

float AApexDriftVehiclePawn::WheelSkid(int32 Wheel, const FWheelStatus& WS, float AbsSlipDeg, float SpeedKmh, bool& bOutSliding) const
{
	if (bSimActive)
	{
		const float S = SimSkid[FMath::Clamp(Wheel, 0, 3)];
		bOutSliding = WS.bInContact && S > 0.14f && SpeedKmh > 8.0f;
		return S;
	}
	const float SlipBoost = (Wheel >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f) ? FMath::Clamp((AbsSlipDeg - 11.0f) / 28.0f, 0.0f, 1.0f) : 0.0f;
	const float SkidNorm = FMath::Max(FMath::Clamp((WS.SkidMagnitude + WS.SlipMagnitude * 0.5f) / 480.0f, 0.0f, 1.0f), SlipBoost);
	bOutSliding = WS.bInContact && (WS.bIsSkidding || WS.bIsSlipping || (bRawHandbrake && Wheel >= 2 && SpeedKmh > 20.0f) || (Wheel >= 2 && AbsSlipDeg > 13.0f && SpeedKmh > 25.0f)) && SkidNorm > 0.14f;
	return SkidNorm;
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
		bool bSliding = false;
		const float SkidNorm = WheelSkid(i, WS, AbsSlipDeg, SpeedKmh, bSliding);

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
