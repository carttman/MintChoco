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
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"RHI",
			"Niagara",
			"OnlineSubsystem",
			"OnlineSubsystemSteam",
			"OnlineSubsystemUtils",
			"SteamSockets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MintChoco",
			"MintChoco/Paint",
			"MintChoco/Sample"
		});
	}
}
