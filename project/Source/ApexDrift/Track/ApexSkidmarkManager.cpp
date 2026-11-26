#include "Track/ApexSkidmarkManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const FTransform GHiddenSkid(FQuat::Identity, FVector::ZeroVector, FVector(0.0001f));
}

AApexSkidmarkManager::AApexSkidmarkManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SkidProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SkidProcMesh"));
	SetRootComponent(SkidProcMesh);
	SkidProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AApexSkidmarkManager::BeginPlay()
{
	Super::BeginPlay();
	AccumulatedV.Init(0.0f, NumWheelSlots);

	UStaticMesh* Quad = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/VFX/SM_VfxQuad.SM_VfxQuad"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	SkidMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/VFX/M_Skidmark.M_Skidmark"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!Quad || !SkidMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ApexVFX] skid mesh/material missing"));
		return;
	}
	SkidBatch = NewObject<UInstancedStaticMeshComponent>(this, TEXT("SkidBatch"));
	SkidBatch->SetMobility(EComponentMobility::Movable);
	SkidBatch->SetUsingAbsoluteLocation(true);
	SkidBatch->SetUsingAbsoluteRotation(true);
	SkidBatch->SetUsingAbsoluteScale(true);
	SkidBatch->SetStaticMesh(Quad);
	SkidBatch->SetMaterial(0, SkidMaterial);
	SkidBatch->NumCustomDataFloats = 3;
	SkidBatch->SetCastShadow(false);
	SkidBatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkidBatch->SetCanEverAffectNavigation(false);
	SkidBatch->bAffectDistanceFieldLighting = false;
	SkidBatch->bAffectDynamicIndirectLighting = false;
	SkidBatch->bVisibleInRayTracing = false;
	SkidBatch->TranslucencySortPriority = -10;
	SkidBatch->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	SkidBatch->RegisterComponent();
	SkidBatch->SetWorldTransform(FTransform::Identity);
	TArray<FTransform> Xf;
	Xf.Init(GHiddenSkid, MaxSegments);
	SkidBatch->AddInstances(Xf, false, true);
}

void AApexSkidmarkManager::BreakWheelTrail(int32 WheelSlot)
{
}

void AApexSkidmarkManager::ClearAllSkidmarks()
{
	if (SkidBatch)
	{
		TArray<FTransform> Xf;
		Xf.Init(GHiddenSkid, MaxSegments);
		SkidBatch->BatchUpdateInstancesTransforms(0, Xf, true, false, true);
	}
	WriteCursor = 0;
}

void AApexSkidmarkManager::AddSkidSegment(int32 WheelSlot, const FVector& PrevPoint, const FVector& CurrPoint, const FVector& SurfaceNormal, float HalfWidthCm, float Alpha)
{
	if (!SkidBatch || !AccumulatedV.IsValidIndex(WheelSlot))
	{
		return;
	}
	const FVector Delta = CurrPoint - PrevPoint;
	const float Dist = Delta.Size();
	if (Dist < 8.0f || Dist > 600.0f)
	{
		return;
	}
	const FVector Up = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	// 面片：X 沿行进方向，Y 横向；略微加长，弯道里段与段之间不露缝
	const FQuat Rot = FRotationMatrix::MakeFromXZ(Delta / Dist, Up).ToQuat();
	const FTransform Xf(Rot, (PrevPoint + CurrPoint) * 0.5f + Up * 2.0f, FVector((Dist + 5.0f) * 0.01f, HalfWidthCm * 0.02f, 1.0f));
	const float Data[3] = { FMath::Clamp(Alpha, 0.0f, 1.0f), AccumulatedV[WheelSlot] * 0.01f, Dist * 0.01f };
	AccumulatedV[WheelSlot] += Dist;
	SkidBatch->UpdateInstanceTransform(WriteCursor, Xf, true, false, true);
	SkidBatch->SetCustomData(WriteCursor, MakeArrayView(Data, 3), false);
	WriteCursor = (WriteCursor + 1) % MaxSegments;
	++TotalSkidSegmentsAdded;
}
