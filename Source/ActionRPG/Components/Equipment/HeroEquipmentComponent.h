// 文件说明：声明英雄装备组件相关接口。

#pragma once

#include "CoreMinimal.h"
#include "Components/PawnExtensionComponentBase.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Components/Equipment/HeroEquipmentRuntimeTypes.h"
#include "Components/Equipment/HeroEquipmentTypes.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "HeroEquipmentComponent.generated.h"

class AActionHeroCharacter;
class AHeroWeaponBase;
class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
class UAnimMontage;
class UDataAsset_WeaponDefinition;
class UHeroLoadoutStateComponent;
class UHeroLoadoutContextComponent;
class UHeroLoadoutEffectComponent;
class UHeroLoadoutRuntimeComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeroEquippedWeaponStateChanged, const FHeroEquippedWeaponState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeroWeaponLoadoutEquipFailed, EHeroWeaponLoadoutSlot);

/**
 * 英雄装备组件。
 * 当前版本的职责为：
 * 1. 管理固定武器槽配置、当前装备快照、能力授予/回收与高层装备语义；
 * 2. 作为装备域统一 public/Blueprint 入口，对外收口装备、高层切武能量与当前装备快照；
 * 3. 资源加载、资源预热、实例生命周期与挂起自动装备请求，已经迁入 HeroLoadoutRuntimeComponent；
 * 4. 属性缓存 / 发射物标签、外部命中效果生命周期、启动状态机与 UI 快照桥，继续分别由独立组件承接；
 * 5. `LoadoutRuntimeStatesBySlot` 只是各固定槽的 equipment 局部运行时壳；对外真正该读的是 `EquippedWeaponState` 这份当前正式结果快照；
 * 6. 后续优化重点转为减小 Runtime / Context / State / Effect 协作网密度，不再新增装备域宿主拆分。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroEquipmentComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroEquipmentComponent();

public:
	/** 固定武器槽配置与装备入口。 */

	/** 初始化角色默认固定武器槽配置。它只做固定槽注册、出生默认槽选择和常驻槽位能力初始化。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Equipment")
	void InitializeWeaponLoadout(const TArray<FHeroWeaponLoadoutDefinition>& InWeaponLoadoutDefinitions);

	/**
	 * 为指定固定武器槽设置武器定义。
	 * 这个接口是后续 UI “给某个槽换武器”的核心入口。
	 * 它只改槽位配置并触发加载、预热与实例化，不直接等价于“当前已经切到手上”。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Equipment")
	bool SetWeaponDefinitionForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot, const TSoftObjectPtr<UDataAsset_WeaponDefinition>& InWeaponDefinition);

	/** 按固定武器槽请求装备武器；当前战斗期切武只接受同步 ready 槽位，成功返回 true 即表示本次真实换装已落地。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Equipment")
	bool EquipWeaponByLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot);

public:
	/** 对外查询、同步快照与特殊切武能量接口。当前保留下来的 public/Blueprint API 都属于装备域高层正式公共入口，不再作为后续拆分候选。 */

	/** 判断某个固定武器槽是否已经装了武器。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	bool HasWeaponAssignedToLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 获取当前装备状态的统一同步快照。对外正式只读 EquippedWeaponState 这一份当前生效结果，不要反向拼各槽 runtime。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	FHeroEquippedWeaponState GetCurrentEquippedWeaponState() const;

	/** 读取某个固定武器槽的静态配置定义。它返回的是槽位资产入口，不是当前异步加载或装备结果快照。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	bool GetWeaponLoadoutDefinition(EHeroWeaponLoadoutSlot InLoadoutSlot, FHeroWeaponLoadoutDefinition& OutLoadoutDefinition) const;

	/** 获取当前装备武器 Tag。继续作为高层当前装备聚合语义的一部分保留。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	FGameplayTag GetCurrentWeaponTag() const { return EquippedWeaponState.EquippedWeaponTag; }

	/** 获取当前装备的是哪个固定武器槽。继续作为高层当前装备聚合语义的一部分保留。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	EHeroWeaponLoadoutSlot GetCurrentEquippedLoadoutSlot() const { return EquippedWeaponState.EquippedLoadoutSlot; }

	/** 判断特殊切武独立能量是否已满。继续作为装备总控高层能量入口保留。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	bool IsSpecialWeaponSwitchReady() const;

	/** 获取目标固定武器槽武器的特殊切武动画。它只读取当前配置资格，不推进真实切武事务。 */
	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	UAnimMontage* GetSpecialWeaponSwitchMontageForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 消耗一次特殊切武所需的独立能量。继续作为装备总控高层能量写入口保留。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Equipment")
	bool ConsumeSpecialWeaponSwitchEnergy();

	/** 增加特殊切武能量。继续作为装备域高层能量写入口保留。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Equipment")
	void AddSpecialWeaponSwitchEnergy(float DeltaEnergy);

	/** 获取当前装备快照变化广播。外部应只读消费这份正式当前装备态。 */
	FOnHeroEquippedWeaponStateChanged& OnEquippedWeaponStateChanged() { return EquippedWeaponStateChangedDelegate; }
	/** 获取装备失败广播。它只同步失败槽位，不回滚其它宿主状态。 */
	FOnHeroWeaponLoadoutEquipFailed& OnWeaponLoadoutEquipFailed() { return WeaponLoadoutEquipFailedDelegate; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 基础访问与状态查找辅助。 */

	/** 注册一条固定武器槽配置。它只建立槽位定义与子组件运行时骨架，不代表武器已加载、runtime ready 或已正式装备。 */
	bool RegisterWeaponLoadoutDefinition(const FHeroWeaponLoadoutDefinition& InWeaponLoadoutDefinition);

	/** 获取拥有者英雄角色。它只是正式宿主解析入口，不表达装备域阶段状态。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;

	/** 获取拥有者 ASC。只服务能力授予/回收，不承接装备域正式状态。 */
	UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

	/** 获取拥有者属性集。当前只服务特殊切武能量等高层装备语义读取。 */
	UActionAttributeSetBase* GetOwningAttributeSet() const;

	/** 获取装备域资源链 / 实例链运行时组件。equipment 只向它委托加载、预热与实例解析。 */
	UHeroLoadoutRuntimeComponent* GetOwningHeroLoadoutRuntimeComponent() const;

	/** 获取装备域属性缓存 / 发射物标签上下文组件。equipment 自身不再维护第二份属性缓存或发射物标签状态。 */
	UHeroLoadoutContextComponent* GetOwningHeroLoadoutContextComponent() const;

	/** 查找指定固定武器槽的 equipment 局部运行时壳。它只服务槽位配置与能力镜像，不替代当前装备正式快照。 */
	FHeroWeaponLoadoutRuntimeState* FindRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 查找指定固定武器槽的只读 equipment 局部运行时壳。 */
	const FHeroWeaponLoadoutRuntimeState* FindRuntimeState(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 校验某把武器是否符合某个固定武器槽的武器类别要求。 */
	bool DoesWeaponDefinitionMatchLoadoutSlot(const UDataAsset_WeaponDefinition* InWeaponDefinition, EHeroWeaponLoadoutSlot InLoadoutSlot) const;

	/** 广播当前装备状态变化，统一驱动战斗组件、装配桥和 UI 等外部订阅者刷新状态。 */
	void BroadcastCurrentWeaponStateChanged() const;
	/** 桥接 startup ready 广播到 state 宿主。equipment 不单独维护第二套 startup 状态机。 */
	void BroadcastWeaponLoadoutStartupReady() const;
	/** 桥接 startup failed 广播到 state 宿主。 */
	void BroadcastWeaponLoadoutStartupFailed(EHeroWeaponLoadoutSlot InLoadoutSlot, const FString& InFailureReason) const;
	/** 桥接 UI 脏标记广播到 state 宿主；真正的 UI 快照构建仍由 LoadoutState 负责。 */
	void BroadcastLoadoutUIStateChanged() const;

	/** 真正执行装备逻辑。只有这里会把解析完成的目标武器正式写入 EquippedWeaponState，并切换当前生效武器结果。 */
	bool EquipResolvedLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot, UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 向角色请求切换武器实例的显示、碰撞与挂点状态。它只桥接表现，不承担切槽事务。 */
	bool SetWeaponEquippedState(AHeroWeaponBase* InWeapon, bool bIsEquipped) const;

	/** 能力、动画层与外部同步刷新链路。 */

	/** 将当前已装备武器切换为未装备表现。它只收当前装备的表现与能力，不自己决定下一个槽位。 */
	void DeactivateCurrentEquippedWeapon();

	/** 将目标武器切换为已装备表现。它只激活当前快照对应的武器实例与附加能力。 */
	bool ActivateCurrentEquippedWeapon();

	/** 在初始化阶段授予指定固定武器槽的专属战斗能力。固定槽能力当前按常驻模型处理。 */
	void GrantLoadoutAbilities(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 在重置负载或组件销毁时回收指定固定武器槽的专属战斗能力。它只清常驻槽位能力，不处理当前武器定义的额外能力。 */
	void RemoveLoadoutAbilities(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/** 在当前武器真正装备到手时，授予该武器定义额外带来的能力。它跟随当前已装备武器，而不是槽位永久常驻。 */
	void GrantWeaponDefinitionAbilities(EHeroWeaponLoadoutSlot InLoadoutSlot, const UDataAsset_WeaponDefinition* InWeaponDefinition);

	/** 在切槽、卸载或销毁组件时回收指定固定武器槽当前武器定义带来的能力。 */
	void RemoveWeaponDefinitionAbilities(EHeroWeaponLoadoutSlot InLoadoutSlot);

	/**
	 * 将资产层配置的通用输入标签改写成“按固定武器槽区分”的输入标签。
	 * 这样既允许资产层继续复用通用输入名，又能保证常驻槽位 GA 不会争抢同一个输入。
	 */
	FGameplayTag ResolveGrantedAbilityInputTagForLoadoutSlot(EHeroWeaponLoadoutSlot InLoadoutSlot, const FGameplayTag& InInputTag) const;

	/** 广播正式装备失败结果。它只同步结果，不承担状态回滚。 */
	void BroadcastWeaponLoadoutEquipFailed(EHeroWeaponLoadoutSlot InLoadoutSlot) const;

protected:
	/** 当前角色所有固定武器槽的 equipment 局部运行时壳。它服务固定槽定义、能力镜像与局部缓存，不直接对外充当正式当前装备态。 */
	UPROPERTY()
	TMap<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutRuntimeState> LoadoutRuntimeStatesBySlot;

	// 当前已装备快照：
	// 对外部战斗链、动画链和调试层来说，真正应该读取的是这一份“当前生效状态”，
	// 而不是自己去拼接各个槽位运行时数据。
	/** 当前真正装备中的武器实例、槽位与武器 Tag 运行时状态。它才是对外正式应读的当前装备结果快照。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Equipment")
	FHeroEquippedWeaponState EquippedWeaponState;

	/** 避免重复初始化默认固定武器槽。固定槽注册与常驻能力授予当前只允许跑一次。 */
	bool bWeaponLoadoutInitialized = false;

private:
	friend class UHeroLoadoutStateComponent;
	friend class UHeroLoadoutEffectComponent;
	friend class UHeroLoadoutRuntimeComponent;

	// 外部依赖缓存与广播：
	// 缓存拥有者角色，统一对外广播装备状态变化、装备失败和启动状态变化事件。
	/** 缓存拥有者英雄角色，避免每次重复 Cast。 */
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedOwnerHeroCharacter;

	/** 当前装备状态变化委托。 */
	FOnHeroEquippedWeaponStateChanged EquippedWeaponStateChangedDelegate;
	/** 当前装备失败结果委托。 */
	FOnHeroWeaponLoadoutEquipFailed WeaponLoadoutEquipFailedDelegate;
};
