// 文件说明：声明项目内复用的通用冷却 GameplayEffect，用于统一承载各类能力冷却。

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActionGE_GenericCooldown.generated.h"

/**
 * 通用冷却 GameplayEffect。
 * 设计目标如下：
 * 1. 统一承载“给目标附加某个冷却标签，并持续一段时间”这类最基础的冷却行为；
 * 2. 冷却时长不写死在效果里，而是由具体 GameplayAbility 在应用时通过 SetByCaller 传入；
 * 3. 后续任意能力只需要配置“冷却标签 + 冷却时长”，就可以直接复用这一份冷却效果。
 */
UCLASS()
class ACTIONRPG_API UActionGE_GenericCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 构造函数：把当前效果配置成“持续型 + 时长由 SetByCaller 提供”的通用冷却效果。 */
	UActionGE_GenericCooldown();
};
