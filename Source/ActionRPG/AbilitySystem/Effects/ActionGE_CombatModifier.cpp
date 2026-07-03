// 文件说明：实现通用战斗修正 GameplayEffect。

#include "AbilitySystem/Effects/ActionGE_CombatModifier.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

namespace
{
	/** 为通用战斗修正效果补一条 Additive + SetByCaller 的属性修改器。*/
	static void AddCombatModifier(
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

	/** 统一写入“处决保护中不可施加/维持”所需的目标 Tag 条件。*/
	static void ConfigureExecutionProtectionTagRequirements(
		UTargetTagRequirementsGameplayEffectComponent& TagRequirementsComponent)
	{
		TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionInvulnerable);
		TagRequirementsComponent.OngoingTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionInvulnerable);
		TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionVictimLocked);
		TagRequirementsComponent.OngoingTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionVictimLocked);
		TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionVictim_HardLock);
		TagRequirementsComponent.OngoingTagRequirements.IgnoreTags.AddTag(
			ActionGameplayTags::State_Combat_ExecutionVictim_HardLock);
	}
}

UActionGE_CombatModifier::UActionGE_CombatModifier()
	: Super()
{
	// 这类效果通常需要持续一段时间，因此默认使用持续型效果。
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 持续时长由外部通过 SetByCaller 注入，这样同一份 GE 能服务多种战斗修正效果。
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = ActionGameplayTags::SetByCaller_Effect_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// 下面这些属性都是很适合被临时增益 / 减益效果修改的战斗修正项。
	AddCombatModifier(Modifiers, UActionAttributeSetBase::GetDamageVulnerabilityAttribute(), ActionGameplayTags::SetByCaller_Attribute_DamageVulnerability);
	AddCombatModifier(Modifiers, UActionAttributeSetBase::GetHealthDamageResistanceAttribute(), ActionGameplayTags::SetByCaller_Attribute_HealthDamageResistance);
	AddCombatModifier(Modifiers, UActionAttributeSetBase::GetGuardStaminaCostResistanceAttribute(), ActionGameplayTags::SetByCaller_Attribute_GuardStaminaCostResistance);
	AddCombatModifier(Modifiers, UActionAttributeSetBase::GetPoiseDamageResistanceAttribute(), ActionGameplayTags::SetByCaller_Attribute_PoiseDamageResistance);
	AddCombatModifier(Modifiers, UActionAttributeSetBase::GetExecutionDamageMultiplierAttribute(), ActionGameplayTags::SetByCaller_Attribute_ExecutionDamageMultiplier);
}

void UActionGE_CombatModifier::PostInitProperties()
{
	Super::PostInitProperties();

	// UE 5.4 中，GameplayEffectComponent 不能在 UObject 构造阶段通过 NewObject(NAME_None) 创建。
	// 因此这里延后到 PostInitProperties，再按需补建组件，避免编辑器启动时构造 CDO 直接崩溃。
	if (!FindComponent<UTargetTagRequirementsGameplayEffectComponent>())
	{
		UTargetTagRequirementsGameplayEffectComponent& TagRequirementsComponent =
			FindOrAddComponent<UTargetTagRequirementsGameplayEffectComponent>();
		ConfigureExecutionProtectionTagRequirements(TagRequirementsComponent);
	}
}
