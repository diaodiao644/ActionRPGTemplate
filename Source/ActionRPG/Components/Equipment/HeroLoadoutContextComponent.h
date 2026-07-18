// 文件说明：声明英雄装备域属性缓存与发射物选中标签上下文组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "Components/Equipment/HeroEquipmentRuntimeTypes.h"
#include "HeroLoadoutContextComponent.generated.h"

class AActionHeroCharacter;
class UDataAsset_WeaponDefinition;
class UHeroLoadoutEffectComponent;
class UHeroLoadoutRuntimeComponent;
class UHeroLoadoutStateComponent;

/**
 * 英雄装备域属性缓存 / 发射物选中标签上下文组件。
 * 负责固定武器槽属性缓存镜像与选中发射物标签运行态的唯一正式状态。
 * 它是上下文 runtime 宿主，不是武器静态资产入口，也不是 startup / UI 快照宿主。
 * 它与 `UHeroLoadoutEffectComponent` 已形成稳定强协作面，后续不再继续拆成更细宿主。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroLoadoutContextComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroLoadoutContextComponent();

public:
	/** 为指定固定武器槽建立 context runtime 骨架。它只建槽位上下文，不刷新属性缓存或发射物标签。 */
	void EnsureLoadoutSlotContextState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 重置指定槽位的上下文运行态。它只清当前槽位 context，不替代 runtime/equipment 收尾。 */
	void ResetLoadoutSlotContextRuntime(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 重置整个 context 组件的运行态与组件缓存。它只回收属性缓存镜像与发射物标签状态，不替代 runtime/state/equipment 的启动收尾。 */
	void ResetRuntimeStateForHeroStartup();

	/** 拷贝读取指定槽位的 WeaponAttributeCache 镜像。它是派生上下文缓存，不是新的武器资产入口。 */
	bool GetWeaponAttributeCacheByLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData& OutAttributeCache) const;

	/** 判断指定槽位当前是否持有有效 WeaponAttributeCache 镜像。这里判断的是派生上下文是否就绪，不是资产是否存在。 */
	bool HasLoadoutSlotWeaponAttributeCache(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 只读获取指定槽位的 WeaponAttributeCache 指针。调用方应把它当成运行时快照，不要反向当成新的正式状态源。 */
	bool TryGetLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FActionWeaponAttributeCacheData*& OutAttributeCache) const;

	/** 可写获取指定槽位的 WeaponAttributeCache 指针。当前仅供装备域内部宿主协作回写镜像使用。 */
	bool TryGetMutableLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData*& OutAttributeCache);

	/** 用当前武器定义刷新指定槽位的属性缓存镜像。它根据正式资产与当前运行时结果重建派生上下文，不改资产本身。 */
	void RefreshLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 清空指定槽位的属性缓存镜像。它只回收派生上下文，不改 WeaponDefinition 或装备事务状态。 */
	void ClearLoadoutSlotWeaponAttributeCache(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 读取指定槽位当前选择的发射物配置标签。它属于 context 正式运行态，而不是 UI 本地临时选择。 */
	bool GetLoadoutSlotSelectedProjectileConfigTag(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag& OutProjectileConfigTag) const;

	/** 设置指定槽位当前选择的发射物配置标签。它只维护上下文运行态，不决定武器是否支持切换。 */
	bool SetLoadoutSlotSelectedProjectileConfigTag(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InProjectileConfigTag);

	/** 清空指定槽位当前选择的发射物配置标签。它清的是 context 正式运行态，不是 UI 临时选中态。 */
	void ClearLoadoutSlotSelectedProjectileConfigTag(EHeroWeaponLoadoutSlot InLoadoutSlot);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 获取拥有者英雄角色。它只是 context 正式宿主解析入口。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 获取外部命中效果生命周期宿主。context 通过它把 effect 聚合镜像回属性缓存。 */
	UHeroLoadoutEffectComponent* GetOwningHeroLoadoutEffectComponent() const;
	/** 获取资源链 / 实例链宿主，用于按当前定义和实例就绪情况刷新 context。 */
	UHeroLoadoutRuntimeComponent* GetOwningHeroLoadoutRuntimeComponent() const;
	/** 获取 startup / UI 快照桥宿主，用于转发 UI 脏标记。 */
	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;

	/** 查找指定槽位的 context runtime。它只服务上下文存取，不表达当前装备事务、实例生命周期或 startup 阶段。 */
	FHeroLoadoutContextRuntimeState* FindContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);
	const FHeroLoadoutContextRuntimeState* FindContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 查找或创建指定槽位的 context runtime。 */
	FHeroLoadoutContextRuntimeState& FindOrAddContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 桥接通知 LoadoutState 刷新 UI 快照，不在 context 组件内维护独立 UI 状态。 */
	void BroadcastLoadoutUIStateChanged() const;

protected:
	/** 各固定武器槽的 context 正式运行态。这里持有的是属性缓存镜像与发射物选中标签，不是 startup/UI 快照，也不是装备事务壳。 */
	UPROPERTY()
	TMap<EHeroWeaponLoadoutSlot, FHeroLoadoutContextRuntimeState> ContextRuntimeStatesBySlot;

private:
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroLoadoutEffectComponent> CachedHeroLoadoutEffectComponent;
	mutable TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;
	mutable TWeakObjectPtr<UHeroLoadoutStateComponent> CachedHeroLoadoutStateComponent;
};
