// 文件说明：配置 ActionRPG.Build 模块构建规则。

using UnrealBuildTool;

public class ActionRPG : ModuleRules
{
	public ActionRPG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				"ActionRPG"
			}
			);	

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "EnhancedInput",
            "ModularGameplay",
            "GameplayTags",
			"GameplayAbilities",
            "GameplayTasks",
            "SlateCore",
            "Slate",
            "UMG",
            "Niagara"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("AssetRegistry");
		}

		// 如果需要使用 Slate UI，可以取消下面这行注释。
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// 如果需要使用在线功能，可以取消下面这行注释。
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// 如果需要启用 OnlineSubsystemSteam，请在 uproject 的 plugins 节点中开启对应插件。
	}
}
