// 文件说明：声明通用战斗修正 GameplayEffect，用于承载持续型战斗增减益效果。

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActionGE_CombatModifier.generated.h"

/**
 * 通用战斗修正 GameplayEffect。
 * 设计目标如下：
 * 1. 用一份效果承载“易伤、抗性、处决伤害强化”这类持续一段时间的战斗状态；
 * 2. 具体加多少完全由 SetByCaller 决定，便于蓝图、GA 或后续数据表动态组装；
 * 3. 属性层与效果层解耦后，后续做中毒抗性、破甲负面效果、处决强化增益效果会简单很多。
 */
UCLASS()
class ACTIONRPG_API UActionGE_CombatModifier : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 构造函数：把当前效果配置成可持续生效的通用战斗修正效果。*/
	UActionGE_CombatModifier();

protected:
	/** 初始化阶段补建目标 Tag 条件组件，避免在构造函数里直接创建组件导致 UE 5.4 启动崩溃。*/
	virtual void PostInitProperties() override;
};
