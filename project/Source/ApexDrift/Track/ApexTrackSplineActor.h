#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ApexDriftTypes.h"
#include "ApexTrackSplineActor.generated.h"

class USplineComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class APEXDRIFT_API AApexInstancedPropActor : public AActor
{
	GENERATED_BODY()

public:
	AApexInstancedPropActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Props")
	TObjectPtr<UInstancedStaticMeshComponent> InstancedMesh;

	UFUNCTION(BlueprintCallable, Category = "Track")
	void ConfigureBatch(UStaticMesh* Mesh, FName CollisionProfile, float CullStart, float CullEnd, bool bCastShadow, UMaterialInterface* OverrideMat);

	UFUNCTION(BlueprintCallable, Category = "Track")
	void AddPropInstance(const FTransform& WorldTransform);
};

UCLASS()
class APEXDRIFT_API AApexTrackSplineActor : public AActor
{
	GENERATED_BODY()

public:
	AApexTrackSplineActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<USplineComponent> TrackSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	float TrackWidthCm = 1500.0f;

	/** 自由场地：不判逆行、复位原地扶正、离开外环不计圈 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	bool bFreeRoam = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	TArray<FApexDriftZoneInfo> DriftZones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	TArray<FVector2D> MinimapNormalizedPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FVector2D MinimapBoundsMin = FVector2D(-50000.0f, -50000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Track")
	FVector2D MinimapBoundsMax = FVector2D(50000.0f, 50000.0f);

	UFUNCTION(BlueprintCallable, Category = "Track")
	void RebuildMinimapCache(int32 NumSamples = 180);

	UFUNCTION(BlueprintCallable, Category = "Track")
	void FindClosestTrackTransform(const FVector& WorldPos, FTransform& OutTransform, float& OutDistanceAlongSpline, float& OutLateralOffset) const;

	UFUNCTION(BlueprintCallable, Category = "Track")
	float GetTrackLength() const;

	UFUNCTION(BlueprintCallable, Category = "Track")
	bool EvaluateDriftZone(float DistanceAlongSpline, const FVector& VehiclePos, FApexDriftZoneInfo& OutZone, bool& bOutInsideClippingZone) const;

	UFUNCTION(BlueprintCallable, Category = "Track")
	FVector2D ProjectWorldToMinimap(const FVector& WorldPos) const;

	UFUNCTION(BlueprintCallable, Category = "Track", meta = (WorldContext = "WorldContextObject"))
	static bool TraceGroundHeight(UObject* WorldContextObject, float X, float Y, float& OutZ, FVector& OutNormal);

	UFUNCTION(BlueprintCallable, Category = "Track")
	static void FlushAllShaderCompilation();

	UFUNCTION(BlueprintCallable, Category = "Track")
	static bool EnableInstancedStaticMeshUsage(UMaterialInterface* Material);

protected:
	virtual void BeginPlay() override;
};
