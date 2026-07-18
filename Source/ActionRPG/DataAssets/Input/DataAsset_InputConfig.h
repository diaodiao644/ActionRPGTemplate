// 文件说明：声明输入映射数据资产，统一承接输入动作到正式输入标签的静态配置。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActionType/ActionInputTypes.h"
#include "DataAsset_InputConfig.generated.h"

/**
 * 输入映射数据资产。
 * 它只负责维护输入动作到正式输入标签的资源映射入口，
 * 方便角色装配、输入初始化和蓝图工具统一读取；
 * 不替代 HeroCombatInputComponent 持有的运行时正式输入状态源。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_InputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// 查询入口：
	// 外层只通过正式输入标签回查资源，不在这里推进输入消费或资格判断。
	/** 按正式输入标签查找对应的输入动作资源。它只做静态资源回查，不推进输入消费、资格判断或缓冲写入。 */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& Tag, bool bLogNotFound = false) const;

public:
	// 资产面板字段：
	// 这里维护“输入标签 -> 输入动作”的静态映射，供角色与输入系统启动时装配。
	/** 输入动作与正式输入标签的静态映射列表。它只是配置入口，不保存运行时输入状态，也不持有控制器侧移动镜像。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "InputConfig", meta = (ToolTip = "输入动作到正式输入标签的静态映射列表。它只负责资源映射，不保存按键按下、缓冲输入、Held 回放或消费状态。"))
	TArray<FActionInputBinding> AbilityInputBindings;
};
