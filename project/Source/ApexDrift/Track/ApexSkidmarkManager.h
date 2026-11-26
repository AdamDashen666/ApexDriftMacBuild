#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ApexSkidmarkManager.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

/** 胎痕：实例化面片环形缓冲，每段一个实例，只更新新写入的那几段 */
UCLASS()
class APEXDRIFT_API AApexSkidmarkManager : public AActor
{
	GENERATED_BODY()

public:
	AApexSkidmarkManager();

	/** 仅作根组件保留（旧关卡里已序列化） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skidmarks")
	TObjectPtr<UProceduralMeshComponent> SkidProcMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skidmarks")
	TObjectPtr<UMaterialInterface> SkidMaterial;

	/** Adds a single skid quad segment between PrevPoint and CurrPoint for WheelSlot (0..3). */
	void AddSkidSegment(int32 WheelSlot, const FVector& PrevPoint, const FVector& CurrPoint, const FVector& SurfaceNormal, float HalfWidthCm, float Alpha);

	void BreakWheelTrail(int32 WheelSlot);
	void ClearAllSkidmarks();
	int32 GetTotalSkidSegmentCount() const { return TotalSkidSegmentsAdded; }

protected:
	virtual void BeginPlay() override;

private:
	static constexpr int32 MaxSegments = 2400;
	static constexpr int32 NumWheelSlots = 4;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> SkidBatch;

	int32 TotalSkidSegmentsAdded = 0;
	TArray<float> AccumulatedV;
	int32 WriteCursor = 0;
};
