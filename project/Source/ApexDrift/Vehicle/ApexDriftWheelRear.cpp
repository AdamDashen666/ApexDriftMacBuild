#include "Vehicle/ApexDriftWheelRear.h"

UApexDriftWheelRear::UApexDriftWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	bAffectedByEngine = true;
	bAffectedByBrake = true;
	bAffectedByHandbrake = true;

	WheelRadius = 34.7f;
	WheelWidth = 28.9f;
	WheelMass = 24.0f;
	MaxSteerAngle = 0.0f;

	FrictionForceMultiplier = 3.90f;
	CorneringStiffness = 1400.0f;
	SideSlipModifier = 0.90f;
	SlipThreshold = 85.0f;
	SkidThreshold = 45.0f;

	bABSEnabled = false;
	bTractionControlEnabled = true;
	MaxBrakeTorque = 3200.0f;
	MaxHandBrakeTorque = 7800.0f;

	SuspensionAxis = FVector(0.0f, 0.0f, -1.0f);
	SuspensionForceOffset = FVector(0.0f, 0.0f, 0.0f);
	SuspensionMaxRaise = 10.0f;
	SuspensionMaxDrop = 10.0f;
	SuspensionDampingRatio = 0.52f;
	SpringRate = 265.0f;
	SpringPreload = 50.0f;
	SuspensionSmoothing = 3;
	RollbarScaling = 0.30f;
	WheelLoadRatio = 0.60f;
	SweepShape = ESweepShape::Raycast;
	SweepType = ESweepType::SimpleSweep;
}
