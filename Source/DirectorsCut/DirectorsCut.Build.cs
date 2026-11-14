using UnrealBuildTool;

public class DirectorsCut : ModuleRules
{
	public DirectorsCut(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false; // parallel agents: avoid cross-file name clashes

		PublicIncludePaths.AddRange(new string[] { ModuleDirectory });

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "Json", "JsonUtilities", "Niagara",
			"AudioMixer", "DeveloperSettings", "MeshDescription", "StaticMeshDescription"
		});
	}
}
