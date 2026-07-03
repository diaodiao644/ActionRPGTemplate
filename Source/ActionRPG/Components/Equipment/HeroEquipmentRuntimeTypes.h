// 文件说明：声明英雄装备域本地运行时壳。

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "HeroEquipmentRuntimeTypes.generated.h"

class AHeroWeaponBase;
struct FStreamableHandle;

/**
 * 固定武器槽属性缓存 / 发射物标签上下文运行时壳。
 * 只服务当前槽位上下文镜像，不作为跨系统共享正式类型。
 * 正式状态仍由 `HeroLoadoutContextComponent` 持有；这份壳只承载它的 private runtime 数据。
 */
USTRUCT()
struct FHeroLoadoutContextRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前槽位缓存的武器属性增量。 */
	UPROPERTY(Transient)
	FActionWeaponAttributeCacheData WeaponAttributeCache;

	/** 当前槽位是否已经持有与当前武器定义对应的属性缓存。 */
	UPROPERTY(Transient)
	bool bHasWeaponAttributeCache = false;

	/** 当前槽位属性缓存来源的武器 Tag。 */
	UPROPERTY(Transient)
	FGameplayTag WeaponAttributeCacheSourceTag;

	/** 当前槽位选中的可切换发射物配置标签。只有支持切换发射物的远程武器才会使用。 */
	UPROPERTY(Transient)
	FGameplayTag SelectedProjectileConfigTag;

	/** 只重置当前槽位 context private runtime，不越权清理其它宿主状态。 */
	void Reset()
	{
		WeaponAttributeCache.Reset();
		bHasWeaponAttributeCache = false;
		WeaponAttributeCacheSourceTag = FGameplayTag();
		SelectedProjectileConfigTag = FGameplayTag();
	}
};

/**
 * 固定武器槽上的外部命中效果来源生命周期运行时壳。
 * 这份结构只承载 `direct / source states / aggregated mirrors / suppressed` 这一层装备域 private runtime，
 * 不作为跨系统共享正式状态。
 */
USTRUCT()
struct FHeroLoadoutHitEffectRuntimeState
{
	GENERATED_BODY()

public:
	/** direct 外部额外命中效果层。 */
	UPROPERTY()
	TArray<FActionHitEffectEntry> DirectExternalAdditionalHitEffects;

	/** 当前槽位按来源标签持有的外部命中效果来源运行时状态。 */
	UPROPERTY()
	TMap<FGameplayTag, FActionExternalHitEffectSourceRuntimeState> ExternalHitEffectSourceStates;

	/** 当前聚合后的近战外部额外命中效果镜像。 */
	UPROPERTY()
	TArray<FActionHitEffectEntry> ExternalAdditionalHitEffects;

	/** 当前聚合后的发射物可继承外部额外命中效果镜像。 */
	UPROPERTY()
	TArray<FActionHitEffectEntry> ProjectileInheritedExternalAdditionalHitEffects;

	/** 当前是否因武器策略而处于挂起状态。 */
	UPROPERTY()
	bool bSuppressedByCurrentWeaponPolicy = true;

	/** 只重置当前槽位 effect lifecycle private runtime，不替代 context 或 equipment 收尾。 */
	void Reset()
	{
		DirectExternalAdditionalHitEffects.Reset();
		ExternalHitEffectSourceStates.Reset();
		ExternalAdditionalHitEffects.Reset();
		ProjectileInheritedExternalAdditionalHitEffects.Reset();
		bSuppressedByCurrentWeaponPolicy = true;
	}
};

/**
 * 固定武器槽启动/预热状态机运行时壳。
 * 只服务 startup prewarm 与 UI 进度，不作为跨系统共享正式类型。
 * 它不替代 equipment/runtime/context 宿主，只记录启动状态机自己的 private runtime。
 */
USTRUCT()
struct FHeroWeaponLoadoutStartupRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前 startup prewarm 状态机所处阶段。 */
	UPROPERTY()
	EHeroWeaponLoadoutStartupState StartupState = EHeroWeaponLoadoutStartupState::None;

	/** 当前 startup 要求最终落地的出生槽。 */
	UPROPERTY()
	EHeroWeaponLoadoutSlot StartupSpawnLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	/** 当前仍未完成 startup prewarm 的槽位数量。 */
	UPROPERTY()
	int32 PendingStartupPrewarmSlotCount = 0;

	/** 本次 startup prewarm 需要处理的总槽位数量。 */
	UPROPERTY()
	int32 TotalStartupPrewarmSlotCount = 0;

	/** 当前 startup 失败原因文本。 */
	UPROPERTY()
	FString FailureReason;

	/** 当前仍处于 startup prewarm 待完成集合中的槽位。 */
	UPROPERTY()
	TSet<EHeroWeaponLoadoutSlot> PendingStartupPrewarmSlots;

	/** 重置 startup private runtime，并切入指定出生槽对应的 InProgress 阶段。 */
	void Reset(const EHeroWeaponLoadoutSlot InSpawnLoadoutSlot)
	{
		StartupState = EHeroWeaponLoadoutStartupState::InProgress;
		StartupSpawnLoadoutSlot = InSpawnLoadoutSlot;
		PendingStartupPrewarmSlotCount = 0;
		TotalStartupPrewarmSlotCount = 0;
		FailureReason.Reset();
		PendingStartupPrewarmSlots.Reset();
	}

	/** 把 startup private runtime 清回 None，不触碰 runtime/equipment 的正式运行态。 */
	void ResetToNone()
	{
		StartupState = EHeroWeaponLoadoutStartupState::None;
		StartupSpawnLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;
		PendingStartupPrewarmSlotCount = 0;
		TotalStartupPrewarmSlotCount = 0;
		FailureReason.Reset();
		PendingStartupPrewarmSlots.Reset();
	}
};

/**
 * 固定武器槽资源链 / 实例链本地运行时壳。
 * 这份结构只服务 definition 加载、runtime assets 预热、实例生命周期与 `LoadRevision` 校验。
 * 正式当前装备态仍由 `HeroEquipmentComponent` 写回；这里不承接跨系统装备宿主语义。
 */
USTRUCT()
struct FHeroLoadoutRuntimeAssetState
{
	GENERATED_BODY()

public:
	/** 当前已经加载进内存的武器定义资产。 */
	UPROPERTY(Transient)
	TObjectPtr<UDataAsset_WeaponDefinition> LoadedWeaponDefinition = nullptr;

	/**
	 * 当前固定武器槽已经生成好的武器实例。
	 * 说明：
	 * 1. 只要某把武器被装进固定武器槽，并且资源预热完成，就会立即实例化；
	 * 2. 未装备到手上的武器实例会被隐藏并关闭碰撞，但不会在切换时再临时生成。
	 */
	UPROPERTY(Transient)
	TObjectPtr<AHeroWeaponBase> SpawnedWeaponInstance = nullptr;

	/** 当前固定武器槽武器定义的异步加载句柄。 */
	TSharedPtr<FStreamableHandle> DefinitionLoadHandle;

	/** 当前固定武器槽武器运行时资源的异步预热句柄。 */
	TSharedPtr<FStreamableHandle> RuntimeAssetLoadHandle;

	/**
	 * 当前槽位的资源版本号。
	 * 每当这个槽位换上新武器，或把原武器卸下时，都要递增一次。
	 * 异步回调回来后会检查版本号，只有版本匹配才允许继续处理。
	 * 这样可以避免旧请求在晚于新请求完成时，把新的槽位状态覆盖掉。
	 */
	int32 LoadRevision = 0;

	/** 推进一次当前槽位的资源版本号，并返回推进后的新值；它只让旧异步链失效，不直接改装备结果。 */
	int32 AdvanceLoadRevision()
	{
		++LoadRevision;
		return LoadRevision;
	}
};

/**
 * 单个固定武器槽的装备总控运行时状态。
 * 这份结构只保留固定槽配置与能力句柄，不再并行承接 runtime / context / effect 子链正式状态。
 * `HeroEquipmentComponent` 继续作为装备域 public 宿主；这份壳只是它的当前槽位 private runtime 容器。
 */
USTRUCT()
struct FHeroWeaponLoadoutRuntimeState
{
	GENERATED_BODY()

public:
	/** 这个运行时状态对应的固定武器槽配置。 */
	UPROPERTY()
	FHeroWeaponLoadoutDefinition LoadoutDefinition;

	/**
	 * 这个固定武器槽当前已授予给角色的能力句柄集合。
	 * 当前版本改为“槽位 GA 常驻”后，这些句柄会在 Loadout 初始化时一次性授予，
	 * 后续切槽时不再反复移除和重授，只在真正重置 Loadout 或销毁组件时才需要清理。
	 */
	TArray<FGameplayAbilitySpecHandle> GrantedLoadoutAbilityHandles;

	/**
	 * 这个固定武器槽当前武器定义额外授予给角色的能力句柄集合。
	 * 这一层只服务“当前装备后才生效”的武器定义能力，例如灵武器专属能力。
	 */
	TArray<FGameplayAbilitySpecHandle> GrantedWeaponDefinitionAbilityHandles;

	/** 判断当前固定武器槽是否已经配置了默认武器软引用。 */
	bool HasAssignedWeaponDefinition() const
	{
		return !LoadoutDefinition.DefaultWeaponDefinition.IsNull();
	}
};
