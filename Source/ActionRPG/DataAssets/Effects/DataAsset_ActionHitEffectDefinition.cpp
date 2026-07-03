#include "DataAssets/Effects/DataAsset_ActionHitEffectDefinition.h"

#include "DataAssets/Effects/DataAsset_StatusEffectDefinition.h"

UDataAsset_ActionHitEffectDefinition::UDataAsset_ActionHitEffectDefinition()
{
}

bool UDataAsset_ActionHitEffectDefinition::IsValidDefinition() const
{
	// 基类最小约束只要求“这份效果至少有正式生命周期入口”。
	// 更细的 DOT / Buff / Debuff 语义由各自子类继续补充，不在这里混成同一套校验。
	return DurationSeconds > 0.f;
}

FGameplayTag UDataAsset_ActionHitEffectDefinition::GetStatusEffectTag() const
{
	// HitEffect 资产自己不成为状态标签源。
	// 运行时需要状态标签语义时，只从关联的 StatusEffectDefinition 做只读桥接。
	return StatusEffectDefinition ? StatusEffectDefinition->GetStatusEffectTag() : FGameplayTag();
}

UDataAsset_ActionHitDotEffectDefinition::UDataAsset_ActionHitDotEffectDefinition()
{
	// 子类构造阶段只固化正式效果种类，避免外层条目重复推导 Dot 语义。
	EffectKind = EActionHitEffectKind::Dot;
}

bool UDataAsset_ActionHitDotEffectDefinition::IsValidDefinition() const
{
	if (bScaleWithAttackLevel)
	{
		// 启用等级缩放后，DOT 的每跳伤害、持续时长和周期都必须能从等级表完整解析。
		return LevelScaledDamagePerTick.HasAnyLevelValue()
			&& LevelScaledDurationSeconds.HasAnyLevelValue()
			&& LevelScaledPeriodSeconds.HasAnyLevelValue();
	}

	// 未启用等级缩放时，直接要求资产面板上的固定基础值形成完整 DOT 模板。
	return DurationSeconds > 0.f
		&& DamagePerTick > 0.f
		&& PeriodSeconds > 0.f;
}

float UDataAsset_ActionHitDotEffectDefinition::ResolveDamagePerTickForLevel(const int32 AbilityLevel) const
{
	// 这里仅把静态模板解析成“本次读取应使用的每跳伤害”。
	// 它不会回写资产，也不在这里生成 DOT 运行时状态。
	return bScaleWithAttackLevel
		? FMath::Max(LevelScaledDamagePerTick.ResolveValueForLevel(AbilityLevel), 0.f)
		: FMath::Max(DamagePerTick, 0.f);
}

float UDataAsset_ActionHitDotEffectDefinition::ResolveDurationSecondsForLevel(const int32 AbilityLevel) const
{
	// 持续时长解析与伤害解析同理，都是一次性的运行时读取结果。
	return bScaleWithAttackLevel
		? FMath::Max(LevelScaledDurationSeconds.ResolveValueForLevel(AbilityLevel), 0.f)
		: FMath::Max(DurationSeconds, 0.f);
}

float UDataAsset_ActionHitDotEffectDefinition::ResolvePeriodSecondsForLevel(const int32 AbilityLevel) const
{
	// 周期解析最后统一做最小值保护，避免无效资产把 DOT 结算周期压成 0。
	return bScaleWithAttackLevel
		? FMath::Max(LevelScaledPeriodSeconds.ResolveValueForLevel(AbilityLevel), KINDA_SMALL_NUMBER)
		: FMath::Max(PeriodSeconds, KINDA_SMALL_NUMBER);
}

UDataAsset_ActionHitBuffEffectDefinition::UDataAsset_ActionHitBuffEffectDefinition()
{
	// Buff 子类只负责固化“标签型强化效果”这层正式种类。
	EffectKind = EActionHitEffectKind::Buff;
}

bool UDataAsset_ActionHitBuffEffectDefinition::IsValidDefinition() const
{
	// Buff 当前只要求：存在正式生命周期，且至少能通过状态定义或 GrantedTags 表达强化语义。
	return DurationSeconds > 0.f
		&& (StatusEffectDefinition != nullptr || GrantedTags.Num() > 0);
}

UDataAsset_ActionHitDebuffEffectDefinition::UDataAsset_ActionHitDebuffEffectDefinition()
{
	// Debuff 子类同样只在构造阶段固定正式效果种类。
	EffectKind = EActionHitEffectKind::Debuff;
}

bool UDataAsset_ActionHitDebuffEffectDefinition::IsValidDefinition() const
{
	// Debuff 与 Buff 共用同一套最小约束：要么有状态定义，要么至少能落到正式运行时标签。
	return DurationSeconds > 0.f
		&& (StatusEffectDefinition != nullptr || GrantedTags.Num() > 0);
}
