// 文件说明：声明通用受击伤害 GameplayEffect，用于承载一次即时受击的原始伤害输入。

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActionGE_HitDamage.generated.h"

/**
 * 通用受击伤害 GameplayEffect。
 * 设计目标如下：
 * 1. 只负责把一次受击中的原始生命 / 防御耗体 / 韧性数值送进目标 AttributeSet；
 * 2. 具体公式仍然放在 AttributeSet 中统一计算，避免效果层和属性集各写一套伤害逻辑；
 * 3. DOT、Buff、Debuff 等 HitEffect 由命中效果资产链单独附着，不再混入这份即时伤害 GE。
 */
UCLASS()
class ACTIONRPG_API UActionGE_HitDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 构造函数：把当前效果配置成瞬时结算，并声明所有受击元属性的 SetByCaller 写入口。*/
	UActionGE_HitDamage();

protected:
	/** 初始化阶段补建目标 Tag 条件组件，避免在构造函数里直接创建组件导致 UE 5.4 启动崩溃。*/
	virtual void PostInitProperties() override;
};
