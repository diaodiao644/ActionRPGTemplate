// 文件说明：实现处决执行保护专用 GameplayEffect。

#include "AbilitySystem/Effects/ActionGE_ExecutionProtection.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

namespace
{
	/** 为执行保护效果补一条 Additive + SetByCaller 的属性修改器。*/
	static void AddExecutionProtectionModifier(
		TArray<FGameplayModifierInfo>& Modifiers,
		const FGameplayAttribute& Attribute,
		const FGameplayTag& SetByCallerTag)
	{
		FGameplayModifierInfo& ModifierInfo = Modifiers.AddDefaulted_GetRef();
		ModifierInfo.Attribute = Attribute;
		ModifierInfo.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat Magnitude;
		Magnitude.DataTag = SetByCallerTag;
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	}
}

UActionGE_ExecutionProtection::UActionGE_ExecutionProtection()
	: Super()
{
	// 执行保护和通用战斗修正一样，默认都按持续型效果处理。
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = ActionGameplayTags::SetByCaller_Effect_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// 这里保留与通用 CombatModifier 一致的属性承载能力。
	// 这样执行保护若需要顺带叠一层执行伤害倍率或抗性修正时，不必再开第三套 GE 结构。
	AddExecutionProtectionModifier(
		Modifiers,
		UActionAttributeSetBase::GetDamageVulnerabilityAttribute(),
		ActionGameplayTags::SetByCaller_Attribute_DamageVulnerability);
	AddExecutionProtectionModifier(
		Modifiers,
		UActionAttributeSetBase::GetHealthDamageResistanceAttribute(),
		ActionGameplayTags::SetByCaller_Attribute_HealthDamageResistance);
	AddExecutionProtectionModifier(
		Modifiers,
		UActionAttributeSetBase::GetGuardStaminaCostResistanceAttribute(),
		ActionGameplayTags::SetByCaller_Attribute_GuardStaminaCostResistance);
	AddExecutionProtectionModifier(
		Modifiers,
		UActionAttributeSetBase::GetPoiseDamageResistanceAttribute(),
		ActionGameplayTags::SetByCaller_Attribute_PoiseDamageResistance);
	AddExecutionProtectionModifier(
		Modifiers,
		UActionAttributeSetBase::GetExecutionDamageMultiplierAttribute(),
		ActionGameplayTags::SetByCaller_Attribute_ExecutionDamageMultiplier);
}
