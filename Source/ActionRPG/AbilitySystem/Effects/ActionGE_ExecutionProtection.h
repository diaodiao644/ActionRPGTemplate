// 文件说明：声明处决执行保护专用 GameplayEffect，用于承载执行者无敌与 victim lock 这类执行链保护状态。

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActionGE_ExecutionProtection.generated.h"

/**
 * 处决执行保护专用 GameplayEffect。
 * 设计目标如下：
 * 1. 继续复用 CombatModifier 那套持续时间、SetByCaller 属性修正和动态授予标签能力；
 * 2. 只服务处决执行保护，不再带“命中 Execution 保护标签就自我抑制”的 TagRequirements；
 * 3. 避免执行者无敌和 victim lock 在授予自身标签后命中 ongoing ignore 条件，触发 GAS 递归抑制。
 */
UCLASS()
class ACTIONRPG_API UActionGE_ExecutionProtection : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 构造函数：把当前效果配置成可持续生效的执行保护效果。*/
	UActionGE_ExecutionProtection();
};
