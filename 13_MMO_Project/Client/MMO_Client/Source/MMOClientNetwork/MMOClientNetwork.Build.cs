using System.IO;
using UnrealBuildTool;

public class MMOClientNetwork : ModuleRules
{
	public MMOClientNetwork(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
		// 네이티브 라이브러리는 UE 전용 중간 경로에서 같은 컴파일러와 릴리스 CRT로 빌드한다.
		const string ToolVersion = "14.50.35717";
		if (Target.Platform != UnrealTargetPlatform.Win64 || Target.Architecture != UnrealArch.X64 ||
			Target.bUseStaticCRT ||
			(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT))
		{
			throw new BuildException("MMOClientNetwork requires Win64 x64 MSVC " + ToolVersion + " with release DLL CRT.");
		}
		string ProjectDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../.."));
		string Root = Path.GetFullPath(Path.Combine(ProjectDir, "../.."));
		PrivateIncludePaths.Add(Path.Combine(Root, "Client/ClientNetworkLib"));
		PrivateIncludePaths.Add(Path.Combine(Root, "Common/NetworkCommon"));
		PrivateIncludePaths.Add(Path.Combine(Root, "Client/UEChatLogin"));
		PrivateIncludePaths.Add(Path.Combine(Root, "Client/GameLogin"));
		foreach (string Name in new[] { "GameLogin", "UEChatLogin", "ClientNetworkLib", "NetworkCommonLib" })
		{
			PublicPreBuildLibraries.Add(Path.Combine(Root, "Build/Unreal/NativeNetwork", ToolVersion, Name, Name + ".lib"));
		}
		PublicSystemLibraries.Add("ws2_32.lib");
	}
}
