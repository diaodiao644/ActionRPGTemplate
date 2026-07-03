// 文件说明：声明通用持续伤害 GameplayEffect，用于统一承载燃烧、中毒、流血等周期伤害。

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActionGE_DamageOverTime.generated.h"

/**
 * 通用持续伤害 GameplayEffect。
 * 设计目标如下：
 * 1. 统一承载“中毒、灼烧、流血”等需要周期扣血的效果；
 * 2. 每一跳伤害直接作用于生命值，不经过护盾，也不复用普通受击的元属性伤害公式；
 * 3. 周期与持续时长允许在创建 Spec 时动态写入，方便同一份 C++ 基类服务多种持续伤害效果。
 */
UCLASS()
class ACTIONRPG_API UActionGE_DamageOverTime : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 构造函数：把当前效果配置成“持续型 + 周期型 + 每跳直接扣生命”的基础持续伤害效果。*/
	UActionGE_DamageOverTime();

protected:
	/** 初始化阶段补建目标 Tag 条件组件，避免在构造函数里直接创建组件导致 UE 5.4 启动崩溃。*/
	virtual void PostInitProperties() override;
};
