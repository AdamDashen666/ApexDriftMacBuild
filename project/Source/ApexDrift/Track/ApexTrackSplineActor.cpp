#include "Track/ApexTrackSplineActor.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "RenderingThread.h"
#endif

AApexInstancedPropActor::AApexInstancedPropActor()
{
	PrimaryActorTick.bCanEverTick = false;

	InstancedMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedMesh"));
	SetRootComponent(InstancedMesh);
}

void AApexInstancedPropActor::ConfigureBatch(UStaticMesh* Mesh, FName CollisionProfile, float CullStart, float CullEnd, bool bCastShadow, UMaterialInterface* OverrideMat)
{
	if (!InstancedMesh)
	{
		return;
	}
	if (Mesh)
	{
		InstancedMesh->SetStaticMesh(Mesh);
	}
	if (OverrideMat)
	{
		const int32 NumMats = InstancedMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			InstancedMesh->SetMaterial(i, OverrideMat);
		}
	}
	InstancedMesh->SetCollisionProfileName(CollisionProfile);
	InstancedMesh->SetCastShadow(bCastShadow);
	if (CullEnd > 0.0f)
	{
		InstancedMesh->SetCullDistances(FMath::RoundToInt(CullStart), FMath::RoundToInt(CullEnd));
	}
}

void AApexInstancedPropActor::AddPropInstance(const FTransform& WorldTransform)
{
	if (InstancedMesh)
	{
		InstancedMesh->AddInstance(WorldTransform, true);
	}
}

AApexTrackSplineActor::AApexTrackSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TrackSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrackSpline"));
	SetRootComponent(TrackSpline);
	TrackSpline->SetClosedLoop(true);
}

void AApexTrackSplineActor::BeginPlay()
{
	Super::BeginPlay();
	if (MinimapNormalizedPoints.Num() < 8)
	{
		RebuildMinimapCache(180);
	}
}

void AApexTrackSplineActor::RebuildMinimapCache(int32 NumSamples)
{
	MinimapNormalizedPoints.Reset();
	if (!TrackSpline || NumSamples < 8)
	{
		return;
	}

	const float Len = TrackSpline->GetSplineLength();
	if (Len <= 10.0f)
	{
		return;
	}

	TArray<FVector2D> RawPts;
	RawPts.Reserve(NumSamples);
	FVector2D MinP(FLT_MAX, FLT_MAX);
	FVector2D MaxP(-FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Dist = (static_cast<float>(i) / static_cast<float>(NumSamples)) * Len;
		const FVector Pos = TrackSpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
		const FVector2D P2(Pos.X, Pos.Y);
		RawPts.Add(P2);
		MinP.X = FMath::Min(MinP.X, P2.X);
		MinP.Y = FMath::Min(MinP.Y, P2.Y);
		MaxP.X = FMath::Max(MaxP.X, P2.X);
		MaxP.Y = FMath::Max(MaxP.Y, P2.Y);
	}

	// Pad bounds slightly into a square aspect box so track never warps on minimap
	const FVector2D Span = MaxP - MinP;
	const float MaxSpan = FMath::Max(1000.0f, FMath::Max(Span.X, Span.Y)) * 1.14f;
	const FVector2D Center = (MinP + MaxP) * 0.5f;
	MinimapBoundsMin = Center - FVector2D(MaxSpan * 0.5f, MaxSpan * 0.5f);
	MinimapBoundsMax = Center + FVector2D(MaxSpan * 0.5f, MaxSpan * 0.5f);

	for (const FVector2D& P : RawPts)
	{
		const float NX = (P.X - MinimapBoundsMin.X) / MaxSpan;
		const float NY = (P.Y - MinimapBoundsMin.Y) / MaxSpan;
		MinimapNormalizedPoints.Add(FVector2D(NX, NY));
	}
}

void AApexTrackSplineActor::FindClosestTrackTransform(const FVector& WorldPos, FTransform& OutTransform, float& OutDistanceAlongSpline, float& OutLateralOffset) const
{
	if (!TrackSpline)
	{
		OutTransform = FTransform::Identity;
		OutDistanceAlongSpline = 0.0f;
		OutLateralOffset = 0.0f;
		return;
	}

	const float InputKey = TrackSpline->FindInputKeyClosestToWorldLocation(WorldPos);
	OutDistanceAlongSpline = TrackSpline->GetDistanceAlongSplineAtSplineInputKey(InputKey);
	const FVector ClosestLoc = TrackSpline->GetLocationAtSplineInputKey(InputKey, ESplineCoordinateSpace::World);
	FVector Tangent = TrackSpline->GetDirectionAtSplineInputKey(InputKey, ESplineCoordinateSpace::World);
	Tangent.Z = 0.0f;
	if (!Tangent.Normalize())
	{
		Tangent = FVector::ForwardVector;
	}
	const FVector RightVec = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
	OutLateralOffset = FVector::DotProduct(WorldPos - ClosestLoc, RightVec);

	const FRotator TrackRot = Tangent.Rotation();
	OutTransform = FTransform(TrackRot, ClosestLoc + FVector(0.0f, 0.0f, 65.0f), FVector::OneVector);
}

float AApexTrackSplineActor::GetTrackLength() const
{
	return TrackSpline ? TrackSpline->GetSplineLength() : 0.0f;
}

bool AApexTrackSplineActor::EvaluateDriftZone(float DistanceAlongSpline, const FVector& VehiclePos, FApexDriftZoneInfo& OutZone, bool& bOutInsideClippingZone) const
{
	bOutInsideClippingZone = false;
	for (const FApexDriftZoneInfo& Zone : DriftZones)
	{
		const bool bWrap = Zone.EndDistance < Zone.StartDistance;
		const bool bInside = bWrap
			? (DistanceAlongSpline >= Zone.StartDistance || DistanceAlongSpline <= Zone.EndDistance)
			: (DistanceAlongSpline >= Zone.StartDistance && DistanceAlongSpline <= Zone.EndDistance);

		if (bInside)
		{
			OutZone = Zone;
			if (!Zone.ClippingPoint.IsNearlyZero() && Zone.ClippingRadius > 1.0f)
			{
				const float DistSq = FVector::DistSquared2D(VehiclePos, Zone.ClippingPoint);
				bOutInsideClippingZone = (DistSq <= Zone.ClippingRadius * Zone.ClippingRadius);
			}
			return true;
		}
	}
	return false;
}

FVector2D AApexTrackSplineActor::ProjectWorldToMinimap(const FVector& WorldPos) const
{
	const float SpanX = FMath::Max(1.0f, MinimapBoundsMax.X - MinimapBoundsMin.X);
	const float SpanY = FMath::Max(1.0f, MinimapBoundsMax.Y - MinimapBoundsMin.Y);
	return FVector2D(
		FMath::Clamp((WorldPos.X - MinimapBoundsMin.X) / SpanX, 0.0f, 1.0f),
		FMath::Clamp((WorldPos.Y - MinimapBoundsMin.Y) / SpanY, 0.0f, 1.0f)
	);
}

bool AApexTrackSplineActor::TraceGroundHeight(UObject* WorldContextObject, float X, float Y, float& OutZ, FVector& OutNormal)
{
	OutZ = 0.0f;
	OutNormal = FVector::UpVector;
	if (!WorldContextObject)
	{
		return false;
	}
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ApexGroundTrace), true);
	if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, 24000.0f), FVector(X, Y, -4000.0f), ECC_Visibility, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		OutNormal = Hit.ImpactNormal;
		return true;
	}
	return false;
}

void AApexTrackSplineActor::FlushAllShaderCompilation()
{
#if WITH_EDITOR
	FlushRenderingCommands();
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				W->SendAllEndOfFrameUpdates();
			}
		}
	}
	IStreamingManager::Get().StreamAllResources(3.0f);
	FlushRenderingCommands();
#endif
}

bool AApexTrackSplineActor::EnableInstancedStaticMeshUsage(UMaterialInterface* Material)
{
#if WITH_EDITOR
	if (!Material)
	{
		return false;
	}
	const bool bOk = Material->CheckMaterialUsage(MATUSAGE_InstancedStaticMeshes);
	Material->MarkPackageDirty();
	return bOk;
#else
	return false;
#endif
}
