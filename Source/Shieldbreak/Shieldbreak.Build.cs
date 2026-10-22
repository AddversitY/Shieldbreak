using UnrealBuildTool;

public class Shieldbreak : ModuleRules
{
	public Shieldbreak(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets every file include module-root relative paths, e.g. "Shield/ShieldHealthComponent.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"PhysicsCore",
			"NetCore",
			"Niagara"
		});

		// Canvas text/font helpers and AI headers reach into these; listing them keeps links explicit.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"NavigationSystem"
		});
	}
}
