#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionHitSourceTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionPresentationTypes.h"
#include "ActionHitTypes.generated.h"

class AActor;
struct FActionDamagePayload;

/** 受击后的最终解析结果类型。它表达公共业务结果，不直接绑定某个组件内部阶段，也不是 `CombatReact` 的正式阶段源。 */
UENUM(BlueprintType)
enum class EActionHitResultType : uint8
{
	None,
	Damaged,
	Blocked,
	GuardBroken,
	Parried,
	PerfectDodged,
	Ignored
};

/** 命中希望触发的受击类型。它表达公共受击意图，不等价于最终一定落地的表现结果，也不替代 `ActionCombatReactComponent` 的正式运行态。 */
UENUM(BlueprintType)
enum class EActionHitReactType : uint8
{
	None,
	HitStun,
	HeavyHitReact,
	Launch,
	Knockdown
};

/**
 * 武器或攻击定义里通用的静态命中模板。
 * 它描述“这类攻击天生带什么伤害、效果和受击语义”，不是本次命中的运行时实例。
 */
USTRUCT(BlueprintType)
struct FActionWeaponHitConfig
{
	GENERATED_BODY()

public:
	// 静态命中配置：
	// 这一层通常挂在武器定义或攻击分支配置中，描述“这类攻击天生带什么伤害与受击语义”。
	/** 生命伤害的新式等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Damage", meta = (ToolTip = "命中时扣除目标生命的等级驱动配置。它是正式直伤入口之一；留空时这次命中不会从生命伤害这一项产出数值。"))
	FActionAttributeDrivenValueConfig HealthDamageValueConfig;

	/** 普通格挡成立后消耗防守方体力的独立等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Damage", meta = (ToolTip = "普通格挡成立后消耗防守方体力的等级驱动配置。只在命中结果进入普通格挡链时消费。"))
	FActionAttributeDrivenValueConfig GuardStaminaCostValueConfig;

	/** 削韧伤害的新式等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Damage", meta = (ToolTip = "命中时削减目标韧性的等级驱动配置。它决定破韧推进量，不直接决定最终受击表现类型。"))
	FActionAttributeDrivenValueConfig PoiseDamageValueConfig;

	/** 命中成功后奖励给攻击方的特殊切武能量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "命中成功后奖励给攻击方的特殊切武能量。它只服务特殊切武能量积攒，不代表旧通用能量系统。设为 0 表示这次命中不提供额外切武能量。"))
	float SpecialWeaponSwitchEnergyRewardOnHit = 8.f;

	/** 命中事件附带的标签，便于区分轻击、重击、投技等来源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ToolTip = "这次命中的正式语义标签，例如轻击、重击、冲刺攻击或 Spirit 段落。后续日志、表现和条件分支会读取它。"))
	FGameplayTag HitTag;

	/** 命中后默认一定会附带的效果条目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Effect", meta = (ToolTip = "这次命中默认一定会附带的效果条目。它们来自这份静态命中模板的基础效果层。"))
	TArray<FActionHitEffectEntry> DefaultEffects;

	/** 命中源自身内建的额外效果条目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Effect", meta = (ToolTip = "这次命中源自身内建的额外效果条目。它们来自武器、攻击段或技能条目的附加效果层。"))
	TArray<FActionHitEffectEntry> AdditionalEffects;

	/** 命中成功后的默认表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|Presentation", meta = (ToolTip = "命中成功后的默认表现配置，例如伤害数字样式、特效和音效。"))
	FActionHitPresentationConfig HitPresentationConfig;

	/** 是否允许该攻击被普通格挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ToolTip = "这次命中是否允许被普通格挡。关闭后目标不会走普通格挡结算。"))
	bool bCanBeBlocked = true;

	/** 是否允许该攻击被精准格挡。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ToolTip = "这次命中是否允许被精准格挡。关闭后目标不会走精准格挡分支。"))
	bool bCanBeParried = true;

	/** 是否允许该攻击触发完美闪避。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit", meta = (ToolTip = "这次命中是否允许触发完美闪避。关闭后目标即使处于完美闪避窗口，也不会按该分支解析。"))
	bool bCanBePerfectDodged = true;

	/** 命中后默认进入的受击类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|React", meta = (ToolTip = "命中成功后默认希望触发的受击类型。最终是否真的落地，仍取决于目标状态和受击解析结果。"))
	EActionHitReactType HitReactType = EActionHitReactType::HitStun;

	/** 受击状态持续时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|React", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "目标进入对应受击状态后的持续时长，单位秒。只有 HitReactType 不是 None 时这里才真正有意义。"))
	float HitStateDuration = 0.35f;

	/** 击退强度。方向由运行时按攻击者到受击者的平面方向自动计算。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit|React", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "命中的击退强度。方向由运行时按攻击者到受击者的平面方向自动计算；设为 0 表示这次命中不额外提供击退。"))
	float KnockbackStrength = 0.f;

	/** 只判断这份静态命中模板是否已经配置了等级驱动伤害骨架。 */
	bool HasAnyLevelDrivenDamageConfig() const
	{
		return HealthDamageValueConfig.HasAnyConfiguredValue()
			|| GuardStaminaCostValueConfig.HasAnyConfiguredValue()
			|| PoiseDamageValueConfig.HasAnyConfiguredValue();
	}

	/** 只判断这份静态命中模板是否写入过任意正式字段。 */
	bool HasAnyConfiguredValue() const
	{
		const FActionWeaponHitConfig DefaultConfig;
		return !StaticStruct()->CompareScriptStruct(this, &DefaultConfig, 0);
	}

	/** 收集这份模板依赖的表现与效果资源路径，不推进命中结算。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		HitPresentationConfig.CollectSoftObjectPaths(OutAssetPaths);

		for (const FActionHitEffectEntry& EffectEntry : DefaultEffects)
		{
			EffectEntry.CollectSoftObjectPaths(OutAssetPaths);
		}

		for (const FActionHitEffectEntry& EffectEntry : AdditionalEffects)
		{
			EffectEntry.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};

/**
 * 处决专用静态命中模板。
 * 只保留处决真正会消费的共享字段，不和普通命中模板混成同一层。
 */
USTRUCT(BlueprintType)
struct FActionExecutionHitConfig
{
	GENERATED_BODY()

public:
	/** 生命伤害的新式等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExecutionHit|Damage", meta = (ToolTip = "处决命中时造成生命伤害的等级驱动配置。"))
	FActionAttributeDrivenValueConfig HealthDamageValueConfig;

	/** 削韧伤害的新式等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExecutionHit|Damage", meta = (ToolTip = "处决命中时削减目标韧性的等级驱动配置。"))
	FActionAttributeDrivenValueConfig PoiseDamageValueConfig;

	/** 命中成功后奖励给攻击方的特殊切武能量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExecutionHit", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "处决命中成功后奖励给攻击方的特殊切武能量。设为 0 表示处决不额外产出切武能量。"))
	float SpecialWeaponSwitchEnergyRewardOnHit = 8.f;

	/** 命中事件附带的标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExecutionHit", meta = (ToolTip = "处决命中的正式语义标签。"))
	FGameplayTag HitTag;

	/** 处决命中后默认一定会附带的效果条目。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ExecutionHit|Effect", meta = (ToolTip = "处决命中后默认一定会附带的效果条目。"))
	TArray<FActionHitEffectEntry> DefaultEffects;

	/** 只判断这份处决共享模板是否已经配置了等级驱动伤害骨架。 */
	bool HasAnyLevelDrivenDamageConfig() const
	{
		return HealthDamageValueConfig.HasAnyConfiguredValue()
			|| PoiseDamageValueConfig.HasAnyConfiguredValue();
	}

	/** 只判断这份处决共享模板是否已经写入过任意正式字段。 */
	bool HasAnyConfiguredValue() const
	{
		const FActionExecutionHitConfig DefaultConfig;
		return !StaticStruct()->CompareScriptStruct(this, &DefaultConfig, 0);
	}

	/** 收集处决默认效果依赖资源，不在这里做完整 payload 拼装。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		for (const FActionHitEffectEntry& EffectEntry : DefaultEffects)
		{
			EffectEntry.CollectSoftObjectPaths(OutAssetPaths);
		}
	}

	/** 把处决链共享语义写进一份正式实例载荷。它只做静态模板到运行时 payload 的桥接，不在这里直接触发命中解析。 */
	void ApplySharedExecutionPayloadSettings(FActionDamagePayload& InOutDamagePayload) const;

};

/**
 * 实际送入受击解析器的攻击载荷。
 * 说明：
 * 1. 这是运行时的“本次命中实例数据”，不是武器上的静态模板；
 * 2. 通常由武器命中配置、攻击方属性和当次上下文共同拼装而成；
 * 3. 进入受击解析器后，应优先消费这一层，而不是再回头读取武器资产。
 */
USTRUCT(BlueprintType)
struct FActionDamagePayload
{
	GENERATED_BODY()

public:
	// 命中来源与数值主体：
	// 这里记录的是一次真实命中在当前帧要送去结算的全部关键信息。
	/** 造成本次攻击的施加者，通常是角色本体。它属于这次实例载荷的来源快照，不回头查询当前装备态。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	/** 触发命中的来源对象，通常是武器或投射物。它只描述这次命中从哪里来，不等于长期命中窗口宿主。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** 生命伤害。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float BaseDamage = 0.f;

	/** 普通格挡成立后消耗防守方体力的成本。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float GuardStaminaCost = 0.f;

	/** 削韧伤害。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float PoiseDamage = 0.f;

	/** 冲击方向，后续可用于受击朝向、位移或镜头反馈。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FVector ImpactDirection = FVector::ZeroVector;

	/** 本次命中的正式来源语义。它是这次 payload 已拼装好的来源结果，不替代 `HeroHitSourceComponent` 的窗口状态。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FActionHitSourceInfo HitSource;

	/** 命中标签。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FGameplayTag HitTag;

	/** 本次命中的伤害大类。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 本次命中的具体元素子类型。只有元素伤害时才有意义。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FGameplayTag DamageElementTypeTag;

	/** 本次命中是否允许继续叠加外部额外效果层。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bAllowAdditionalHitEffects = true;

	/** 本次命中默认一定会附带的效果条目。它们来自静态命中模板的基础效果层。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Effect")
	TArray<FActionHitEffectEntry> DefaultEffects;

	/** 本次命中源自身内建的额外效果条目。它们来自武器或攻击源的附加效果层。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Effect")
	TArray<FActionHitEffectEntry> AdditionalEffects;

	/** 来自附魔、Buff 或临时技能的外部额外效果层。它们来自运行时外部聚合层。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Effect")
	TArray<FActionHitEffectEntry> ExternalAdditionalEffects;

	/** 本次命中已快照好的表现配置。它只是这次命中实例携带的展示指令，不反向变成新的表现状态源。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Presentation")
	FActionHitPresentationConfig HitPresentationConfig;

	/** 是否允许格挡。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bCanBeBlocked = true;

	/** 是否允许精准格挡。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bCanBeParried = true;

	/** 是否允许完美闪避。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bCanBePerfectDodged = true;

	/** 这次命中希望触发的受击类型。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|React")
	EActionHitReactType HitReactType = EActionHitReactType::HitStun;

	/** 这次受击状态持续时长。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|React")
	float HitStateDuration = 0.35f;

	/** 这次命中的击退强度。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|React")
	float KnockbackStrength = 0.f;

	/** 命中后奖励给攻击方的特殊切武能量。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float SpecialWeaponSwitchEnergyRewardOnHit = 0.f;

	/** 是否将这次命中视为“处决伤害”。 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Execution")
	bool bTreatAsExecutionHit = false;

	/**
	 * 处决伤害倍率覆写值。
	 * 1. 小于等于 0 时，表示改用攻击方属性集中的 ExecutionDamageMultiplier；
	 * 2. 大于 0 时，表示本次命中显式指定处决倍率；
	 * 3. 只有 bTreatAsExecutionHit 为 true 时，这个值才会参与结算。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Hit|Execution")
	float ExecutionDamageMultiplierOverride = 0.f;

	// 基础可用性判断：
	// 只有至少存在伤害、受击意图或正式 HitEffect 请求时，这次载荷才值得进入解析链。
	/** 判断这份正式实例载荷是否值得进入解析流程。它只做“要不要送进 `ActionHitResolver`”的最小判断，不代表一定能产出最终结果。 */
	bool IsValidPayload() const
	{
		return IsValid(SourceActor)
			&& (BaseDamage > 0.f
				|| GuardStaminaCost > 0.f
				|| PoiseDamage > 0.f
				|| HitReactType != EActionHitReactType::None
				|| HasAnyHitEffectSpecs());
	}

	/** 只判断这次实例是否携带了正式效果请求，不代表后续一定成功应用，也不表示效果生命周期已经建立。 */
	bool HasAnyHitEffectSpecs() const
	{
		return DefaultEffects.Num() > 0
			|| AdditionalEffects.Num() > 0
			|| ExternalAdditionalEffects.Num() > 0;
	}
};

/**
 * 受击解析后的结果数据。
 * 说明：
 * 1. 这是受击解析器对外返回的统一结算结果；
 * 2. 上层系统应读这里判断是否崩防、是否削韧击破、是否真正触发了受击；
 * 3. 不应再靠推测输入载荷去反推最终结果。
 * 它是最终解析结果层，不再回头承载静态配置或运行时 payload 语义，也不是新的长期状态宿主。
 */
USTRUCT(BlueprintType)
struct FActionHitResolveResult
{
	GENERATED_BODY()

public:
	// 最终结算输出：
	// 这里保存的是本次命中真正已经落地的结果，便于动画、状态机、调试输出继续消费。
	/** 本次受击的最终结果类型。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	EActionHitResultType ResultType = EActionHitResultType::Ignored;

	/** 实际结算到生命上的伤害。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float AppliedDamage = 0.f;

	/** 实际结算到体力上的防御耗体。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float AppliedGuardStaminaCost = 0.f;

	/** 实际结算到削韧上的伤害。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	float AppliedPoiseDamage = 0.f;

	/** 目标是否在这次结算后死亡。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bTargetDied = false;

	/** 目标是否在这次结算后被打空韧性。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bPoiseBroken = false;

	/** 目标是否在这次格挡结算后被打空体力，从而进入崩防状态。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bGuardBroken = false;

	/** 这次命中是否成功给目标或攻击者附着了任意一种正式效果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit")
	bool bAppliedHitEffect = false;

	/** 这次命中最终真正触发的受击类型。它表达最终落地的表现/控制结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit|React")
	EActionHitReactType AppliedHitReactType = EActionHitReactType::None;

	/** 这次命中是否被霸体吃掉了受击表现。它不等于本次命中整体无效。 */
	UPROPERTY(BlueprintReadOnly, Category = "Hit|React")
	bool bReactBlockedBySuperArmor = false;

	/** 判断这次命中是否真的落成了正式解析结果。它只回答“这次解析有没有正式结果”，不等于外层受击链是否已经完整收尾。 */
	bool WasResolved() const
	{
		return ResultType != EActionHitResultType::Ignored && ResultType != EActionHitResultType::None;
	}
};

inline void FActionExecutionHitConfig::ApplySharedExecutionPayloadSettings(FActionDamagePayload& InOutDamagePayload) const
{
	// 这里专门把处决链共享语义写入正式实例载荷。
	// 处决命中不再走普通格挡、完美闪避和外部额外效果层，因此统一在这里做最小收口。
	InOutDamagePayload.HitTag = HitTag;
	InOutDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit = FMath::Max(SpecialWeaponSwitchEnergyRewardOnHit, 0.f);
	InOutDamagePayload.DefaultEffects = DefaultEffects;
	InOutDamagePayload.AdditionalEffects.Reset();
	InOutDamagePayload.ExternalAdditionalEffects.Reset();
	InOutDamagePayload.HitPresentationConfig = FActionHitPresentationConfig();
	InOutDamagePayload.bAllowAdditionalHitEffects = false;
	InOutDamagePayload.bCanBeBlocked = false;
	InOutDamagePayload.bCanBeParried = false;
	InOutDamagePayload.bCanBePerfectDodged = false;
	InOutDamagePayload.HitReactType = EActionHitReactType::None;
	InOutDamagePayload.HitStateDuration = 0.f;
	InOutDamagePayload.KnockbackStrength = 0.f;
	InOutDamagePayload.bTreatAsExecutionHit = true;
	InOutDamagePayload.ExecutionDamageMultiplierOverride = 1.f;
}
