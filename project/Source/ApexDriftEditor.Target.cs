using UnrealBuildTool;
using System.Collections.Generic;

public class ApexDriftEditorTarget : TargetRules
{
	public ApexDriftEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("ApexDrift");
	}
}
