// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MintChoco : ModuleRules
{
	public MintChoco(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"RHI",
			"RenderCore",
			"Niagara",
			"OnlineSubsystem",
			"OnlineSubsystemSteam",
			"OnlineSubsystemUtils",
			"SteamSockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
            "AssetRegistry"
        });

		PublicIncludePaths.AddRange(new string[] {
			"MintChoco",
			"MintChoco/Paint",
			"MintChoco/Sample",
			"MintChoco/Weapons"
		});
	}
}
