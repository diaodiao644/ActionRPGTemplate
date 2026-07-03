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

	/** 返回指定受击事件的固定优先级。 */
	int32 ResolveCombatReactPriority(FGameplayTag EventTag) const;

	/** 判断当前受击链是否允许被新的受击事件覆盖。 */
	bool CanIncomingCombatReactOverrideCurrent(FGameplayTag EventTag) const;
	bool IsSamePriorityRepeatableCombatReact(FGameplayTag EventTag) const;
	bool IsRepeatableCombatReactEvent(FGameplayTag EventTag) const;

	/** 输出当前受击链对新受击事件的覆盖裁决摘要。 */
	FString DescribeIncomingCombatReactOverrideDecision(FGameplayTag EventTag) const;

	/** 恢复阶段是否允许当前输入通过取消窗口切回主动链。 */
	bool CanActivateAbilityDuringRecoveryCancelWindow(const FGameplayTag& InputTag) const;

	/** 非处决受击到达正式恢复帧后，开放输入白名单并恢复移动响应。 */
	void NotifyCombatReactUnlockFrame();

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool IsCombatReactActive() const { return bCombatReactActive; }

	/** 将当前 PoiseBreak 受击运行态正式交接给目标侧处决 victim 演出。 */
	bool HandOffActiveCombatReactToExecutionVictim(FString* OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	bool WasHandedOffToExecutionVictim() const { return bHandedOffToExecutionVictim; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	FString DescribeCurrentCombatReactState() const;

	UFUNCTION(BlueprintCallable, Category = "Action|CombatReact")
	void PrintCurrentCombatReactStateDebug() const;

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	EActionCombatReactDirection GetCurrentReactDirection() const { return CurrentReactDirection; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	EActionCombatReactPhase GetCurrentReactPhase() const { return CurrentReactPhase; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	FGameplayTag GetCurrentReactEventTag() const { return CurrentReactEventTag; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
	UAnimMontage* GetCurrentReactMontage() const { return CurrentCombatReactMontage; }

	UFUNCTION(BlueprintPure, Category = "Action|CombatReact")
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

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|CombatReact")
	void K2_OnCombatReactStarted(FGameplayTag EventTag, EActionCombatReactDirection ReactDirection, AActor* InstigatorActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|CombatReact")
	void K2_OnCombatReactFinished(FGameplayTag EventTag, bool bWasInterrupted);

protected:
	UAnimMontage* ResolveCombatReactMontage(
		FGameplayTag EventTag,
		const FActionDamagePayload& DamagePayload,
		EActionCombatReactDirection& OutReactDirection) const;

	FGameplayTag ResolveCombatReactStateTag(FGameplayTag EventTag) const;

	FGameplayTag ResolveCombatReactStatusEffectTag(FGameplayTag EventTag) const;

	EActionCombatReactDirection ResolveCombatReactDirection(const FActionDamagePayload& DamagePayload) const;

	void PrepareOwnerCombatStateForReact() const;

	void ApplyCombatReactState(const FGameplayTag& StateTag, const FActionDamagePayload& DamagePayload);

	void ApplyCombatReactStateEffect(const FGameplayTag& StateTag, const FGameplayTag& StatusEffectTag, float Duration);

	void RemoveCombatReactStateEffect();

	void ApplyCombatReactMovementRestriction();

	void ApplyKnockbackIfNeeded(const FActionDamagePayload& DamagePayload) const;

	void ClearCombatReactMovementRestriction();

	bool ShouldWaitForLandingToFinishReact() const;

	void HandleCombatReactLanded();

	void FinishCombatReact(bool bWasInterrupted, bool bBroadcastEvent);

	void StopCurrentCombatReactMontage();

	UAnimInstance* GetOwnerAnimInstance() const;

	AActionCharacterBase* GetOwningCharacter() const;

	UPawnCombatComponent* GetOwningCombatComponent() const;

	class UActionAbilitySystemComponent* GetOwningActionAbilitySystemComponent() const;

	UCharacterMovementComponent* GetOwningMovementComponent() const;

	bool EnsureCombatReactAssetsLoaded();

	const FActionCombatReactMontageSet* GetCombatReactMontageSetForEvent(FGameplayTag EventTag) const;

	bool HasOwningGameplayTag(const FGameplayTag& TagToCheck) const;

	float ResolveCombatReactDuration(UAnimMontage* ReactMontage, const FActionDamagePayload& DamagePayload) const;

	float ResolveCombatReactEffectDuration(const FActionDamagePayload& DamagePayload) const;

	bool HasCombatReactUnlockNotify(const UAnimMontage* ReactMontage) const;

	float ResolveCombatReactUnlockNotifyTriggerTime(const UAnimMontage* ReactMontage) const;

	float ResolveMinimumCombatReactDurationForUnlockFrame(UAnimMontage* ReactMontage, float BaseReactDuration) const;

	void OpenCombatReactRecoveryCancelWindow();

	void CloseCombatReactRecoveryCancelWindow();

	/** 只供处决 victim 演出接管时复位当前受击运行态，不走普通受击收尾语义。 */
	void ResetActiveCombatReactRuntimeForExecutionVictimHandoff();

	/** 受击计时器到期后的统一收尾入口：在处决锁、破韧解锁帧和落地等待之间做最终裁决。 */
	UFUNCTION()
	void OnCombatReactExpired();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|CombatReact")
	FActionCombatReactConfig CombatReactConfig;

private:
	/** 当前正式受击蒙太奇结束时的收尾入口。这里只收当前运行中的那一段，不处理旧蒙太奇晚到回调。 */
	UFUNCTION()
	void HandleCombatReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
	bool bCombatReactActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CurrentCombatReactMontage = nullptr;

	EActionCombatReactDirection CurrentReactDirection = EActionCombatReactDirection::None;

	EActionCombatReactPhase CurrentReactPhase = EActionCombatReactPhase::None;

	FGameplayTag CurrentReactEventTag;

	FGameplayTag CurrentReactStateTag;

	FGameplayTag CurrentReactStatusEffectTag;

	FActiveGameplayEffectHandle CurrentCombatReactEffectHandle;

	FTimerHandle CombatReactTimerHandle;

	bool bWaitingForLanding = false;

	bool bCombatReactMovementRestricted = false;

	float CachedMaxWalkSpeed = 0.f;

	float CachedMaxAcceleration = 0.f;

	float PendingLandingRecoveryDuration = 0.f;

	bool bHoldReactUntilTimerExpires = false;

	bool bCombatReactUnlockFrameReached = false;

	bool bHandedOffToExecutionVictim = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimMontage>> LoadedCombatReactMontages;

	bool bCombatReactAssetsLoaded = false;
};
