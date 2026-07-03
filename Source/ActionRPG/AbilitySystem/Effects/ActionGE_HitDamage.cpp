// 文件说明：实现通用受击伤害 GameplayEffect。

#include "AbilitySystem/Effects/ActionGE_HitDamage.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

namespace
{
	/** 为指定元属性补一条 Additive + SetByCaller 的属性修改器，统一受击伤害声明方式。*/
	static void AddSetByCallerModifier(
		TArray<FGameplayModifierInfo>& Modifiers,
		const FGameplayAttribute& TargetAttribute,
		const FGameplayTag& DataTag)
	{
		FGameplayModifierInfo& ModifierInfo = Modifiers.AddDefaulted_GetRef();
		ModifierInfo.Attribute = TargetAttribute;
		ModifierInfo.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCallerMagnitude;
		SetByCallerMagnitude.DataTag = DataTag;
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);
	}
}

UActionGE_HitDamage::UActionGE_HitDamage()
	: Super()
{
	// 受击伤害需要立即结算，因此这里使用瞬时 GameplayEffect。
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 普通受击链统一通过 SetByCaller 写入，再由 AttributeSet 内部做防御、抗性和落地。
	AddSetByCallerModifier(Modifiers, UActionAttributeSetBase::GetIncomingDamageAttribute(), ActionGameplayTags::SetByCaller_Damage_Health);
	AddSetByCallerModifier(Modifiers, UActionAttributeSetBase::GetIncomingGuardStaminaCostAttribute(), ActionGameplayTags::SetByCaller_Cost_GuardStamina);
	AddSetByCallerModifier(Modifiers, UActionAttributeSetBase::GetIncomingPoiseDamageAttribute(), ActionGameplayTags::SetByCaller_Damage_Poise);
}

void UActionGE_HitDamage::PostInitProperties()
{
	Super::PostInitProperties();

	// VictimLock / RecoverHardLock 需要由命中解析器区分“当前处决命中”和外部干扰。
	// GE 层只保留无法区分来源也必须生效的无条件硬拦截。
	// 每次初始化都主动归一化组件内容，避免旧 CDO / 旧子对象状态继续保留已删除的忽略规则。
	UTargetTagRequirementsGameplayEffectComponent& TagRequirementsComponent =
		FindOrAddComponent<UTargetTagRequirementsGameplayEffectComponent>();
	TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.RemoveTag(
		ActionGameplayTags::State_Combat_ExecutionVictimLocked);
	TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.RemoveTag(
		ActionGameplayTags::State_Combat_ExecutionVictim_HardLock);
	TagRequirementsComponent.ApplicationTagRequirements.IgnoreTags.AddTag(
		ActionGameplayTags::State_Combat_ExecutionInvulnerable);
}
