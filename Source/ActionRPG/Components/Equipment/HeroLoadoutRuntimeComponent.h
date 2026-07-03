// 文件说明：声明英雄装备域资源链 / 实例链运行时组件。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "Components/Equipment/HeroEquipmentRuntimeTypes.h"
#include "HeroLoadoutRuntimeComponent.generated.h"

class AActionHeroCharacter;
class AHeroWeaponBase;
class UDataAsset_WeaponDefinition;
class UHeroEquipmentComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutStateComponent;
struct FStreamableHandle;

USTRUCT()
struct FHeroLoadoutDefinitionFailureLogState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGameplayTag WeaponTag;

	UPROPERTY()
	int32 LoadRevision = INDEX_NONE;

	UPROPERTY()
	FString FailureReason;

	bool Matches(const int32 InLoadRevision, const FGameplayTag& InWeaponTag, const FString& InFailureReason) const
	{
		return LoadRevision == InLoadRevision
			&& WeaponTag == InWeaponTag
			&& FailureReason == InFailureReason;
	}
};

/**
 * 英雄装备域资源链 / 实例链运行时组件。
 * 负责固定武器槽定义加载、资源预热、实例生命周期、挂起自动装备请求与版本校验。
 * 后续若继续优化，默认只压缩完成回调协作分支，不再继续细拆宿主。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroLoadoutRuntimeComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroLoadoutRuntimeComponent();

public:
	/** 为指定固定武器槽建立资源链运行时骨架。它只建槽位状态，不触发定义加载或资源预热。 */
	void EnsureLoadoutSlotRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);
	bool IsLoadoutSlotRuntimeRegistered(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 读取指定槽位当前的异步加载版本号。 */
	int32 GetLoadoutSlotLoadRevision(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 推进指定槽位的异步加载版本号，用于让旧回调整体失效。 */
	int32 AdvanceLoadoutSlotLoadRevision(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 读取指定槽位当前已通过校验的定义缓存。它是 runtime 链镜像，不等于正式已装备状态。 */
	UDataAsset_WeaponDefinition* GetLoadedWeaponDefinitionByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 读取指定槽位当前缓存的武器实例。实例存在只代表实例链已准备，不等于已正式装备到手上。 */
	AHeroWeaponBase* GetSpawnedWeaponInstanceByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 判断指定槽位当前是否已有缓存实例。 */
	bool HasSpawnedWeaponInstanceByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 判断指定槽位当前是否同时满足“资源已就绪 + 必要实例已存在”。 */
	bool IsLoadoutSlotRuntimeReady(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 描述一份武器定义当前缺失哪些 runtime assets，只服务诊断和 UI 提示。 */
	FString DescribeWeaponRuntimeAssetReadiness(const UDataAsset_WeaponDefinition* InWeaponDefinition) const;

	/** 记录“资源就绪后是否自动补装备”的挂起请求。它只保存意图，不直接写当前装备态。 */
	void SetPendingEquipWhenReadyRequest(EHeroWeaponLoadoutSlot InLoadoutSlot, int32 InExpectedLoadRevision);

	/** 判断当前槽位是否仍命中同一笔挂起自动补装备请求。 */
	bool MatchesPendingEquipWhenReadyRequest(EHeroWeaponLoadoutSlot InLoadoutSlot, int32 InExpectedLoadRevision) const;

	/** 清空当前挂起的自动补装备请求。 */
	void ClearPendingEquipWhenReadyRequest();

	/** 请求异步加载固定武器槽定义。它只推进 definition 资源链，不直接写 EquippedWeaponState。 */
	void RequestLoadoutSlotDefinitionAsync(EHeroWeaponLoadoutSlot InLoadoutSlot, bool bEquipWhenReady, int32 InExpectedLoadRevision);

	/** 请求异步预热指定武器定义的 runtime assets。它只推进资源预热，不直接执行正式装备写回。 */
	void RequestLoadoutSlotRuntimeAssetsAsync(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		UDataAsset_WeaponDefinition* InWeaponDefinition,
		bool bEquipWhenReady,
		int32 InExpectedLoadRevision);

	/** 释放指定槽位的资源链运行时。它只收 handle / 缓存定义 / 实例，不直接裁决当前装备态。 */
	void ReleaseLoadoutSlotRuntimeResources(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		bool bClearLoadedWeaponDefinition,
		bool bDestroySpawnedInstance);

	/** 取消指定槽位当前异步链并推进 revision，让旧回调整体失效。 */
	void CancelLoadoutSlotAsyncRequestsAndAdvanceRevision(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 确保指定槽位拥有与已加载定义匹配的运行时武器实例。它只维护实例存在性，不承担正式切槽语义。 */
	bool EnsureLoadoutSlotWeaponInstance(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 销毁指定槽位当前缓存的武器实例。 */
	void DestroyLoadoutSlotWeaponInstance(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 解析正式装备时应使用的目标实例。runtime 组件只保证实例可解析，正式写回仍交给 equipment 宿主。 */
	bool ResolveEquipTargetWeaponInstance(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		UDataAsset_WeaponDefinition* InWeaponDefinition,
		AHeroWeaponBase*& OutWeaponInstance);

	/** 组件退场时统一回收资源链、实例链和挂起请求。 */
	void ResetRuntimeStateForEndPlay();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 缓存读取当前角色宿主。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;

	/** 缓存读取正式装备态宿主。 */
	UHeroEquipmentComponent* GetOwningHeroEquipmentComponent() const;

	/** 缓存读取定义派生上下文宿主。 */
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;

	/** 缓存读取 startup prewarm 状态宿主。 */
	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;

	/** 按槽位读取当前 runtime 壳。 */
	FHeroLoadoutRuntimeAssetState* FindRuntimeAssetState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 按槽位只读读取当前 runtime 壳。 */
	const FHeroLoadoutRuntimeAssetState* FindRuntimeAssetState(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 按 revision 查找槽位运行时，只服务“旧异步回调是否仍有效”的校验。 */
	FHeroLoadoutRuntimeAssetState* FindRuntimeAssetStateForRevision(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		int32 InExpectedLoadRevision);

	/** 在 definition 软引用加载完成后，解析并校验这份定义是否可正式进入当前槽位运行时。 */
	UDataAsset_WeaponDefinition* ResolveLoadoutSlotWeaponDefinitionAfterLoad(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 按“槽位 + revision + 失败原因”收口非法定义日志，避免同一配置错误重复刷屏。 */
	void LogInvalidLoadoutDefinitionOnce(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		int32 InLoadRevision,
		const FGameplayTag& InWeaponTag,
		const FString& InFailureReason,
		const FString& InWeaponSubtypeTagText,
		const FString& InWeaponCategoryText,
		const FString& InWeaponActorClassPath);

	/** 清理指定槽位的非法定义日志去重缓存。 */
	void ClearLoadoutDefinitionFailureLogState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** definition 加载完成回调：负责解析定义、校验 revision，并决定是否继续推进 runtime assets 预热。 */
	void HandleLoadoutSlotDefinitionLoaded(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		bool bEquipWhenReady,
		int32 InExpectedLoadRevision);

	/** runtime assets 预热完成回调：负责实例创建、自动补装备和 startup prewarm 收尾。 */
	void HandleLoadoutSlotRuntimeAssetsLoaded(
		EHeroWeaponLoadoutSlot InLoadoutSlot,
		FGameplayTag InWeaponTag,
		bool bEquipWhenReady,
		int32 InExpectedLoadRevision);

	/** 释放一条异步加载句柄。 */
	void ReleaseStreamableHandle(TSharedPtr<FStreamableHandle>& InOutHandle) const;

	/** 从已解析定义同步生成一把待命武器实例。它只生成 holstered/hidden 初始表现。 */
	AHeroWeaponBase* SpawnWeaponFromDefinition(UDataAsset_WeaponDefinition* InWeaponDefinition) const;

protected:
	UPROPERTY()
	TMap<EHeroWeaponLoadoutSlot, FHeroLoadoutRuntimeAssetState> LoadoutRuntimeAssetsBySlot;

private:
	friend class UHeroEquipmentComponent;

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroEquipmentComponent> CachedHeroEquipmentComponent;
	mutable TWeakObjectPtr<UHeroLoadoutContextComponent> CachedHeroLoadoutContextComponent;
	mutable TWeakObjectPtr<UHeroLoadoutStateComponent> CachedHeroLoadoutStateComponent;

	/** 当前挂起的“资源就绪后自动补装备”目标槽位。它只是挂起意图壳，不等于正式已装备。 */
	EHeroWeaponLoadoutSlot PendingEquipWhenReadyLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 当前挂起自动补装备请求对应的 revision，用于保证只命中同一笔异步资源链。 */
	int32 PendingEquipWhenReadyLoadRevision = INDEX_NONE;

	/** 非法定义日志的按槽位去重缓存。它只服务表达层，不参与正式运行时状态。 */
	UPROPERTY(Transient)
	TMap<EHeroWeaponLoadoutSlot, FHeroLoadoutDefinitionFailureLogState> DefinitionFailureLogStatesBySlot;
};
