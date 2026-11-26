using UnrealBuildTool;

public class ApexDrift : ModuleRules
{
	public ApexDrift(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"Niagara",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore",
			"RenderCore",
			"RHI",
			"ApplicationCore",
			"MoviePlayer"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AudioExtensions",
			"Chaos"
		});

		PublicIncludePaths.AddRange(new string[] {
			"ApexDrift",
			"ApexDrift/Vehicle",
			"ApexDrift/Track",
			"ApexDrift/Gameplay",
			"ApexDrift/Audio",
			"ApexDrift/Core",
			"ApexDrift/UI"
		});
	}
}
