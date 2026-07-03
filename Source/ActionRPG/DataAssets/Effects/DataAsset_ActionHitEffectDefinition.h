#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "DataAsset_ActionHitEffectDefinition.generated.h"

class UDataAsset_StatusEffectDefinition;

/**
 * 命中附加效果定义资产基类。
 * 它只描述“命中后要附加什么命中效果”，
 * 不承担直伤公式，也不承担旧命中修正语义。
 *
 * 这份资产始终是静态配置模板：
 * 外层命中链会先读取它，再生成正式效果请求或运行时效果实例，
 * 不会把数据资产本身当成活跃效果状态源。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_ActionHitEffectDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAsset_ActionHitEffectDefinition();

	// 查询与校验入口：
	// 外层命中链会先读这份静态定义，再在真正命中时生成正式效果请求或运行态。
	UFUNCTION(BlueprintPure, Category = "HitEffect")
	virtual EActionHitEffectKind GetEffectKind() const { return EffectKind; }

	UFUNCTION(BlueprintPure, Category = "HitEffect")
	virtual bool IsValidDefinition() const;

	UFUNCTION(BlueprintPure, Category = "HitEffect")
	FGameplayTag GetStatusEffectTag() const;

public:
	// 资产面板字段：
	// 这些字段只描述“命中后想附加什么效果”，不直接变成已经运行中的效果实例。
	/** 当前效果的小类，由正式子类构造函数固化，不在条目层重复推导。 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "HitEffect", meta = (ToolTip = "这份命中效果定义的正式子类语义。由子类固定为 Dot / Buff / Debuff，资产面板只读查看，不在条目层重复推导。"))
	EActionHitEffectKind EffectKind = EActionHitEffectKind::Debuff;

	/** 当前效果对应的状态效果定义。运行时会从这里补齐状态标签和 UI 语义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect", meta = (ToolTip = "命中效果对应的状态效果定义。运行时会通过它补齐状态标签语义和展示数据，但不会把这份资产本身当成活跃状态实例。"))
	TObjectPtr<UDataAsset_StatusEffectDefinition> StatusEffectDefinition = nullptr;

	/** 当前效果持续时长。DOT / Buff / Debuff 都以这条时长作为正式生命周期。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect", meta = (ClampMin = "0.0", ToolTip = "这份命中效果的默认持续时长。Dot / Buff / Debuff 都会把它当成正式生命周期配置读取。"))
	float DurationSeconds = 0.f;

	/** 当前效果真正授予目标 ASC 的运行时标签。Buff / Debuff 只通过标签表达，不再直带数值修正。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect", meta = (ToolTip = "命中后要授予目标 ASC 的运行时标签集合。Buff / Debuff 当前只通过标签表达效果语义，不在这里直接写数值修正。"))
	FGameplayTagContainer GrantedTags;
};

/**
 * DOT 效果资产。
 * 当前正式语义是“挂一份独立 DOT GE”，
 * 每跳直接扣生命，不经过护盾和普通受击公式。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_ActionHitDotEffectDefinition : public UDataAsset_ActionHitEffectDefinition
{
	GENERATED_BODY()

public:
	UDataAsset_ActionHitDotEffectDefinition();

	virtual bool IsValidDefinition() const override;

public:
	// DOT 资产字段：
	// 这一组只描述 DOT 的静态模板，不代表目标身上已经存在正在跳伤害的运行态。
	/** 每跳 DOT 伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (ClampMin = "0.0", ToolTip = "DOT 每次结算时造成的基础伤害。未启用等级缩放时，运行时直接读取这个值。"))
	float DamagePerTick = 0.f;

	/** DOT 周期。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (ClampMin = "0.01", ToolTip = "DOT 的结算周期秒数。运行时会按这个周期持续触发每跳伤害。"))
	float PeriodSeconds = 1.f;

	/** 是否按当前攻击等级解析 DOT 数值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (ToolTip = "是否改用等级驱动配置解析 DOT 伤害、持续时长和周期。关闭时只读取上面的固定基础值。"))
	bool bScaleWithAttackLevel = false;

	/** 等级驱动 DOT 每跳伤害。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (EditCondition = "bScaleWithAttackLevel", EditConditionHides, ToolTip = "启用等级缩放后，每个能力等级对应的 DOT 每跳伤害曲线。"))
	FActionLevelScaledFloat LevelScaledDamagePerTick;

	/** 等级驱动 DOT 持续时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (EditCondition = "bScaleWithAttackLevel", EditConditionHides, ToolTip = "启用等级缩放后，每个能力等级对应的 DOT 总持续时长。"))
	FActionLevelScaledFloat LevelScaledDurationSeconds;

	/** 等级驱动 DOT 周期。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitEffect|Dot", meta = (EditCondition = "bScaleWithAttackLevel", EditConditionHides, ToolTip = "启用等级缩放后，每个能力等级对应的 DOT 结算周期。"))
	FActionLevelScaledFloat LevelScaledPeriodSeconds;

public:
	float ResolveDamagePerTickForLevel(int32 AbilityLevel) const;
	float ResolveDurationSecondsForLevel(int32 AbilityLevel) const;
	float ResolvePeriodSecondsForLevel(int32 AbilityLevel) const;
};

/** Buff 效果资产：只承载“给自身或目标挂标签型强化”的正式数据。 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_ActionHitBuffEffectDefinition : public UDataAsset_ActionHitEffectDefinition
{
	GENERATED_BODY()

public:
	UDataAsset_ActionHitBuffEffectDefinition();

	virtual bool IsValidDefinition() const override;
};

/** Debuff 效果资产：只承载“给自身或目标挂标签型削弱”的正式数据。 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_ActionHitDebuffEffectDefinition : public UDataAsset_ActionHitEffectDefinition
{
	GENERATED_BODY()

public:
	UDataAsset_ActionHitDebuffEffectDefinition();

	virtual bool IsValidDefinition() const override;
};
