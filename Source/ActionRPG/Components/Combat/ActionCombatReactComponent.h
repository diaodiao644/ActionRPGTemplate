#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "ActionGameplayTags.h"
#include "Components/PawnExtensionComponentBase.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionReactionTypes.h"
#include "ActionCombatReactComponent.generated.h"

class AActionCharacterBase;
class UAnimInstance;
class UAnimMontage;
class UCharacterMovementComponent;
class UPawnCombatComponent;

/**
 * 受击反应组件。
 * 统一维护角色在受击、破韧、击飞、击倒等链路里的运行态，
 * 并负责把受击表现、恢复开窗和主链收尾收口到同一处。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UActionCombatReactComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UActionCombatReactComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

public:
	/** 处理一条正式受击事件，并切换到对应的受击主链。 */
	bool HandleCombatReactEvent(FGameplayTag EventTag, const FActionDamagePayload& DamagePayload);

	/** 返回指定受击事件的固定优先级。它只解释 CombatReact 配置层的覆盖顺序，不触发受击状态切换。 */
	int32 ResolveCombatReactPriority(FGameplayTag EventTag) const;

	/** 判断当前受击链是否允许被新的受击事件覆盖。它只做裁决查询，不改当前受击运行态。 */
	bool CanIncomingCombatReactOverrideCurrent(FGameplayTag EventTag) const;
	/** 判断“同优先级重复触发”是否可按正式规则覆盖当前受击链。 */
	bool IsSamePriorityRepeatableCombatReact(FGameplayTag EventTag) const;
	/** 判断某类受击事件是否属于允许重复触发的正式受击事件。 */
	bool IsRepeatableCombatReactEvent(FGameplayTag EventTag) const;

	/** 输出当前受击链对新受击事件的覆盖裁决摘要。 */
	FString DescribeIncomingCombatReactOverrideDecision(FGameplayTag EventTag) const;

	/** 恢复阶段是否允许当前输入通过取消窗口切回主动链。 */
	bool CanActivateAbilityDuringRecoveryCancelWindow(const FGameplayTag& InputTag) const;

	/** 非处决受击到达正式恢复帧后，开放输入白名单并恢复移动响应。它是普通受击唯一正式开窗点。 */
	void NotifyCombatReactUnlockFrame();

	/** 当前是否仍处于 CombatReact 正式运行态。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前是否仍处于 CombatReact 正式运行态。它只回答普通受击主链是否激活，不和 Hero 主动 Ability 激活态混用。"))
	bool IsCombatReactActive() const { return bCombatReactActive; }

	/** 将当前 PoiseBreak 受击运行态正式交接给目标侧处决 victim 演出。 */
	bool HandOffActiveCombatReactToExecutionVictim(FString* OutFailureReason);

	/** 当前这次受击是否已正式移交给目标侧处决 victim 演出。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前这次受击是否已正式移交给目标侧处决 victim 演出。移交后，普通 CombatReact 收尾不应再继续改写 victim 运行态。"))
	bool WasHandedOffToExecutionVictim() const { return bHandedOffToExecutionVictim; }

	/** 输出当前受击运行态摘要。只服务 UI/日志/排错，不推进正式状态。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "输出当前受击运行态摘要。它只服务 UI、日志和排错，不推进正式受击状态。"))
	FString DescribeCurrentCombatReactState() const;

	/** 屏幕或日志打印当前受击运行态。只服务调试。 */
	UFUNCTION(BlueprintCallable, Category = "Action|CombatReact", meta = (ToolTip = "屏幕或日志打印当前受击运行态。它只服务调试，不重放受击或推进恢复链。"))
	void PrintCurrentCombatReactStateDebug() const;

	/** 当前受击方向查询。它返回的是当前受击运行态结果，不是配置默认方向。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前正式受击方向。它读取的是运行态结果，不是资产默认方向。"))
	EActionCombatReactDirection GetCurrentReactDirection() const { return CurrentReactDirection; }

	/** 当前受击阶段查询。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前正式受击阶段。它只描述普通受击链自己的阶段，不等于角色整体动作状态机。"))
	EActionCombatReactPhase GetCurrentReactPhase() const { return CurrentReactPhase; }

	/** 当前受击事件标签查询。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前正式受击事件标签。它只描述这次普通受击类型，不是新的 GameplayTag 状态源。"))
	FGameplayTag GetCurrentReactEventTag() const { return CurrentReactEventTag; }

	/** 当前受击蒙太奇查询。它只反映正在运行的正式受击表现。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前正式受击蒙太奇。它只反映正在运行的受击表现，不反向决定受击规则。"))
	UAnimMontage* GetCurrentReactMontage() const { return CurrentCombatReactMontage; }

	/** 当前受击附带的状态效果标签查询。 */
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact", meta = (ToolTip = "当前受击附带的状态效果标签。它只服务状态效果和查询对齐，不替代 CombatReact 本体运行态。"))
	FGameplayTag GetCurrentReactStatusEffectTag() const { return CurrentReactStatusEffectTag; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsHitStunActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_HitReact; }
	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsHeavyHitReactActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_HeavyHitReact; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsPrimaryReactPhaseActive() const
	{
		return CurrentReactPhase == EActionCombatReactPhase::Reacting
			|| CurrentReactPhase == EActionCombatReactPhase::AirborneUncontrolled;
	}

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsRecoveryPhaseActive() const { return CurrentReactPhase == EActionCombatReactPhase::Recovery; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsGuardBreakActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_GuardBreak; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsPoiseBreakActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_PoiseBreak; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsLaunchActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_Launch; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsKnockdownActive() const { return CurrentReactEventTag == ActionGameplayTags::Combat_Event_Knockdown; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsAirborneUncontrolledActive() const
	{
		return CurrentReactPhase == EActionCombatReactPhase::AirborneUncontrolled;
	}

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool HasCombatReactUnlockFrameReached() const { return bCombatReactUnlockFrameReached; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsWaitingForLanding() const { return bWaitingForLanding; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsCombatReactMovementRestricted() const { return bCombatReactMovementRestricted; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsHoldingReactUntilTimerExpires() const { return bHoldReactUntilTimerExpires; }

#if WITH_EDITOR
	/** 只给编辑器离线审计读取当前正式 CombatReact 静态配置，不对运行时暴露第二套状态入口。 */
	const FActionCombatReactConfig& GetCombatReactConfigForEditorAudit() const { return CombatReactConfig; }
#endif

	/** 蓝图受击开始回调。只消费正式运行态变化，不应在蓝图里重造第二套受击状态机。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action|CombatReact")
	void K2_OnCombatReactStarted(FGameplayTag EventTag, EActionCombatReactDirection ReactDirection, AActor* InstigatorActor);

	/** 蓝图受击结束回调。只作为表现/提示出口，不推进正式收尾链。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action|CombatReact")
	void K2_OnCombatReactFinished(FGameplayTag EventTag, bool bWasInterrupted);

protected:
	/** 按事件标签与命中信息解析这次正式受击应播放的蒙太奇与方向。 */
	UAnimMontage* ResolveCombatReactMontage(
		FGameplayTag EventTag,
		const FActionDamagePayload& DamagePayload,
		EActionCombatReactDirection& OutReactDirection) const;

	/** 把受击事件映射到角色正式受击状态标签。 */
	FGameplayTag ResolveCombatReactStateTag(FGameplayTag EventTag) const;

	/** 把受击事件映射到需要附加的状态效果标签。 */
	FGameplayTag ResolveCombatReactStatusEffectTag(FGameplayTag EventTag) const;

	/** 依据受击来源方向解析正式受击朝向。 */
	EActionCombatReactDirection ResolveCombatReactDirection(const FActionDamagePayload& DamagePayload) const;

	/** 在进入受击主链前先收角色当前战斗状态，避免主动链残留。 */
	void PrepareOwnerCombatStateForReact() const;

	/** 正式写入当前受击运行态并切入对应阶段。 */
	void ApplyCombatReactState(const FGameplayTag& StateTag, const FActionDamagePayload& DamagePayload);

	/** 给当前受击应用状态效果壳。它是表现/资格附着层，不替代 CombatReact 正式运行态。 */
	void ApplyCombatReactStateEffect(const FGameplayTag& StateTag, const FGameplayTag& StatusEffectTag, float Duration);

	/** 收掉当前受击附加的状态效果壳。 */
	void RemoveCombatReactStateEffect();

	/** 应用受击期间的移动限制。 */
	void ApplyCombatReactMovementRestriction();

	/** 按命中结果需要时施加击退。 */
	void ApplyKnockbackIfNeeded(const FActionDamagePayload& DamagePayload) const;

	/** 清掉受击期间的移动限制。 */
	void ClearCombatReactMovementRestriction();

	/** 判断当前受击是否应在落地后再结束，而不是按计时器立即收尾。 */
	bool ShouldWaitForLandingToFinishReact() const;

	/** 落地后的统一收尾推进点。 */
	void HandleCombatReactLanded();

	/** 正式结束当前受击运行态。所有普通收尾都统一回到这里。 */
	void FinishCombatReact(bool bWasInterrupted, bool bBroadcastEvent);

	/** 停止当前正式受击蒙太奇。 */
	void StopCurrentCombatReactMontage();

	/** 读取拥有者动画实例。 */
	UAnimInstance* GetOwnerAnimInstance() const;

	/** 读取拥有者角色。 */
	AActionCharacterBase* GetOwningCharacter() const;

	/** 读取拥有者最小战斗公共状态宿主。 */
	UPawnCombatComponent* GetOwningCombatComponent() const;

	/** 读取拥有者 ASC，用于状态效果与标签读写。 */
	class UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

	/** 读取拥有者移动组件。 */
	UCharacterMovementComponent* GetOwningMovementComponent() const;

	/** 确保当前受击链依赖的蒙太奇资源已加载。 */
	bool EnsureCombatReactAssetsLoaded();

	/** 按事件标签读取对应的受击方向蒙太奇集。 */
	const FActionCombatReactMontageSet* GetCombatReactMontageSetForEvent(FGameplayTag EventTag) const;

	/** 查询拥有者当前是否持有某个 GameplayTag。 */
	bool HasOwningGameplayTag(const FGameplayTag& TagToCheck) const;

	/** 解析当前受击持续时长。 */
	float ResolveCombatReactDuration(UAnimMontage* ReactMontage, const FActionDamagePayload& DamagePayload) const;

	/** 解析当前受击附加状态效果持续时长。 */
	float ResolveCombatReactEffectDuration(const FActionDamagePayload& DamagePayload) const;

	/** 查询受击蒙太奇是否带有正式恢复开窗 Notify。 */
	bool HasCombatReactUnlockNotify(const UAnimMontage* ReactMontage) const;

	/** 解析受击恢复开窗 Notify 的触发时机。 */
	float ResolveCombatReactUnlockNotifyTriggerTime(const UAnimMontage* ReactMontage) const;

	/** 解析为保证开窗存在而需要的最小时长。 */
	float ResolveMinimumCombatReactDurationForUnlockFrame(UAnimMontage* ReactMontage, float BaseReactDuration) const;

	/** 打开受击恢复取消窗口。 */
	void OpenCombatReactRecoveryCancelWindow();

	/** 关闭受击恢复取消窗口。 */
	void CloseCombatReactRecoveryCancelWindow();

	/** 只供处决 victim 演出接管时复位当前受击运行态，不走普通受击收尾语义。 */
	void ResetActiveCombatReactRuntimeForExecutionVictimHandoff();

	/** 受击计时器到期后的统一收尾入口：在处决锁、破韧解锁帧和落地等待之间做最终裁决。 */
	UFUNCTION()
	void OnCombatReactExpired();

protected:
	/** 正式受击配置入口。决定各事件的蒙太奇、恢复白名单和时长相关规则。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|CombatReact", meta = (ToolTip = "正式受击配置入口。它决定普通受击事件的蒙太奇、方向回退、恢复开窗白名单和相关时长规则。"))
	FActionCombatReactConfig CombatReactConfig;

private:
	/** 当前正式受击蒙太奇结束时的收尾入口。这里只收当前运行中的那一段，不处理旧蒙太奇晚到回调。 */
	UFUNCTION()
	void HandleCombatReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
	/** 当前是否仍处于 CombatReact 正式运行态。 */
	bool bCombatReactActive = false;

	/** 当前正式受击蒙太奇。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentCombatReactMontage = nullptr;

	/** 当前正式受击方向。 */
	EActionCombatReactDirection CurrentReactDirection = EActionCombatReactDirection::None;

	/** 当前正式受击阶段。 */
	EActionCombatReactPhase CurrentReactPhase = EActionCombatReactPhase::None;

	/** 当前正式受击事件标签。 */
	FGameplayTag CurrentReactEventTag;

	/** 当前正式受击状态标签。 */
	FGameplayTag CurrentReactStateTag;

	/** 当前正式受击附带的状态效果标签。 */
	FGameplayTag CurrentReactStatusEffectTag;

	/** 当前正式受击附加的状态效果句柄。 */
	FActiveGameplayEffectHandle CurrentCombatReactEffectHandle;

	/** 当前受击总时长计时器。 */
	FTimerHandle CombatReactTimerHandle;

	/** 当前是否正在等待角色落地后再收尾。 */
	bool bWaitingForLanding = false;

	/** 当前是否已施加受击移动限制。 */
	bool bCombatReactMovementRestricted = false;

	/** 进入受击前缓存的移动速度。 */
	float CachedMaxWalkSpeed = 0.f;

	/** 进入受击前缓存的移动加速度。 */
	float CachedMaxAcceleration = 0.f;

	/** 落地后需要继续补走的恢复时长。 */
	float PendingLandingRecoveryDuration = 0.f;

	/** 当前是否要求至少持有到计时器到期。 */
	bool bHoldReactUntilTimerExpires = false;

	/** 当前正式恢复开窗帧是否已到达。 */
	bool bCombatReactUnlockFrameReached = false;

	/** 当前受击是否已正式移交给目标侧处决 victim 演出。 */
	bool bHandedOffToExecutionVictim = false;

	/** 预加载到内存的受击蒙太奇集合。只服务资源复用，不是正式受击状态。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimMontage>> LoadedCombatReactMontages;

	/** 当前受击资源是否已完成本地预载。 */
	bool bCombatReactAssetsLoaded = false;
};
