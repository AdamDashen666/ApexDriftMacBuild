#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ApexPropField.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FApexDynProp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props")
	FTransform Xf = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props")
	float MassKg = 10.0f;
};

/** 训练场里可以撞飞的道具：开局全部睡眠，重开或连按两次复位时回到原位 */
UCLASS()
class APEXDRIFT_API AApexPropField : public AActor
{
	GENERATED_BODY()

public:
	AApexPropField();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Props")
	TArray<FApexDynProp> Props;

	UFUNCTION(BlueprintCallable, Category = "Props")
	void AddProp(UStaticMesh* Mesh, const FTransform& Xf, float MassKg);

	/** 全部复位并睡眠 */
	UFUNCTION(BlueprintCallable, Category = "Props")
	void ResetAll();

	/** 离原位超过 1m 的道具数（自动验证用） */
	int32 CountMoved() const;
	int32 Num() const { return Comps.Num(); }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Comps;
};
