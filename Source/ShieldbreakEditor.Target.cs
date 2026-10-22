using UnrealBuildTool;
using System.Collections.Generic;

public class ShieldbreakEditorTarget : TargetRules
{
	public ShieldbreakEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Shieldbreak");
	}
}
