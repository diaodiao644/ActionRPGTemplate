// 文件说明：声明英雄装备域的外部命中效果来源生命周期组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "Components/Equipment/HeroEquipmentRuntimeTypes.h"
#include "Components/Equipment/HeroEquipmentTypes.h"
#include "HeroLoadoutEffectComponent.generated.h"

class AActionHeroCharacter;
class UDataAsset_WeaponDefinition;
class UHeroEquipmentComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutRuntimeComponent;

/**
 * 英雄装备外部命中效果来源生命周期组件。
 * 负责 direct 层、具名来源、定时来源、挂起/恢复与聚合镜像的唯一正式状态。
 * 它是外部命中效果生命周期正式宿主，不是武器静态资产入口，也不是单次命中结果快照。
 * 它与 `UHeroLoadoutContextComponent` 已形成稳定强协作面，后续不再继续细拆生命周期宿主。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroLoadoutEffectComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroLoadoutEffectComponent();

public:
	/** 只读查询当前槽位聚合后的 external additional hit effects。它返回的是 effect runtime 聚合结果，不是武器资产默认值。 */
	bool GetLoadoutSlotExternalAdditionalHitEffects(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		TArray<FActionHitEffectEntry>& OutHitEffects) const;

	/** 只读查询当前槽位可继承给 projectile 的聚合外部命中效果。它仍然来自当前 effect runtime，而不是 projectile 自己保存的状态。 */
	bool GetLoadoutSlotProjectileInheritedExternalAdditionalHitEffects(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		TArray<FActionHitEffectEntry>& OutHitEffects) const;

	/** 只读查询当前槽位所有具名来源的调试快照。它只服务排错与展示，不是新的配置入口。 */
	bool GetLoadoutSlotExternalHitEffectSourceDebugSnapshots(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		TArray<FActionExternalHitEffectSourceDebugSnapshot>& OutSnapshots) const;

	/** 高层 direct 写入口：把 request 落到当前槽位正式 effect runtime。 */
	bool ApplyExternalAdditionalHitEffectsRequestToLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FActionExternalAdditionalHitEffectsApplyRequest& InRequest,
		FHeroExternalAdditionalHitEffectsApplyResult& OutResult);

	/** 高层具名来源写入口：把 request 落到 scoped/timed 正式运行态。 */
	bool ApplyExternalHitEffectSourceRequestToLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FActionExternalHitEffectSourceApplyRequest& InRequest,
		FHeroExternalHitEffectSourceApplyResult& OutResult);

	/** 判断当前槽位武器策略是否允许额外命中效果参与聚合。它只读当前策略与 runtime 资格。 */
	bool DoesLoadoutSlotWeaponAllowAdditionalHitEffects(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** direct 层正式写入口。写入后需要统一重建聚合镜像。 */
	bool SetLoadoutSlotExternalAdditionalHitEffects(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const TArray<FActionHitEffectEntry>& InHitEffects);

	/** scoped source 正式写入口。负责具名来源的覆盖、清空和聚合重建。 */
	bool SetScopedLoadoutSlotExternalHitEffectSource(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InSourceTag,
		EActionExternalHitEffectApplyScope InApplyScope,
		const TArray<FActionHitEffectEntry>& InHitEffects);

	/** 向 direct 层追加单条命中效果。 */
	bool AddExternalAdditionalHitEffectToLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FActionHitEffectEntry& InHitEffect);

	/** 清空当前槽位的 direct 层与具名来源聚合结果。它清的是 effect runtime，不是武器静态模板。 */
	void ClearLoadoutSlotExternalAdditionalHitEffects(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** timed source 正式写入口：在 scoped source 之上叠加 duration 生命周期。 */
	bool ApplyTimedScopedExternalHitEffectSourceToLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InSourceTag,
		EActionExternalHitEffectApplyScope InApplyScope,
		const TArray<FActionHitEffectEntry>& InHitEffects,
		float InDuration);

	/** 移除指定具名来源。 */
	bool RemoveLoadoutSlotExternalHitEffectSource(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InSourceTag);

	/** 响应 context 缓存刷新，重建当前槽位 effect 聚合镜像。effect 是正式状态源，context 只接收镜像回写。 */
	void HandleLoadoutSlotWeaponAttributeCacheRefreshed(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData& InOutAttributeCache,
		const UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 响应 context 缓存清空，回零当前槽位聚合镜像。它仍只处理 effect runtime -> context 镜像收尾。 */
	void HandleLoadoutSlotWeaponAttributeCacheCleared(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData& InOutAttributeCache);

	/** 重置单槽位的 hit effect 生命周期运行态与 timer。 */
	void ResetLoadoutSlotHitEffectRuntime(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 重置整个组件的生命周期运行态与 timer。它只回收 effect runtime，不创建新的装备或 context 状态。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 懒取宿主角色缓存。只服务组件内部访问，不表达 effect 生命周期阶段。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 懒取装备域 public 宿主，用于校验槽位注册与读取外部正式状态。 */
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;
	/** 懒取 context 缓存宿主，用于读取和回写 WeaponAttributeCache 镜像。 */
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;
	/** 懒取 runtime 组件，用于确认 definition/runtime assets 是否已就绪。 */
	UHeroLoadoutRuntimeComponent* GetOwningHeroLoadoutRuntimeComponent() const;

	/** 判断指定槽位是否已在装备域正式注册。 */
	bool IsLoadoutSlotRegistered(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 查找指定槽位的 effect runtime。这里持有的是外部命中效果生命周期正式状态，不是单次命中结果。 */
	FHeroLoadoutHitEffectRuntimeState* FindHitEffectRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);
	const FHeroLoadoutHitEffectRuntimeState* FindHitEffectRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 查找或创建指定槽位的 effect runtime。它是 direct / scoped / timed 生命周期的正式局部宿主。 */
	FHeroLoadoutHitEffectRuntimeState& FindOrAddHitEffectRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 只校验 effect entries 本身是否合法，不负责业务资格判定。 */
	bool ValidateExternalHitEffects(
		const TArray<FActionHitEffectEntry>& InHitEffects,
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FString& OutFailureReason) const;

	/** 判断当前槽位 effect runtime 是否因武器策略而处于 suppressed 状态。它返回的是当前生命周期资格结果。 */
	bool IsLoadoutSlotExternalHitEffectSuppressed(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 从当前 runtime 状态重建槽位聚合镜像。聚合结果随后再镜像回 context/attribute cache。 */
	void RebuildLoadoutSlotExternalAdditionalHitEffects(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 在已拿到 WeaponAttributeCache 镜像时重建槽位聚合结果。 */
	void RebuildLoadoutSlotExternalAdditionalHitEffects(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData& InOutAttributeCache);

	/** 把正式 effect runtime 镜像回 WeaponAttributeCache。它只回写派生镜像，不把 context 反向变成状态源。 */
	void RefreshCacheMirrorFromRuntime(
		const FHeroLoadoutHitEffectRuntimeState* InRuntimeState,
		FActionWeaponAttributeCacheData& InOutAttributeCache) const;

	/** 清理单个具名来源的 timer。 */
	void ClearLoadoutSlotExternalHitEffectSourceTimer(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InSourceTag);

	/** 清理指定槽位下所有具名来源的 timer。 */
	void ClearLoadoutSlotExternalHitEffectSourceTimers(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 清理指定槽位下所有具名来源状态。 */
	void ClearLoadoutSlotExternalHitEffectSourceStates(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** timed source 到期回调：统一回到正式 remove 入口。 */
	void HandleTimedExternalHitEffectSourceExpired(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InSourceTag);

protected:
	/** 各固定武器槽的外部命中效果生命周期正式运行态。 */
	UPROPERTY()
	TMap<EHeroWeaponLoadoutSlot, FHeroLoadoutHitEffectRuntimeState> HitEffectRuntimeStatesBySlot;

private:
	friend class UHeroEquipmentComponent;

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroEquipmentComponent> CachedHeroEquipmentComponent;
	mutable TWeakObjectPtr<UHeroLoadoutContextComponent> CachedHeroLoadoutContextComponent;
	mutable TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;
};
