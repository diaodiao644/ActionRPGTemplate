// 文件说明：实现项目内复用的通用冷却 GameplayEffect。

#include "AbilitySystem/Effects/ActionGE_GenericCooldown.h"

#include "ActionGameplayTags.h"

UActionGE_GenericCooldown::UActionGE_GenericCooldown()
	: Super()
{
	// 统一把这份效果配置成“持续型冷却”，具体持续秒数由能力在应用时写入。
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat CooldownDurationMagnitude;
	CooldownDurationMagnitude.DataTag = ActionGameplayTags::SetByCaller_Cooldown_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(CooldownDurationMagnitude);
}
