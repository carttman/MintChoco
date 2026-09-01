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
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemSteam"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MintChoco",
			"MintChoco/Paint",
			"MintChoco/Sample",
			"MintChoco/Variant_Platforming",
			"MintChoco/Variant_Platforming/Animation",
			"MintChoco/Variant_Combat",
			"MintChoco/Variant_Combat/AI",
			"MintChoco/Variant_Combat/Animation",
			"MintChoco/Variant_Combat/Gameplay",
			"MintChoco/Variant_Combat/Interfaces",
			"MintChoco/Variant_Combat/UI",
			"MintChoco/Variant_SideScrolling",
			"MintChoco/Variant_SideScrolling/AI",
			"MintChoco/Variant_SideScrolling/Gameplay",
			"MintChoco/Variant_SideScrolling/Interfaces",
			"MintChoco/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		 //PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
