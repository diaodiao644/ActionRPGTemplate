// 文件说明：实现通用持续伤害 GameplayEffect。

#include "AbilitySystem/Effects/ActionGE_DamageOverTime.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

UActionGE_DamageOverTime::UActionGE_DamageOverTime()
	: Super()
{
	// 持续伤害需要持续生效，因此这里配置为持续型效果。
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 默认先给一个 1 秒周期，真正应用时可由外部直接修改运行时 Spec 的 Period。
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;

	// 持续时长通过 SetByCaller 动态提供，方便一份效果服务多种持续伤害类型。
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = ActionGameplayTags::SetByCaller_Effect_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// DOT 每一跳直接改生命值：
	// 1. 不经过 IncomingDamage；
	// 2. 不吃护盾吸收；
	// 3. 也不复用普通受击链的最终伤害公式。
	FGameplayModifierInfo& DamageModifier = Modifiers.AddDefaulted_GetRef();
	DamageModifier.Attribute = UActionAttributeSetBase::GetHealthAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat DamageSetByCaller;
	DamageSetByCaller.DataTag = ActionGameplayTags::SetByCaller_Damage_Health;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageSetByCaller);
}

void UActionGE_DamageOverTime::PostInitProperties()
{
	Super::PostInitProperties();

	// 目标一旦进入处决无敌或处决受害者锁定状态：
	// 1. 新的持续伤害效果不能再被施加；
	// 2. 已经挂上的持续伤害效果也会因为 Ongoing 条件失效而停止周期结算。
	// 这里改为在 PostInitProperties 补建组件，避免构造阶段触发 UE 5.4 的 CDO 崩溃保护。
	if (!FindComponent<UTargetTagRequirementsGameplayEffectComponent>())
	{
		UTargetTagRequirementsGameplayEffectComponent& TagRequirementsComponent =
			FindOrAddComponent<UTargetTagRequirementsGameplayEffectComponent>();
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
