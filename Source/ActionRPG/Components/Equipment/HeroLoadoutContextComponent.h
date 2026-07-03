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

	/** 重置整个 context 组件的运行态与组件缓存。 */
	void ResetRuntimeStateForHeroStartup();

	/** 拷贝读取指定槽位的 WeaponAttributeCache 镜像。 */
	bool GetWeaponAttributeCacheByLoadoutSlot(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData& OutAttributeCache) const;

	/** 判断指定槽位当前是否持有有效 WeaponAttributeCache。 */
	bool HasLoadoutSlotWeaponAttributeCache(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 只读获取指定槽位的 WeaponAttributeCache 指针。 */
	bool TryGetLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FActionWeaponAttributeCacheData*& OutAttributeCache) const;

	/** 可写获取指定槽位的 WeaponAttributeCache 指针。 */
	bool TryGetMutableLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FActionWeaponAttributeCacheData*& OutAttributeCache);

	/** 用当前武器定义刷新指定槽位的属性缓存镜像。 */
	void RefreshLoadoutSlotWeaponAttributeCache(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		const UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 清空指定槽位的属性缓存镜像。 */
	void ClearLoadoutSlotWeaponAttributeCache(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 读取指定槽位当前选择的发射物配置标签。 */
	bool GetLoadoutSlotSelectedProjectileConfigTag(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag& OutProjectileConfigTag) const;

	/** 设置指定槽位当前选择的发射物配置标签。它只维护上下文运行态，不决定武器是否支持切换。 */
	bool SetLoadoutSlotSelectedProjectileConfigTag(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InProjectileConfigTag);

	/** 清空指定槽位当前选择的发射物配置标签。 */
	void ClearLoadoutSlotSelectedProjectileConfigTag(EHeroWeaponLoadoutSlot InLoadoutSlot);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UHeroLoadoutEffectComponent* GetOwningHeroLoadoutEffectComponent() const;
	UHeroLoadoutRuntimeComponent* GetOwningHeroLoadoutRuntimeComponent() const;
	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;

	/** 查找指定槽位的 context runtime。它只服务上下文存取，不表达正式业务阶段。 */
	FHeroLoadoutContextRuntimeState* FindContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);
	const FHeroLoadoutContextRuntimeState* FindContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 查找或创建指定槽位的 context runtime。 */
	FHeroLoadoutContextRuntimeState& FindOrAddContextRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 桥接通知 LoadoutState 刷新 UI 快照，不在 context 组件内维护独立 UI 状态。 */
	void BroadcastLoadoutUIStateChanged() const;

protected:
	UPROPERTY()
	TMap<EHeroWeaponLoadoutSlot, FHeroLoadoutContextRuntimeState> ContextRuntimeStatesBySlot;

private:
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroLoadoutEffectComponent> CachedHeroLoadoutEffectComponent;
	mutable TWeakObjectPtr<UHeroLoadoutRuntimeComponent> CachedHeroLoadoutRuntimeComponent;
	mutable TWeakObjectPtr<UHeroLoadoutStateComponent> CachedHeroLoadoutStateComponent;
};
