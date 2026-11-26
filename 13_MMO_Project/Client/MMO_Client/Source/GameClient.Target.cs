// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class GameClientTarget : TargetRules
{
	public GameClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("MMO_Client");
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration == UnrealTargetConfiguration.Development)
		{
			PostBuildSteps.Add("\"$(EngineDir)/Binaries/ThirdParty/Python3/Win64/python.exe\" \"$(ProjectDir)/Source/CollectGameClient.py\" \"$(EngineDir)\" || exit /b 1");
		}
	}
}
