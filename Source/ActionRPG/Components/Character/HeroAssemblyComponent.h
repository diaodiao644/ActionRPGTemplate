// 文件说明：声明英雄角色装配桥组件，负责启动链协调、HUD 初始化桥和武器表现桥。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Components/PawnExtensionComponentBase.h"
#include "HeroAssemblyComponent.generated.h"

class AActionHeroCharacter;
class AHeroWeaponBase;
class UActionHeroLinkedAnimLayer;
class UAnimInstance;
class UDataAsset_WeaponDefinition;
class UHeroLoadoutStateComponent;

/**
 * 英雄角色装配桥组件。
 * 负责角色层的启动链协作、HUD 初始化桥以及武器表现桥，
 * 它是 Hero 启动链协调壳、HUD 初始化桥和武器表现桥宿主，
 * 但不是装备域 startup 正式状态机本体，也不是当前正式装备态宿主。
 * 避免这些跨系统装配细节继续堆在 Character 本体里。
 * 当前粒度已经稳定，后续默认冻结，不再新增新的 Character 桥接拆分。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroAssemblyComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroAssemblyComponent();

	/** 查询 Hero 启动链是否已经完整就绪。它返回的是正式启动状态，不是 UI 自己推测的可交互快照。 */
	bool IsHeroSystemsStartupReady() const { return bHeroSystemsStartupReady; }
	/** 查询 Hero 启动链是否仍在执行中。它适合驱动加载界面或输入门禁，不返回更细的槽位级原因。 */
	bool IsHeroSystemsStartupInProgress() const;
	/** 查询最近一次 Hero 启动链是否已经进入失败态。 */
	bool HasHeroSystemsStartupFailed() const { return bHeroSystemsStartupFailed; }
	/** 读取最近一次启动失败原因，主要服务 UI 和调试展示。 */
	const FString& GetHeroSystemsStartupFailureReason() const { return HeroSystemsStartupFailureReason; }
	/** 读取启动链总体状态枚举。它是正式状态机快照，适合在外围做明确分支判断。 */
	EHeroWeaponLoadoutStartupState GetHeroSystemsStartupState() const;
	/** 读取当前启动链进度比例。它只是只读进度快照，不会推动启动链继续执行。 */
	float GetHeroSystemsStartupProgressRatio() const;
	/** 读取当前仍未完成启动预热的固定武器槽数量。 */
	int32 GetHeroSystemsStartupPendingSlotCount() const;
	/** 读取这次启动链总共需要覆盖的固定武器槽数量。 */
	int32 GetHeroSystemsStartupTotalSlotCount() const;

	/** 启动链开始前统一做输入门禁、状态清理与回调绑定准备。它只协调 Hero 层启动桥，不重建资源链。 */
	void PrepareForHeroSystemsStartup();
	/** 收到装备域启动完成回调后，统一收尾 Hero 级启动链并开放后续桥接。它只消费装备域正式 startup 结果。 */
	void HandleWeaponLoadoutStartupReady();
	/** 收到装备域启动失败回调后，转入 Hero 级失败态并缓存对外可读的失败原因。它不创建第二套 startup 状态机。 */
	void HandleWeaponLoadoutStartupFailed(EHeroWeaponLoadoutSlot InLoadoutSlot, const FString& InFailureReason);
	/** 对外重试 Hero 启动链。它重走正式装配链，不依赖 UI 自己缓存的旧快照，也不把快照反向当成状态源。 */
	bool RetryHeroSystemsStartup();

	/** 初始化玩家 HUD。它只负责 Hero 侧建页桥和首批数据桥，不形成新的 UI 状态源。 */
	void InitPlayerHUD() const;
	/** 销毁或解除当前玩家 HUD 桥接。通常用于 EndPlay、重进关卡或重新 Possess。 */
	void UninitPlayerHUD() const;

	/** 聚合当前 Loadout UI 所需只读快照。它只服务 HUD / MVVM 展示，不反向作为正式运行时状态源。 */
	bool BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const;
	/**
	 * 武器表现桥只负责两类结果：
	 * 1. 非当前槽位武器：Holster + Hidden + NoCollision；
	 * 2. 当前已装备武器：根据 Combat 表现态挂到 HolsterSocket 或 WeaponSocket，并保持可见。
	 * 它不负责重新决定“哪把武器当前正式已装备”，只消费装备域已经落地的结果。
	 */
	bool ApplyWeaponActorPresentation(AHeroWeaponBase* InWeapon, bool bIsEquipped) const;
	/** 只切当前已装备武器的 HolsterSocket / WeaponSocket 挂点，不重新决定“谁是当前装备武器”。 */
	bool ApplyCurrentEquippedWeaponSocketPresentation(AHeroWeaponBase* InWeapon, bool bAttachToWeaponSocket) const;
	/** 按当前角色正式战斗表现态，重新把当前已装备武器落到正确的挂点表现。 */
	bool RefreshCurrentEquippedWeaponSocketPresentation() const;
	/** 按当前 WeaponDefinition 和正式已装备结果，把可见性、碰撞和表现挂点统一刷新到当前武器实例。 */
	bool ApplyCurrentWeaponVisualState(
		AHeroWeaponBase* InWeapon,
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		bool bIsEquipped);
	/** 按当前正式层类执行 link / unlink，不在这里平行维护 linked layer 开关状态源。 */
	void RefreshWeaponAnimLayer(TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass);
	/** 读取当前 mesh 上真正挂着的武器 LinkedLayer 实例。它主要服务诊断与桥接一致性检查，不是新的动画状态源。 */
	UAnimInstance* GetCurrentWeaponLinkedLayerInstance() const;
	/** 校验当前 mesh 是否真的接住了目标 linked layer，供运行时桥接链做一致性检查。 */
	bool ValidateCurrentWeaponLinkedLayerApplied(TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 解析拥有者 Hero 角色。它是本组件的稳定宿主入口，不会在这里创建新的角色级状态。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 解析装备域启动 / UI 状态宿主。这里读到的是正式状态源，不是本组件自己维护的副本。 */
	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;
	/** 把武器实际挂到 WeaponSocket 或 HolsterSocket，并统一处理显隐。它是表现桥内部 helper，不改单独的装备正式状态。 */
	bool ApplyWeaponSocketPresentation(
		AHeroWeaponBase* InWeapon,
		bool bAttachToWeaponSocket,
		bool bShouldHideActor) const;

	/** 绑定装备域启动完成 / 失败回调，形成 Hero 层的启动链桥接，不在这里重建第二套 startup 状态机。 */
	void BindWeaponLoadoutStartupDelegates();
	/** 解绑启动链回调，避免 EndPlay 或重试阶段残留旧绑定。它只做收尾，不改变 startup 正式结果。 */
	void UnbindWeaponLoadoutStartupDelegates();
	/** 启动链期间统一开关 Hero 级输入门禁。它是装配桥的门禁 helper，不替代战斗输入正式状态源。 */
	void SetHeroStartupInputEnabled(bool bEnabled);

protected:
	/** Hero 启动链是否已经正式就绪。它只表达 Hero 层桥接状态，不替代装备域 startup 正式状态机本体。 */
	bool bHeroSystemsStartupReady = false;
	/** Hero 启动链是否已经进入失败态。它只表达 Hero 层桥接结果。 */
	bool bHeroSystemsStartupFailed = false;
	/** 最近一次启动失败原因缓存，主要服务 UI 和调试输出。 */
	FString HeroSystemsStartupFailureReason;

	/** 当前正式应消费的武器 LinkedLayer 类缓存。它只服务表现桥和一致性检查，不替代装备或动画正式状态源。 */
	UPROPERTY(Transient)
	TSubclassOf<UActionHeroLinkedAnimLayer> CurrentWeaponLinkedAnimLayer;

	/** 装备域启动完成回调句柄。它只是桥接绑定句柄，不是新的生命周期状态。 */
	FDelegateHandle EquipmentStartupReadyHandle;
	/** 装备域启动失败回调句柄。它同样只服务桥接收尾。 */
	FDelegateHandle EquipmentStartupFailedHandle;

	/** 拥有者 Hero 的弱引用缓存，避免装配桥在高频查询中重复向上解析。 */
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
};
