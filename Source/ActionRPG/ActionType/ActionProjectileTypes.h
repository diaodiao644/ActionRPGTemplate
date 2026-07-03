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

/** 当前一次发射物配置最终是从哪一层正式入口解析出来的。它只表达公共解析来源，不绑定具体组件阶段。 */
UENUM(BlueprintType)
enum class EActionResolvedProjectileConfigSource : uint8
{
	None,
	DefaultProjectileConfig,
	SwitchableProjectileConfig,
	BranchProjectileOverride
};

/** 发射物命中有效目标后应如何继续处理自己的生命周期。它只表达模板策略，不直接驱动实例更新。 */
UENUM(BlueprintType)
enum class EActionProjectileSuccessfulHitPolicy : uint8
{
	DestroyImmediately,
	ContinueFlight
};

/** 发射物撞到世界阻挡几何后应如何处理。它只表达公共世界阻挡策略。 */
UENUM(BlueprintType)
enum class EActionProjectileWorldBlockPolicy : uint8
{
	IgnoreWorld,
	DestroyOnBlock
};

/** 发射物对表现层广播的正式事件类型。它只表达反馈语义，不承担生命周期宿主职责。 */
UENUM(BlueprintType)
enum class EActionProjectilePresentationEventType : uint8
{
	Spawned,
	HitResolved,
	HitResolveIgnored,
	WorldBlocked,
	Destroyed
};

/** 发射物最终销毁时的正式原因。它只表达公共销毁归因，不等价于具体销毁实现路径。 */
UENUM(BlueprintType)
enum class EActionProjectileDestroyReason : uint8
{
	None,
	SuccessfulHitPolicy,
	SuccessfulHitLimitReached,
	WorldBlocked,
	LifeSpanExpired,
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
	/** 当前攻击段是否启用了发射物生成。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bHasProjectileSpawnRequest = false;

	/** 当前攻击段是否已经成功解析出一份可用发射物配置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bResolvedSuccessfully = false;

	/** 当前解析出的发射物配置来自默认、可切换列表还是条目级覆写。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource = EActionResolvedProjectileConfigSource::None;

	/** 当前槽位运行时选中的可切换发射物标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SelectedProjectileConfigTag;

	/** 当前最终解析出的发射物语义标签。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ResolvedProjectileTag;

	/** 当前攻击段请求的发射 Socket。 */
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
	/** 当前发射物配置最终来自哪一层入口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;

	/** 当前槽位在生成这一枚发射物时选中的可切换配置标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FGameplayTag SelectedProjectileConfigTag;

	/** 当前攻击段请求使用的生成 Socket。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FName SpawnSocketName = NAME_None;
};

/**
 * 发射物对表现层广播的一次事件快照。
 * 这份结构只服务表现、调试与蓝图回调，不作为新的权威状态源。
 */
USTRUCT(BlueprintType)
struct FActionProjectilePresentationEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectilePresentationEventType EventType =
		EActionProjectilePresentationEventType::Spawned;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileDestroyReason DestroyReason =
		EActionProjectileDestroyReason::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ProjectileTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionDamageType DamageType = EActionDamageType::Physical;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag DamageElementTypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileSuccessfulHitPolicy SuccessfulHitPolicy =
		EActionProjectileSuccessfulHitPolicy::DestroyImmediately;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionProjectileWorldBlockPolicy WorldBlockPolicy =
		EActionProjectileWorldBlockPolicy::IgnoreWorld;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	int32 SuccessfulResolvedTargetCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	int32 MaxSuccessfulTargetHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FVector ImpactLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FVector ImpactNormal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionHitResultType HitResultType = EActionHitResultType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bWillDestroyAfterEvent = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SelectedProjectileConfigTag;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSoftClassPtr<AActionProjectileBase> ProjectileClass;

	/** 发射物语义标签，便于后续调试、筛选和表现层读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ProjectileTag;

	/** 发射物命中时默认一定会附带的效果条目。它们来自这份静态模板的基础效果层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect")
	TArray<FActionHitEffectEntry> DefaultEffects;

	/** 发射物自身内建的额外效果条目。它们来自这份静态模板的附加效果层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect")
	TArray<FActionHitEffectEntry> AdditionalEffects;

	/** 当前发射物是否允许继承武器槽外部额外效果层。它只决定模板是否接受外部聚合层。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Effect")
	bool bAllowInheritedAdditionalEffects = false;

	/** 发射物命中成功后的表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	FActionHitPresentationConfig HitPresentationConfig;

	/** 发射物自身的生命伤害等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	FActionAttributeDrivenValueConfig HealthDamageValueConfig;

	/** 发射物命中被普通格挡时消耗防守方体力的独立等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	FActionAttributeDrivenValueConfig GuardStaminaCostValueConfig;

	/** 发射物自身的削韧伤害等级驱动配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	FActionAttributeDrivenValueConfig PoiseDamageValueConfig;

	/** 发射物命中时默认使用的伤害大类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Semantic")
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 发射物命中时默认使用的元素伤害子类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Semantic")
	FGameplayTag DamageElementTypeTag;

	/** 发射物初速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1800.f;

	/** 发射物最大速度。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 1800.f;

	/** 发射物重力比例。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement")
	float GravityScale = 0.f;

	/** 发射物生命周期。小于等于 0 时表示沿用 Actor 默认寿命。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float LifeSeconds = 6.f;

	/** 发射物命中成功后应立即销毁，还是继续飞行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior")
	EActionProjectileSuccessfulHitPolicy SuccessfulHitPolicy = EActionProjectileSuccessfulHitPolicy::DestroyImmediately;

	/** 发射物撞到世界阻挡时应忽略，还是立刻销毁。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior")
	EActionProjectileWorldBlockPolicy WorldBlockPolicy = EActionProjectileWorldBlockPolicy::IgnoreWorld;

	/** 一枚发射物最多允许成功命中多少个不同目标。小于等于 0 视为不限制。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Behavior")
	int32 MaxSuccessfulTargetHitCount = 0;

	/** 发射物生成成功时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	FActionProjectileLifecyclePresentationConfig SpawnPresentationConfig;

	/** 发射物撞到世界阻挡时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	FActionProjectileLifecyclePresentationConfig WorldBlockedPresentationConfig;

	/** 发射物销毁时的生命周期表现配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	FActionProjectileLifecyclePresentationConfig DestroyedPresentationConfig;

public:
	/** 只判断这份静态发射物模板是否至少写过任意正式字段。 */
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

	/** 只判断这份静态发射物模板是否具备最小可用入口。 */
	bool IsValidConfig() const
	{
		return !ProjectileClass.IsNull();
	}

	/** 只判断这份静态发射物模板是否配置了自己的新式直伤骨架。 */
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

	/** 收集这份静态模板依赖的软引用资源路径，供外层统一预热。 */
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ProjectileConfigTag;

	/** 与该标签对应的发射物具体配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
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
 * 它是条目级 spawn 配置，不承担真正的发射物生成。
 */
USTRUCT(BlueprintType)
struct FActionProjectileSpawnConfig
{
	GENERATED_BODY()

public:
	/** 是否沿用武器定义上的默认发射物配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bUseWeaponDefaultProjectileConfig = true;

	/** 当不沿用武器默认配置时，这里提供当前攻击条目自己的发射物覆写。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FActionProjectileConfig ProjectileConfigOverride;

	/** 发射物从哪个 Socket 或骨骼位置发出。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FName SpawnSocketName = NAME_None;

public:
	/** 只判断这份条目级 spawn 配置是否至少写了任意一项生成语义。 */
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
