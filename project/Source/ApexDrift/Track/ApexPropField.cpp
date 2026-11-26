#include "Track/ApexPropField.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AApexPropField::AApexPropField()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AApexPropField::AddProp(UStaticMesh* Mesh, const FTransform& Xf, float MassKg)
{
	FApexDynProp& P = Props.AddDefaulted_GetRef();
	P.Mesh = Mesh;
	P.Xf = Xf;
	P.MassKg = MassKg;
}

void AApexPropField::BeginPlay()
{
	Super::BeginPlay();
	Comps.Reserve(Props.Num());
	for (const FApexDynProp& P : Props)
	{
		if (!P.Mesh) continue;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(P.Mesh);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->SetWorldTransform(P.Xf);
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		// 悬挂射线用 WorldDynamic：车轮不骑上道具，只用车身去撞
		C->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		C->BodyInstance.bStartAwake = false;
		C->BodyInstance.LinearDamping = 0.15f;
		C->BodyInstance.AngularDamping = 0.35f;
		C->SetSimulatePhysics(true);
		C->SetCanEverAffectNavigation(false);
		C->SetGenerateOverlapEvents(false);
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		C->SetMassOverrideInKg(NAME_None, P.MassKg, true);
		C->PutRigidBodyToSleep();
		Comps.Add(C);
	}
}

void AApexPropField::ResetAll()
{
	int32 i = 0;
	for (const FApexDynProp& P : Props)
	{
		if (!P.Mesh) continue;
		if (Comps.IsValidIndex(i) && Comps[i])
		{
			UStaticMeshComponent* C = Comps[i];
			C->SetPhysicsLinearVelocity(FVector::ZeroVector);
			C->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			C->SetWorldTransform(P.Xf, false, nullptr, ETeleportType::ResetPhysics);
			C->PutRigidBodyToSleep();
		}
		++i;
	}
}

int32 AApexPropField::CountMoved() const
{
	int32 N = 0, i = 0;
	for (const FApexDynProp& P : Props)
	{
		if (!P.Mesh) continue;
		if (Comps.IsValidIndex(i) && Comps[i] && FVector::Dist(Comps[i]->GetComponentLocation(), P.Xf.GetLocation()) > 100.0f) ++N;
		++i;
	}
	return N;
}
