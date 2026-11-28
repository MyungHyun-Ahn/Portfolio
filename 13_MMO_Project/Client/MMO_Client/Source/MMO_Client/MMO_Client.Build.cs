// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MMO_Client : ModuleRules
{
	public MMO_Client(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "MMOClientNetwork" });
		RuntimeDependencies.Add("$(ProjectDir)/Content/Slate/Login/AshenGate.png", StagedFileType.UFS);

		PublicIncludePaths.AddRange(new string[] {
			"MMO_Client",
			"MMO_Client/Variant_Strategy",
			"MMO_Client/Variant_Strategy/UI",
			"MMO_Client/Variant_TwinStick",
			"MMO_Client/Variant_TwinStick/AI",
			"MMO_Client/Variant_TwinStick/Gameplay",
			"MMO_Client/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
