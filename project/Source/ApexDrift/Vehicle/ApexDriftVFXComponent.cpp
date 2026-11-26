#include "Vehicle/ApexDriftVFXComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FTransform GHidden(FQuat::Identity, FVector::ZeroVector, FVector(0.0001f));

	// 面片：X=屏幕右，Y=屏幕下，Z=朝镜头
	FTransform BillboardXf(const FVector& Pos, const FVector& X, const FVector& Y, const FVector& Z, float SizeX, float SizeY)
	{
		FTransform T(FRotationMatrix::MakeFromXY(X, Y).ToQuat(), Pos, FVector(SizeX * 0.01f, SizeY * 0.01f, 1.0f));
		return T;
	}
}

UApexDriftVFXComponent::UApexDriftVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UInstancedStaticMeshComponent* UApexDriftVFXComponent::MakeBatch(const TCHAR* Name, const TCHAR* MaterialPath, int32 NumInstances, int32 NumCustom)
{
	AActor* Owner = GetOwner();
	UStaticMesh* Quad = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/VFX/SM_VfxQuad.SM_VfxQuad"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!Owner || !Quad || !Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApexVFX] missing quad or material %s"), MaterialPath);
		return nullptr;
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Owner, Name);
	C->SetMobility(EComponentMobility::Movable);
	C->SetUsingAbsoluteLocation(true);
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	C->SetStaticMesh(Quad);
	C->SetMaterial(0, Mat);
	C->NumCustomDataFloats = NumCustom;
	C->SetCastShadow(false);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->bAffectDistanceFieldLighting = false;
	C->bAffectDynamicIndirectLighting = false;
	C->bVisibleInRayTracing = false;
	C->bNeverDistanceCull = true;
	C->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	C->RegisterComponent();
	C->SetWorldTransform(FTransform::Identity);
	TArray<FTransform> Xf;
	Xf.Init(GHidden, NumInstances);
	C->AddInstances(Xf, false, true);
	return C;
}

void UApexDriftVFXComponent::BeginPlay()
{
	Super::BeginPlay();
	SmokePool.SetNum(MaxSmokePuffs);
	AdditivePool.SetNum(MaxAdditiveQuads);
	SmokeXf.Init(GHidden, MaxSmokePuffs);
	AddXf.Init(GHidden, MaxAdditiveQuads);
	SmokeData.Init(0.0f, MaxSmokePuffs * 3);
	AddData.Init(0.0f, MaxAdditiveQuads * 4);
	SmokeBatch = MakeBatch(TEXT("SmokeBatch"), TEXT("/Game/VFX/M_TireSmoke.M_TireSmoke"), MaxSmokePuffs, 3);
	AddBatch = MakeBatch(TEXT("AdditiveBatch"), TEXT("/Game/VFX/M_VfxAdditive.M_VfxAdditive"), MaxAdditiveQuads, 4);
	bInitialized = true;
}

void UApexDriftVFXComponent::EmitTireSmoke(const FVector& WorldPos, const FVector& InheritedVel, float Intensity)
{
	if (!bInitialized) return;
	++TotalSmokeEmitted;
	FSmokePuff& P = SmokePool[SmokeCursor];
	SmokeCursor = (SmokeCursor + 1) % MaxSmokePuffs;

	const FVector Jitter(FMath::FRandRange(-16.0f, 16.0f), FMath::FRandRange(-16.0f, 16.0f), FMath::FRandRange(10.0f, 26.0f));
	P.Pos = WorldPos + Jitter;
	// 多带一点车速，烟才会先裹着后轮翻卷再被甩到车后，追尾视角里看得见
	P.Vel = InheritedVel * FMath::FRandRange(0.42f, 0.66f) + FVector(FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(-70.0f, 70.0f), FMath::FRandRange(70.0f, 150.0f));
	P.Age = 0.0f;
	P.MaxAge = FMath::FRandRange(1.55f, 2.35f);
	P.StartSize = FMath::FRandRange(95.0f, 135.0f);
	P.EndSize = FMath::FRandRange(290.0f, 410.0f) * FMath::Lerp(0.65f, 1.15f, Intensity);
	P.Intensity = FMath::Clamp(Intensity, 0.25f, 1.0f);
	// 贴图带顶光，滚转只给小范围
	P.RollAngle = FMath::FRandRange(-0.7f, 0.7f);
	P.RollSpeed = FMath::FRandRange(-0.35f, 0.35f);
	P.Frame = FMath::RandRange(0, 15);
	P.bAlive = true;
}

void UApexDriftVFXComponent::EmitExhaustFlame(const FVector& WorldPos, const FVector& BackwardDir)
{
	if (!bInitialized) return;
	for (int32 i = 0; i < 3; ++i)
	{
		FAdditiveQuad& Q = AdditivePool[AddCursor];
		AddCursor = (AddCursor + 1) % MaxAdditiveQuads;
		Q.Pos = WorldPos + BackwardDir * (i * 14.0f);
		Q.Vel = BackwardDir * FMath::FRandRange(420.0f, 680.0f);
		Q.Age = 0.0f;
		Q.MaxAge = FMath::FRandRange(0.11f, 0.18f);
		Q.Size = FMath::FRandRange(15.0f, 24.0f);
		Q.Color = (i == 0) ? FLinearColor(0.35f, 0.78f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.52f, 0.12f, 1.0f);
		Q.bStreak = false;
		Q.bAlive = true;
	}
}

void UApexDriftVFXComponent::EmitImpactSparks(const FVector& WorldPos, const FVector& SurfaceNormal, float Intensity)
{
	if (!bInitialized) return;
	const int32 Count = FMath::Clamp(FMath::RoundToInt(Intensity * 14.0f), 6, 18);
	for (int32 i = 0; i < Count; ++i)
	{
		FAdditiveQuad& Q = AdditivePool[AddCursor];
		AddCursor = (AddCursor + 1) % MaxAdditiveQuads;
		const FVector RandDir = (SurfaceNormal + FVector(FMath::FRandRange(-0.8f, 0.8f), FMath::FRandRange(-0.8f, 0.8f), FMath::FRandRange(0.1f, 0.9f))).GetSafeNormal();
		Q.Pos = WorldPos;
		Q.Vel = RandDir * FMath::FRandRange(450.0f, 1100.0f);
		Q.Age = 0.0f;
		Q.MaxAge = FMath::FRandRange(0.28f, 0.55f);
		Q.Size = FMath::FRandRange(6.0f, 10.0f);
		Q.Color = FLinearColor(1.0f, 0.66f, 0.18f, 1.0f);
		Q.bStreak = true;
		Q.bAlive = true;
	}
}

void UApexDriftVFXComponent::UpdateParticles(float DeltaSeconds, const FVector& CameraLocation)
{
	if (!bInitialized) return;

	bool bAnySmoke = false;
	for (int32 i = 0; i < MaxSmokePuffs; ++i)
	{
		FSmokePuff& P = SmokePool[i];
		if (P.bAlive)
		{
			P.Age += DeltaSeconds;
			if (P.Age >= P.MaxAge) P.bAlive = false;
		}
		if (!P.bAlive)
		{
			SmokeXf[i] = GHidden;
			SmokeData[i * 3] = 0.0f;
			continue;
		}
		bAnySmoke = true;
		const float LifeT = P.Age / P.MaxAge;
		P.Pos += P.Vel * DeltaSeconds;
		P.Vel *= FMath::Max(0.0f, 1.0f - DeltaSeconds * 1.9f);
		P.Vel.Z += 35.0f * DeltaSeconds; // 热浮力
		P.RollAngle += P.RollSpeed * DeltaSeconds;

		const float Size = FMath::Lerp(P.StartSize, P.EndSize, FMath::Sqrt(LifeT));
		const float FadeIn = FMath::Clamp(LifeT / 0.035f, 0.0f, 1.0f);
		const float FadeOut = FMath::Pow(FMath::Clamp(1.0f - LifeT, 0.0f, 1.0f), 1.35f);

		const FVector Z = (CameraLocation - P.Pos).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector WorldUp = FMath::Abs(Z.Z) < 0.95f ? FVector::UpVector : FVector::ForwardVector;
		const FVector BaseX = FVector::CrossProduct(Z, WorldUp).GetSafeNormal();   // 屏幕右
		const FVector BaseY = FVector::CrossProduct(Z, BaseX).GetSafeNormal();     // 屏幕下
		const float CS = FMath::Cos(P.RollAngle), SN = FMath::Sin(P.RollAngle);
		const FVector X = BaseX * CS + BaseY * SN;
		const FVector Y = BaseY * CS - BaseX * SN;

		SmokeXf[i] = BillboardXf(P.Pos, X, Y, Z, Size, Size);
		// 镜头扎进烟里时淡出，免得糊屏
		const float CamFade = FMath::Clamp((FVector::Dist(CameraLocation, P.Pos) - 120.0f) / 260.0f, 0.0f, 1.0f);
		SmokeData[i * 3 + 0] = FadeIn * FadeOut * P.Intensity * 0.70f * CamFade;
		SmokeData[i * 3 + 1] = LifeT;
		SmokeData[i * 3 + 2] = static_cast<float>(P.Frame);
	}
	if (SmokeBatch && (bAnySmoke || bHadSmokeLastFrame))
	{
		SmokeBatch->BatchUpdateInstancesTransforms(0, SmokeXf, true, false, true);
		SmokeBatch->SetCustomData(0, MaxSmokePuffs - 1, SmokeData, false);
		bHadSmokeLastFrame = bAnySmoke;
	}

	bool bAnyAdd = false;
	for (int32 i = 0; i < MaxAdditiveQuads; ++i)
	{
		FAdditiveQuad& Q = AdditivePool[i];
		if (Q.bAlive)
		{
			Q.Age += DeltaSeconds;
			if (Q.Age >= Q.MaxAge) Q.bAlive = false;
		}
		if (!Q.bAlive)
		{
			AddXf[i] = GHidden;
			AddData[i * 4 + 3] = 0.0f;
			continue;
		}
		bAnyAdd = true;
		if (Q.bStreak) Q.Vel.Z -= 980.0f * DeltaSeconds;
		Q.Pos += Q.Vel * DeltaSeconds;

		const float Fade = FMath::Clamp(1.0f - Q.Age / Q.MaxAge, 0.0f, 1.0f);
		const FVector Z = (CameraLocation - Q.Pos).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector Axis = Q.bStreak ? Q.Vel.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector) : FVector::UpVector;
		const FVector Y = (Axis - Z * FVector::DotProduct(Axis, Z)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector X = FVector::CrossProduct(Y, Z).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		AddXf[i] = BillboardXf(Q.Pos, X, Y, Z, Q.Size * (Q.bStreak ? 0.9f : 1.4f), Q.bStreak ? Q.Size * 5.0f : Q.Size * 1.4f);
		AddData[i * 4 + 0] = Q.Color.R;
		AddData[i * 4 + 1] = Q.Color.G;
		AddData[i * 4 + 2] = Q.Color.B;
		AddData[i * 4 + 3] = Fade;
	}
	if (AddBatch && (bAnyAdd || bHadAdditiveLastFrame))
	{
		AddBatch->BatchUpdateInstancesTransforms(0, AddXf, true, false, true);
		AddBatch->SetCustomData(0, MaxAdditiveQuads - 1, AddData, false);
		bHadAdditiveLastFrame = bAnyAdd;
	}
}
