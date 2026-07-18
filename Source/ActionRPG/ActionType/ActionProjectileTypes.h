#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionPresentationTypes.h"
#include "ActionProjectileTypes.generated.h"

class AActor;
class AActionProjectileBase;

/** 当前一次发射物配置最终是从哪一层正式入口解析出来的。它只表达公共解析来源，不绑定具体组件阶段，也不表示发射物已经成功生成。 */
UENUM(BlueprintType)
enum class EActionResolvedProjectileConfigSource : uint8
{
	/** 当前没有解析到可用发射物配置。 */
	None,
	/** 来自武器定义上的默认发射物配置。 */
	DefaultProjectileConfig,
	/** 来自当前槽位选中的可切换发射物配置。 */
	SwitchableProjectileConfig,
	/** 来自当前攻击段或 Spirit 段的条目级发射物覆写。 */
	BranchProjectileOverride
};

/** 发射物命中有效目标后应如何继续处理自己的生命周期。它只表达模板策略，不直接驱动实例更新，也不单独构成发射物运行态。 */
UENUM(BlueprintType)
enum class EActionProjectileSuccessfulHitPolicy : uint8
{
	/** 成功命中后立即销毁这枚发射物。 */
	DestroyImmediately,
	/** 成功命中后继续飞行，通常配合最大命中目标数使用。 */
	ContinueFlight
};

/** 发射物撞到世界阻挡几何后应如何处理。它只表达公共世界阻挡策略，不等于实例已经真的撞到世界。 */
UENUM(BlueprintType)
enum class EActionProjectileWorldBlockPolicy : uint8
{
	/** 忽略世界阻挡反馈，继续按发射物自己的移动逻辑运行。 */
	IgnoreWorld,
	/** 命中世界阻挡后销毁发射物。 */
	DestroyOnBlock
};

/** 发射物对表现层广播的正式事件类型。它只表达反馈语义，不承担生命周期宿主职责，也不替代发射物真实销毁路径。 */
UENUM(BlueprintType)
enum class EActionProjectilePresentationEventType : uint8
{
	/** 发射物生成成功后的表现事件。 */
	Spawned,
	/** 发射物命中目标且命中解析正式落地后的表现事件。 */
	HitResolved,
	/** 发射物触发命中解析但最终被规则忽略后的表现事件。 */
	HitResolveIgnored,
	/** 发射物撞到世界阻挡后的表现事件。 */
	WorldBlocked,
	/** 发射物销毁时的表现事件。 */
	Destroyed
};

/** 发射物最终销毁时的正式原因。它只表达公共销毁归因，不等价于具体销毁实现路径，也不替代外层对 Destroy 时机的控制。 */
UENUM(BlueprintType)
enum class EActionProjectileDestroyReason : uint8
{
	/** 当前没有明确销毁原因，通常表示事件并非销毁事件。 */
	None,
	/** 成功命中后的生命周期策略要求销毁。 */
	SuccessfulHitPolicy,
	/** 成功命中目标数量达到上限后销毁。 */
	SuccessfulHitLimitReached,
	/** 撞到世界阻挡后销毁。 */
	WorldBlocked,
	/** 发射物生命周期超时后销毁。 */
	LifeSpanExpired,
	/** 外部逻辑主动销毁。 */
	ExternalDestroyed
};

/**
 * 当前攻击段发射物解析后的最小调试快照。
 * 这份结构只用于只读查询与调试输出，不作为新的权威状态源。
 */
USTRUCT(BlueprintType)
struct FActionProjectileResolutionDebugInfo
{
	GENERATED_BODY()

public:
	/** 当前攻击段是否启用了发射物生成。它只服务解析诊断，不表示发射物已经生成。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bHasProjectileSpawnRequest = false;

	/** 当前攻击段是否已经成功解析出一份可用发射物配置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bResolvedSuccessfully = false;

	/** 当前解析出的发射物配置来自默认、可切换列表还是条目级覆写。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource = EActionResolvedProjectileConfigSource::None;

	/** 当前槽位运行时选中的可切换发射物标签。它是诊断快照，不反向改变槽位选择。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SelectedProjectileConfigTag;

	/** 当前最终解析出的发射物语义标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ResolvedProjectileTag;

	/** 当前攻击段请求的发射 Socket。它只描述解析结果，不负责真正查找 Socket。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FName SpawnSocketName = NAME_None;

	/** 当前攻击段是否要求沿用武器默认发射物配置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bUseWeaponDefaultProjectileConfig = true;
};

/**
 * 发射物生成成功时一次性写入的初始化上下文。
 * 这份结构只保存生成当帧已经解析完毕的只读快照，避免后续表现层回头读取当前装备态。
 */
USTRUCT(BlueprintType)
struct FActionProjectileInitializationContext
{
	GENERATED_BODY()

public:
	/** 当前发射物配置最终来自哪一层入口。它是生成当帧写死的只读上下文，不会在飞行过程中跟着当前武器切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ToolTip = "当前发射物配置最终来自哪一层正式入口，例如默认配置、可切换配置或条目级覆写。"))
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;

	/** 当前槽位在生成这一枚发射物时选中的可切换配置标签。它只是当次生成快照，不反向改变装备域里的当前选择。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ToolTip = "当前槽位在生成这枚发射物时选中的可切换配置标签。未启用发射物切换时通常留空。"))
	FGameplayTag SelectedProjectileConfigTag;

	/** 当前攻击段请求使用的生成 Socket。它只是当次生成结果，不重新决定武器默认发射点配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ToolTip = "当前攻击段请求使用的生成 Socket。它是已经解析好的生成上下文快照，不再回头查询当前武器态。"))
	FName SpawnSocketName = NAME_None;
};

/**
 * 发射物对表现层广播的一次事件快照。
 * 这份结构只服务表现、调试与蓝图回调，不作为新的权威状态源。
 * 它描述的是“刚刚广播出去的这一条事件”，不是单枚发射物的完整生命周期宿主。
 */
USTRUCT(BlueprintType)
struct FActionProjectilePresentationEvent
{
	GENERATED_BODY()

public:
	/** 当前事件属于生成、命中、阻挡还是销毁。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectilePresentationEventType EventType =
		EActionProjectilePresentationEventType::Spawned;

	/** 若本次事件与销毁有关，这里记录正式销毁归因；否则保持 None。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileDestroyReason DestroyReason =
		EActionProjectileDestroyReason::None;

	/** 当前事件对应的发射物语义标签，供表现层和日志直接识别是哪一类弹体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ProjectileTag;

	/** 当前事件对应的伤害大类快照。它只服务表现与调试，不反向驱动伤害结算。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 若伤害大类为元素，这里记录对应的元素子类型；否则通常保持空标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag DamageElementTypeTag;

	/** 当前弹体命中成功后的生命周期策略快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileSuccessfulHitPolicy SuccessfulHitPolicy =
		EActionProjectileSuccessfulHitPolicy::DestroyImmediately;

	/** 当前弹体撞到世界阻挡后的生命周期策略快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileWorldBlockPolicy WorldBlockPolicy =
		EActionProjectileWorldBlockPolicy::IgnoreWorld;

	/** 这枚发射物到当前事件为止已成功结算了多少个目标。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	int32 SuccessfulResolvedTargetCount = 0;

	/** 这枚发射物允许成功结算的目标数量上限；小于等于 0 表示模板未设置上限。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	int32 MaxSuccessfulTargetHitCount = 0;

	/** 生成这枚发射物的施加者快照，通常是角色本体。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	/** 若这次事件与命中有关，这里记录命中的目标；否则保持空。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<AActor> TargetActor = nullptr;

	/** 当前事件对应的命中或阻挡位置；纯生成/销毁事件通常保持默认值。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FVector ImpactLocation = FVector::ZeroVector;

	/** 当前事件对应的命中或阻挡法线；纯生成/销毁事件通常保持默认值。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FVector ImpactNormal = FVector::ZeroVector;

	/** 当前事件若来自一次命中解析，这里记录最终结果类型。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionHitResultType HitResultType = EActionHitResultType::None;

	/** 当前事件广播后，这枚发射物是否会立刻销毁。它是事件快照，不是外层销毁命令。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bWillDestroyAfterEvent = false;

	/** 这枚发射物当前实际采用的是默认配置、切换配置还是条目级覆写。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;

	/** 若本次生成来自可切换发射物体系，这里记录当时选中的切换标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SelectedProjectileConfigTag;

	/** 这枚发射物当次生成时使用的 SpawnSocket 快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FName SpawnSocketName = NAME_None;
};

/**
 * 单类发射物的静态配置。
 * 这份结构体只描述“这类发射物本身是什么，以及它命中时附带什么命中效果和伤害语义”，
 * 不负责真正生成 Actor，也不直接承担完整弹道逻辑。
 * 它是静态发射物模板，不是运行中的发射物实例。
 */
USTRUCT(BlueprintType)
struct FActionProjectileConfig
{
	GENERATED_BODY()

public:
	/** 运行时真正要生成的发射物 Actor 类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "运行时真正要生成的发射物 Actor 类。留空时这份发射物配置无效，条目级或武器级发射请求都无法正式落地。"))
	TSoftClassPtr<AActionProjectileBase> ProjectileClass;

	/** 发射物语义标签，便于后续调试、筛选和表现层读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "发射物的正式语义标签。它主要服务调试、表现分流和可切换发射物识别，不替代输入标签。"))
	FGameplayTag ProjectileTag;

	/** 发射物命中时默认一定会附带的效果条目。它们来自这份静态模板的基础效果层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect", meta = (ToolTip = "发射物命中时默认一定会附带的效果条目。它们来自这份静态模板的基础效果层。"))
	TArray<FActionHitEffectEntry> DefaultEffects;

	/** 发射物自身内建的额外效果条目。它们来自这份静态模板的附加效果层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect", meta = (ToolTip = "发射物自身内建的额外效果条目。只有这枚发射物命中时才会携带它们。"))
	TArray<FActionHitEffectEntry> AdditionalEffects;

	/** 当前发射物是否允许继承武器槽外部额外效果层。它只决定模板是否接受外部聚合层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect", meta = (ToolTip = "这类发射物是否允许继承武器槽运行时聚合出来的外部额外命中效果层。关闭后只消费自己模板里的效果。"))
	bool bAllowInheritedAdditionalEffects = false;

	/** 发射物命中成功后的表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (ToolTip = "发射物命中成功时的默认表现配置，例如伤害数字、命中特效和音效。"))
	FActionHitPresentationConfig HitPresentationConfig;

	/** 发射物自身的生命伤害等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (ToolTip = "发射物命中时造成生命伤害的等级驱动配置。若这里和武器/攻击基础命中都未提供有效直伤，发射物命中将只剩表现或效果层。"))
	FActionAttributeDrivenValueConfig HealthDamageValueConfig;

	/** 发射物命中被普通格挡时消耗防守方体力的独立等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (ToolTip = "发射物命中被普通格挡时消耗防守方体力的等级驱动配置。"))
	FActionAttributeDrivenValueConfig GuardStaminaCostValueConfig;

	/** 发射物自身的削韧伤害等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (ToolTip = "发射物命中时削减目标韧性的等级驱动配置。"))
	FActionAttributeDrivenValueConfig PoiseDamageValueConfig;

	/** 发射物命中时默认使用的伤害大类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Semantic", meta = (ToolTip = "发射物命中时默认使用的伤害大类。只有设置为 Elemental 时，下面的元素标签才应配置。"))
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 发射物命中时默认使用的元素伤害子类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Semantic", meta = (EditCondition = "DamageType == EActionDamageType::Elemental", EditConditionHides, ToolTip = "发射物的具体元素伤害子类型，例如 Damage.Element.Fire。只在 DamageType 为 Elemental 时可配置。"))
	FGameplayTag DamageElementTypeTag;

	/** 发射物初速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0", ToolTip = "发射物的初速度。若 Projectile 类本身也有速度参数，这里作为框架级静态配置入口。"))
	float InitialSpeed = 1800.f;

	/** 发射物最大速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0", ToolTip = "发射物的最大速度上限。"))
	float MaxSpeed = 1800.f;

	/** 发射物重力比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ToolTip = "发射物的重力比例。0 表示不受重力影响；大于 0 时会产生抛物线。"))
	float GravityScale = 0.f;

	/** 发射物生命周期。小于等于 0 时表示沿用 Actor 默认寿命。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0", ToolTip = "发射物生命周期，单位秒。小于等于 0 时表示沿用发射物 Actor 自己的默认寿命。"))
	float LifeSeconds = 6.f;

	/** 发射物命中成功后应立即销毁，还是继续飞行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior", meta = (ToolTip = "发射物成功命中后是立刻销毁，还是继续飞行。若要穿透多个目标，通常应选择 ContinueFlight 并配合命中数量上限。"))
	EActionProjectileSuccessfulHitPolicy SuccessfulHitPolicy = EActionProjectileSuccessfulHitPolicy::DestroyImmediately;

	/** 发射物撞到世界阻挡时应忽略，还是立刻销毁。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior", meta = (ToolTip = "发射物撞到世界阻挡后应忽略还是销毁。"))
	EActionProjectileWorldBlockPolicy WorldBlockPolicy = EActionProjectileWorldBlockPolicy::IgnoreWorld;

	/** 一枚发射物最多允许成功命中多少个不同目标。小于等于 0 视为不限制。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior", meta = (ClampMin = "0", UIMin = "0", ToolTip = "一枚发射物最多允许成功命中多少个不同目标。小于等于 0 视为不限制。"))
	int32 MaxSuccessfulTargetHitCount = 0;

	/** 发射物生成成功时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (ToolTip = "发射物成功生成时的默认生命周期表现配置。"))
	FActionProjectileLifecyclePresentationConfig SpawnPresentationConfig;

	/** 发射物撞到世界阻挡时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (ToolTip = "发射物撞到世界阻挡时的默认生命周期表现配置。"))
	FActionProjectileLifecyclePresentationConfig WorldBlockedPresentationConfig;

	/** 发射物销毁时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (ToolTip = "发射物销毁时的默认生命周期表现配置。"))
	FActionProjectileLifecyclePresentationConfig DestroyedPresentationConfig;

public:
	/** 只判断这份静态发射物模板是否至少写过任意正式字段。它只服务静态模板读法，不代表运行时一定会选择这份模板。 */
	bool HasAnyConfiguredValue() const
	{
			return !ProjectileClass.IsNull()
			|| ProjectileTag.IsValid()
			|| DefaultEffects.Num() > 0
			|| AdditionalEffects.Num() > 0
			|| bAllowInheritedAdditionalEffects
			|| HitPresentationConfig.HasAnyConfiguredValue()
			|| HealthDamageValueConfig.HasAnyConfiguredValue()
			|| GuardStaminaCostValueConfig.HasAnyConfiguredValue()
			|| PoiseDamageValueConfig.HasAnyConfiguredValue()
			|| DamageType != EActionDamageType::Physical
			|| DamageElementTypeTag.IsValid()
			|| !FMath::IsNearlyZero(InitialSpeed - 1800.f)
			|| !FMath::IsNearlyZero(MaxSpeed - 1800.f)
			|| !FMath::IsNearlyZero(GravityScale)
			|| !FMath::IsNearlyZero(LifeSeconds - 6.f)
			|| SuccessfulHitPolicy != EActionProjectileSuccessfulHitPolicy::DestroyImmediately
			|| WorldBlockPolicy != EActionProjectileWorldBlockPolicy::IgnoreWorld
			|| MaxSuccessfulTargetHitCount > 0
			|| SpawnPresentationConfig.HasAnyConfiguredValue()
			|| WorldBlockedPresentationConfig.HasAnyConfiguredValue()
			|| DestroyedPresentationConfig.HasAnyConfiguredValue();
	}

	/** 只判断这份静态发射物模板是否具备最小可用入口。它只做模板合法性最小判断，不推进生成行为。 */
	bool IsValidConfig() const
	{
		return !ProjectileClass.IsNull();
	}

	/** 只判断这份静态发射物模板是否配置了自己的新式直伤骨架，不代表命中时一定只由它独立产出数值。 */
	bool HasAnyDamageConfig() const
	{
		return HealthDamageValueConfig.HasAnyConfiguredValue()
			|| GuardStaminaCostValueConfig.HasAnyConfiguredValue()
			|| PoiseDamageValueConfig.HasAnyConfiguredValue();
	}

	/** 从模板读取当前应采用的有效命中后生命周期策略，不推进运行时逻辑。 */
	EActionProjectileSuccessfulHitPolicy ResolveSuccessfulHitPolicy() const
	{
		return SuccessfulHitPolicy;
	}

	/** 从模板读取当前应采用的世界阻挡策略，不推进运行时逻辑。 */
	EActionProjectileWorldBlockPolicy ResolveWorldBlockPolicy() const
	{
		return WorldBlockPolicy;
	}

	/** 从模板读取最大成功命中目标数。小于等于 0 表示不限制。 */
	int32 ResolveMaxSuccessfulTargetHitCount() const
	{
		return MaxSuccessfulTargetHitCount > 0 ? MaxSuccessfulTargetHitCount : 0;
	}

	/** 收集这份静态模板依赖的软引用资源路径，供外层统一预热。它只服务预热，不构成第二套发射物状态。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!ProjectileClass.IsNull())
		{
			OutAssetPaths.AddUnique(ProjectileClass.ToSoftObjectPath());
		}

		HitPresentationConfig.CollectSoftObjectPaths(OutAssetPaths);
		for (const FActionHitEffectEntry& EffectEntry : DefaultEffects)
		{
			EffectEntry.CollectSoftObjectPaths(OutAssetPaths);
		}
		for (const FActionHitEffectEntry& EffectEntry : AdditionalEffects)
		{
			EffectEntry.CollectSoftObjectPaths(OutAssetPaths);
		}
		SpawnPresentationConfig.CollectSoftObjectPaths(OutAssetPaths);
		WorldBlockedPresentationConfig.CollectSoftObjectPaths(OutAssetPaths);
		DestroyedPresentationConfig.CollectSoftObjectPaths(OutAssetPaths);
	}
};

/**
 * 一条可切换的发射物配置入口。
 * 主要给法杖类远程武器使用，用标签标识不同发射物方案。
 * 它是“标签 -> 发射物模板”的公共配置入口，不是当前已选中的运行态。
 */
USTRUCT(BlueprintType)
struct FActionSwitchableProjectileConfigEntry
{
	GENERATED_BODY()

public:
	/** 这条可切换发射物配置的唯一标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "这条可切换发射物配置的唯一标签。只在开启发射物切换的武器上使用，且同一武器内必须保持唯一。"))
	FGameplayTag ProjectileConfigTag;

	/** 与该标签对应的发射物具体配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "与上面标签对应的发射物静态模板。"))
	FActionProjectileConfig ProjectileConfig;

public:
	/** 只判断这条可切换配置入口是否已经具备最小合法性。 */
	bool IsValidEntry() const
	{
		return ProjectileConfigTag.IsValid() && ProjectileConfig.IsValidConfig();
	}

	/** 收集这条配置入口依赖的软引用资源路径，不表达当前是否已被选中。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		ProjectileConfig.CollectSoftObjectPaths(OutAssetPaths);
	}
};

/**
 * 单次攻击条目要如何生成发射物的配置。
 * 这份结构体只回答“是否沿用武器默认发射物，以及从哪里发出”，
 * 真正的发射物内容优先来自武器默认配置，必要时再由条目级覆写补充。
 * 它是条目级 spawn 配置，不承担真正的发射物生成，也不是“当前已生成发射物”的运行态。
 */
USTRUCT(BlueprintType)
struct FActionProjectileSpawnConfig
{
	GENERATED_BODY()

public:
	/** 是否沿用武器定义上的默认发射物配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "当前攻击段或 Spirit 段是否直接沿用武器定义上的 DefaultProjectileConfig。开启时下面的条目级覆写不会参与正式解析。"))
	bool bUseWeaponDefaultProjectileConfig = true;

	/** 当不沿用武器默认配置时，这里提供当前攻击条目自己的发射物覆写。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "!bUseWeaponDefaultProjectileConfig", EditConditionHides, ToolTip = "当不沿用武器默认发射物配置时，这里填写当前攻击条目自己的发射物覆写。只在 bUseWeaponDefaultProjectileConfig 为 false 时可配置；若只是改出弹位置而不改弹体内容，保持沿用默认配置即可。"))
	FActionProjectileConfig ProjectileConfigOverride;

	/** 发射物从哪个 Socket 或骨骼位置发出。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ToolTip = "发射物从哪个 Socket 或骨骼位置发出。留空时会回退到武器或角色上的默认发射点逻辑。"))
	FName SpawnSocketName = NAME_None;

public:
	/** 只判断这份条目级 spawn 配置是否至少写了任意一项生成语义，不代表当前攻击段一定会真的生成发射物。 */
	bool HasAnyConfiguredValue() const
	{
		return !bUseWeaponDefaultProjectileConfig
			|| ProjectileConfigOverride.HasAnyConfiguredValue()
			|| SpawnSocketName != NAME_None;
	}

	/** 只判断当前是否真的写了条目级发射物覆写。 */
	bool HasProjectileConfigOverride() const
	{
		return ProjectileConfigOverride.HasAnyConfiguredValue();
	}

	/** 收集这份条目级 spawn 配置依赖的软引用资源路径，不负责真正生成 Actor。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!bUseWeaponDefaultProjectileConfig)
		{
			ProjectileConfigOverride.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};
