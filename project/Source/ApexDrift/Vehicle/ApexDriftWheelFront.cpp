#include "Vehicle/ApexDriftWheelFront.h"

UApexDriftWheelFront::UApexDriftWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	bAffectedByEngine = false;
	bAffectedByBrake = true;
	bAffectedByHandbrake = false;

	WheelRadius = 32.1f;
	WheelWidth = 26.7f;
	WheelMass = 22.0f;
	MaxSteerAngle = 46.0f;

	FrictionForceMultiplier = 3.40f;
	CorneringStiffness = 1350.0f;
	SideSlipModifier = 0.92f;
	SlipThreshold = 90.0f;
	SkidThreshold = 60.0f;

	bABSEnabled = true;
	bTractionControlEnabled = false;
	MaxBrakeTorque = 4800.0f;
	MaxHandBrakeTorque = 0.0f;

	SuspensionAxis = FVector(0.0f, 0.0f, -1.0f);
	SuspensionForceOffset = FVector(0.0f, 0.0f, 0.0f);
	SuspensionMaxRaise = 10.0f;
	SuspensionMaxDrop = 10.0f;
	SuspensionDampingRatio = 0.55f;
	SpringRate = 280.0f;
	SpringPreload = 50.0f;
	SuspensionSmoothing = 3;
	RollbarScaling = 0.35f;
	WheelLoadRatio = 0.65f;
	SweepShape = ESweepShape::Raycast;
	SweepType = ESweepType::SimpleSweep;
}
