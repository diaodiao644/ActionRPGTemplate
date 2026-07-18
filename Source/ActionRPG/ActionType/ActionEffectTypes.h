#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "ActionEffectTypes.generated.h"

class UTexture2D;
class UDataAsset_ActionHitEffectDefinition;

/** 外部命中效果来源适用范围。它只表达公共业务语义，不直接绑定某个组件运行阶段。 */
UENUM(BlueprintType)
enum class EActionExternalHitEffectApplyScope : uint8
{
	MeleeOnly,
	ProjectileOnly,
	MeleeAndProjectile
};

/** 命中附加效果要施加给谁。它只表达目标侧别，不承担实际应用链路决策。 */
UENUM(BlueprintType)
enum class EActionHitEffectTargetSide : uint8
{
	Target,
	Self
};

/** 命中附加效果的小类。它只表达公共分类口径，不直接等价于具体 GE 实现。 */
UENUM(BlueprintType)
enum class EActionHitEffectKind : uint8
{
	Dot,
	Buff,
	Debuff
};

/**
 * 状态效果展示数据。
 * 这份结构体专门服务“状态效果在 UI 上应该怎么显示”，
 * 不参与数值结算，因此可以稳定复用到状态栏、详情面板和调试面板。
 */
USTRUCT(BlueprintType)
struct FActionStatusEffectDisplayData
{
	GENERATED_BODY()

public:
	// 纯展示数据：
	// 这一层只服务 UI 与调试面板，不参与数值结算，也不依赖运行时 ASC 状态。
	/** 状态效果在 UI 上显示的名称。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果在 UI 上显示的名称。它只服务展示，不承担正式标签语义。"))
	FText DisplayName;

	/** 状态效果在 UI 上显示的说明文本。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果在 UI 上显示的说明文本。适合给状态栏、详情面板或调试面板直接读取。"))
	FText Description;

	/** 状态效果在 UI 上的主色调。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果在 UI 上使用的主色调。"))
	FLinearColor DisplayColor = FLinearColor::White;

	/** 状态效果图标。当前阶段只做数据入口，后续 UI 可直接读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "状态效果图标资源。当前只作为展示入口，不参与效果结算。"))
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * 运行时活跃状态效果信息。
 * 这份结构体是 ASC 对外提供给 UI / 调试层的快照，
 * 目的是避免 Widget 直接深入 GE 结构读取底层数据。
 */
USTRUCT(BlueprintType)
struct FActionActiveStatusEffectInfo
{
	GENERATED_BODY()

public:
	// 运行时展示快照：
	// 这里保存的是“某个活跃状态效果当前对外显示成什么样”，
	// 适合给 UI、调试信息和蓝图读，不适合反向作为底层结算数据源。
	/** 当前状态效果对应的核心标签。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果的核心标签。它来自正式状态源快照，便于 UI 和调试层快速识别。"))
	FGameplayTag StatusEffectTag;

	/** 当前状态效果显示名称。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果在 UI 上显示的名称。它是只读快照，不应反向当成正式标签入口。"))
	FText DisplayName;

	/** 当前状态效果说明。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果在 UI 上显示的说明文本。"))
	FText Description;

	/** 当前状态效果显示色。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果在 UI 上使用的展示色。"))
	FLinearColor DisplayColor = FLinearColor::White;

	/** 当前状态效果图标。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果对应的图标资源。"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** 剩余持续时间。小于 0 表示无限时长。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果的剩余持续时间。小于 0 表示无限时长。"))
	float RemainingTime = 0.f;

	/** 总持续时间。小于 0 表示无限时长。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果的总持续时间。小于 0 表示无限时长。"))
	float TotalDuration = 0.f;

	/** 当前层数。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果的层数快照。"))
	int32 StackCount = 1;

	/** 对应的活跃 GameplayEffect Handle，便于后续做手动移除或调试。 */
	UPROPERTY(BlueprintReadOnly, Category = "StatusEffect", meta = (ToolTip = "当前活跃状态效果对应的 GameplayEffect Handle。它主要服务调试或手动移除，不是稳定存档键。"))
	FActiveGameplayEffectHandle ActiveEffectHandle;
};

/**
 * 一条命中附加效果入口。
 * 正式语义只收三件事：
 * 1. 这条效果作用到攻击者自己还是命中目标；
 * 2. 它属于 DOT / Buff / Debuff 中的哪一类；
 * 3. 真正的持续时间、标签和 DOT 参数全部回到专用效果资产。
 * 这是一条公共 HitEffect 条目，不是已经进入运行时应用链的效果实例。
 */
USTRUCT(BlueprintType)
struct FActionHitEffectEntry
{
	GENERATED_BODY()

public:
	/** 当前效果作用在命中目标还是攻击者自己。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffect", meta = (ToolTip = "这条命中效果最终施加给命中目标还是攻击者自己。它只决定目标侧别，不改变效果定义资产本身。"))
	EActionHitEffectTargetSide TargetSide = EActionHitEffectTargetSide::Target;

	/** 当前效果属于 DOT / Buff / Debuff 中的哪一类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffect", meta = (ToolTip = "这条命中效果的公共分类。应与实际引用的效果定义资产语义一致，用于运行时校验和调试。"))
	EActionHitEffectKind EffectKind = EActionHitEffectKind::Debuff;

	/** 当前条目正式引用的效果配置资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffect", meta = (ToolTip = "真正描述持续时间、标签或 DOT 参数的效果定义资产。留空时这条命中效果条目无效。"))
	TObjectPtr<UDataAsset_ActionHitEffectDefinition> EffectDefinition = nullptr;

public:
	/** 只判断这条公共条目是否至少配置了正式效果资产。 */
	bool HasAnyConfiguredValue() const
	{
		return EffectDefinition != nullptr;
	}

	/** 只校验条目自身是否可进入后续链路，不在这里推进效果应用。 */
	bool IsValidEntry() const
	{
		return EffectDefinition != nullptr;
	}

	/** 收集这条条目依赖的软资源路径，供外层做预加载或资源整理。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (EffectDefinition)
		{
			OutAssetPaths.AddUnique(FSoftObjectPath(EffectDefinition));
		}
	}
};

/**
 * 通用持续战斗修正请求。
 * 这份结构体用于把“持续几秒、附加哪些标签、修改哪些战斗修正属性”统一收口成一份请求数据，
 * 方便 GA、组件或蓝图都通过同一条入口生成运行时 CombatModifier GE。
 * 它是持续修正规格，不是已经运行中的 GE 状态。
 */
USTRUCT(BlueprintType)
struct FActionCombatModifierEffectSpec
{
	GENERATED_BODY()

public:
	// 持续战斗修正主体：
	// 这一层描述的是“想给目标挂一个持续几秒的战斗修正效果”，
	// 真正运行时会再被转换成 GE 或其他可执行表现。
	/** 持续时间，单位秒。小于等于 0 时视为非法请求。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ClampMin = "0.0", ToolTip = "持续战斗修正的持续时间，单位秒。小于等于 0 时这份修正规格不会进入正式执行链。"))
	float Duration = 0.f;

	/** 当前修正效果在 UI / 调试层上的主状态标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "这份修正效果在 UI / 调试层使用的主状态标签。它主要服务展示和查询，不替代 GrantedTags。"))
	FGameplayTag StatusEffectTag;

	/** 当前修正效果实际授予给目标 ASC 的运行时标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "这份修正效果真正授予给目标 ASC 的运行时标签集合。需要它来表达正式能力门禁或状态语义时再填写。"))
	FGameplayTagContainer GrantedTags;

	/** 对伤害易伤系数属性的加法修正量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "对伤害易伤系数属性的加法修正量。正值通常表示目标更容易受到伤害。"))
	float DamageVulnerabilityDelta = 0.f;

	/** 对生命伤害抗性属性的加法修正量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "对生命伤害抗性属性的加法修正量。"))
	float HealthDamageResistanceDelta = 0.f;

	/** 对防御耗体抗性属性的加法修正量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "对普通格挡耗体抗性属性的加法修正量。"))
	float GuardStaminaCostResistanceDelta = 0.f;

	/** 对韧性伤害抗性属性的加法修正量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "对削韧伤害抗性属性的加法修正量。"))
	float PoiseDamageResistanceDelta = 0.f;

	/** 对处决伤害倍率属性的加法修正量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombatModifier", meta = (ToolTip = "对处决伤害倍率属性的加法修正量。"))
	float ExecutionDamageMultiplierDelta = 0.f;

	// 基础校验：
	// 先判断是否真的填了内容，再判断持续时间是否合法，避免生成“空效果”。
	/** 判断当前是否已经填写了任意一项战斗修正效果配置。 */
	bool HasAnyConfiguredValue() const
	{
		return Duration > 0.f
			|| StatusEffectTag.IsValid()
			|| GrantedTags.Num() > 0
			|| !FMath::IsNearlyZero(DamageVulnerabilityDelta)
			|| !FMath::IsNearlyZero(HealthDamageResistanceDelta)
			|| !FMath::IsNearlyZero(GuardStaminaCostResistanceDelta)
			|| !FMath::IsNearlyZero(PoiseDamageResistanceDelta)
			|| !FMath::IsNearlyZero(ExecutionDamageMultiplierDelta);
	}

	/** 判断这份持续修正规格是否值得进入后续执行链。 */
	bool IsValidSpec() const
	{
		return Duration > 0.f && HasAnyConfiguredValue();
	}
};

/**
 * 一条具名的外部命中效果来源记录。
 * 用途：
 * 1. 让附魔、Buff、临时技能都能带着自己的来源标签写入武器槽；
 * 2. 后续移除某一类外部效果时，只需要按来源标签删除，不会误伤其他来源；
 * 3. 运行时最终仍会汇总成一条聚合数组，供命中链直接读取。
 * 它是 source entry 语义数据，不是高层 request，也不是 runtime 壳。
 */
USTRUCT(BlueprintType)
struct FActionExternalHitEffectSourceEntry
{
	GENERATED_BODY()

public:
	/** 这条外部命中效果来源的唯一标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这条外部命中效果来源的唯一来源标签。后续覆盖、刷新或移除同类来源都依赖它。"))
	FGameplayTag SourceTag;

	/** 这条来源允许投递到哪些命中路径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这条来源允许投递到近战命中、发射物命中还是两者都投递。"))
	EActionExternalHitEffectApplyScope ApplyScope = EActionExternalHitEffectApplyScope::MeleeAndProjectile;

	/** 这条来源当前提供给武器槽的效果集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这条来源当前提供给武器槽的效果集合。"))
	TArray<FActionHitEffectEntry> HitEffects;

	/** 判断这条来源记录是否至少具备有效来源标签。 */
	bool IsValidSource() const
	{
		return SourceTag.IsValid();
	}

	/** 只回答这条 source entry 是否允许近战命中继承，不决定实际聚合。 */
	bool AppliesToMelee() const
	{
		return ApplyScope == EActionExternalHitEffectApplyScope::MeleeOnly
			|| ApplyScope == EActionExternalHitEffectApplyScope::MeleeAndProjectile;
	}

	/** 只回答这条 source entry 是否允许发射物命中继承，不决定实际聚合。 */
	bool AppliesToProjectile() const
	{
		return ApplyScope == EActionExternalHitEffectApplyScope::ProjectileOnly
			|| ApplyScope == EActionExternalHitEffectApplyScope::MeleeAndProjectile;
	}
};

/**
 * 高层外部命中效果来源写入请求。
 * 这份结构体只描述“想把什么来源写进去”，
 * 不负责描述目标槽位；目标由装备组件的两套入口函数分别决定。
 * 它是高层 request，不表达目标槽位当前长期状态。
 */
USTRUCT(BlueprintType)
struct FActionExternalHitEffectSourceApplyRequest
{
	GENERATED_BODY()

public:
	/** 这次写入请求的来源标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这次外部命中效果来源写入请求的唯一来源标签。后续更新、覆盖或移除同类来源都依赖它。"))
	FGameplayTag SourceTag;

	/** 这条来源允许投递到哪些命中路径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这条来源允许投递到近战命中、发射物命中还是两者都投递。"))
	EActionExternalHitEffectApplyScope ApplyScope = EActionExternalHitEffectApplyScope::MeleeAndProjectile;

	/** 这次请求准备写入的正式效果集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这次请求要写入的正式效果集合。至少要有一条有效 HitEffect 条目，写入请求才会成立。"))
	TArray<FActionHitEffectEntry> HitEffects;

	/** 当前是否启用自动过期限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "是否为这条外部命中效果来源启用自动过期。关闭时这条来源会持续存在，直到被显式覆盖或移除。"))
	bool bUseDurationLimit = false;

	/** 若启用自动过期限制，这里填写持续时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ClampMin = "0.0", EditCondition = "bUseDurationLimit", EditConditionHides, ToolTip = "启用自动过期限制后，这里填写持续时间秒数。只在 bUseDurationLimit 为 true 时可配置。"))
	float Duration = 0.f;

	/** 判断这份高层 request 是否至少满足基础结构要求。 */
	bool IsValidRequest() const
	{
		if (!SourceTag.IsValid() || HitEffects.Num() <= 0)
		{
			return false;
		}

		if (bUseDurationLimit && Duration <= 0.f)
		{
			return false;
		}

		return true;
	}
};

/**
 * 高层 direct 外部额外命中效果写入请求。
 * 这层不带来源标签，也不带单独生命周期；
 * 语义固定为“直接覆盖目标槽位当前这层 direct 数组”。
 * 它与 source request 平行存在，不混成同一条写入口。
 */
USTRUCT(BlueprintType)
struct FActionExternalAdditionalHitEffectsApplyRequest
{
	GENERATED_BODY()

public:
	/** 这次请求准备写入 direct 层的命中效果集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffect", meta = (ToolTip = "这次请求准备直接写入目标槽位 direct 层的命中效果集合。它不带来源标签，也不带单独生命周期。"))
	TArray<FActionHitEffectEntry> HitEffects;

	/** 判断这份 direct 层请求是否至少带了一条待写入效果。 */
	bool IsValidRequest() const
	{
		return HitEffects.Num() > 0;
	}
};

/**
 * 槽位绑定型外部命中效果来源的运行时状态。
 * 说明：
 * 1. SourceEntry 负责保存这条来源当前的正式语义数据；
 * 2. DurationLimit 负责保存这条来源是否有生命周期限制；
 * 3. 这份状态只存在于固定武器槽运行时状态里，不直接参与命中结算；
 * 4. 真正给命中链使用的仍然是聚合缓存数组。
 * 它是单来源 runtime 壳，不是外部调用侧看到的高层结果。
 */
USTRUCT(BlueprintType)
struct FActionExternalHitEffectSourceRuntimeState
{
	GENERATED_BODY()

public:
	/** 这条来源当前的正式语义数据。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "这条来源当前的正式语义数据，包括来源标签、适用范围和效果集合。"))
	FActionExternalHitEffectSourceEntry SourceEntry;

	/** 当前是否存在自动过期限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "当前这条来源是否存在自动过期限制。它是运行时生命周期元数据，不是资产作者手动长期维护的配置入口。"))
	bool bHasDurationLimit = false;

	/** 若存在自动过期限制，对应的世界到期时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitEffectSource", meta = (ToolTip = "若存在自动过期限制，对应的世界到期时间。它是运行时元数据，不是资产配置入口。"))
	float ExpireWorldTime = 0.f;

	/** 自动过期计时器句柄。 */
	FTimerHandle ExpireTimerHandle;

	/** 判断当前 runtime 壳是否至少持有一条有效来源。 */
	bool IsValidRuntimeState() const
	{
		return SourceEntry.IsValidSource();
	}

	/** 只清当前这条来源的生命周期元数据，不删除来源语义数据本身。 */
	void ClearDurationLimit()
	{
		bHasDurationLimit = false;
		ExpireWorldTime = 0.f;
		ExpireTimerHandle = FTimerHandle();
	}
};

/**
 * 外部命中效果来源调试快照。
 * 这份结构体只服务蓝图调试、日志验证与后续 Buff / 附魔桥接，不参与正式结算。
 * 它只镜像当前来源对外可观察结果，不替代 runtime 壳或聚合数组。
 */
USTRUCT(BlueprintType)
struct FActionExternalHitEffectSourceDebugSnapshot
{
	GENERATED_BODY()

public:
	/** 来源标签。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	FGameplayTag SourceTag;

	/** 适用范围。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	EActionExternalHitEffectApplyScope ApplyScope = EActionExternalHitEffectApplyScope::MeleeAndProjectile;

	/** 当前是否带自动过期限制。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	bool bHasDurationLimit = false;

	/** 当前剩余时间；小于 0 表示无限时长。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	float RemainingTime = -1.f;

	/** 当前是否因武器权限不兼容而被挂起。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	bool bSuppressedByCurrentWeaponPolicy = false;

	/** 当前这条来源里一共配置了多少条效果。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	int32 EffectCount = 0;
};
