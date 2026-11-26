#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ApexDriftVFXComponent.generated.h"

class UInstancedStaticMeshComponent;

/** 轮胎烟、排气火焰、撞击火花：CPU 模拟 + 实例化面片（每实例自定义数据驱动材质） */
UCLASS(ClassGroup = (VFX), meta = (BlueprintSpawnableComponent))
class APEXDRIFT_API UApexDriftVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UApexDriftVFXComponent();

	void EmitTireSmoke(const FVector& WorldPos, const FVector& InheritedVel, float Intensity);
	void EmitExhaustFlame(const FVector& WorldPos, const FVector& BackwardDir);
	void EmitImpactSparks(const FVector& WorldPos, const FVector& SurfaceNormal, float Intensity);

	void UpdateParticles(float DeltaSeconds, const FVector& CameraLocation);
	int32 GetTotalEmittedSmokeCount() const { return TotalSmokeEmitted; }

protected:
	virtual void BeginPlay() override;

private:
	UInstancedStaticMeshComponent* MakeBatch(const TCHAR* Name, const TCHAR* MaterialPath, int32 NumInstances, int32 NumCustom);

	int32 TotalSmokeEmitted = 0;
	struct FSmokePuff
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Age = 0.0f;
		float MaxAge = 2.0f;
		float StartSize = 42.0f;
		float EndSize = 260.0f;
		float Intensity = 1.0f;
		float RollAngle = 0.0f;
		float RollSpeed = 0.0f;
		int32 Frame = 0;
		bool bAlive = false;
	};

	struct FAdditiveQuad
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Age = 0.0f;
		float MaxAge = 0.3f;
		float Size = 24.0f;
		FLinearColor Color = FLinearColor::White;
		bool bStreak = false;
		bool bAlive = false;
	};

	static constexpr int32 MaxSmokePuffs = 180;
	static constexpr int32 MaxAdditiveQuads = 72;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> SmokeBatch;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> AddBatch;

	TArray<FSmokePuff> SmokePool;
	TArray<FAdditiveQuad> AdditivePool;
	TArray<FTransform> SmokeXf;
	TArray<float> SmokeData;
	TArray<FTransform> AddXf;
	TArray<float> AddData;
	int32 SmokeCursor = 0;
	int32 AddCursor = 0;
	bool bInitialized = false;
	bool bHadSmokeLastFrame = false;
	bool bHadAdditiveLastFrame = false;
};
