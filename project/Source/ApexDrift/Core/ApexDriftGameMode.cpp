#include "Core/ApexDriftGameMode.h"
#include "Core/ApexDriftPlayerController.h"
#include "Vehicle/ApexDriftVehiclePawn.h"

AApexDriftGameMode::AApexDriftGameMode()
{
	DefaultPawnClass = AApexDriftVehiclePawn::StaticClass();
	PlayerControllerClass = AApexDriftPlayerController::StaticClass();
}
