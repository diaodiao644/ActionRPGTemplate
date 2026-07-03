#pragma once

#include "ActiveGameplayEffectHandle.h"
#include "CoreMinimal.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "Components/ActorComponent.h"
#include "HeroDefenseComponent.generated.h"

class AActionHeroCharacter;
class UActionAbilitySystemComponent;
class UAnimMontage;
class UHeroCombatComponent;
class UHeroCombatInputComponent;

/**
 * 英雄防御系输入组件。
 * 这一层负责收口闪避、防御以及非攻击输入放行判断，
 * 让 HeroCombatComponent 不再直接持有这部分输入细节。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroDefenseComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UHeroCombatComponent;

public:
	UHeroDefenseComponent();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 非攻击输入门禁与动画查询：
	// 负责判断当前时刻是否允许闪避、防御等非攻击输入进入，以及读取对应动画资源。
	bool CanActivateNonAttackInputNow(const FGameplayTag& InputTag) const;
	UAnimMontage* GetDodgeAnimMontage() const;
	UAnimMontage* GetDefenseAnimMontage() const;
	UAnimMontage* GetBlockedHitAnimMontage() const;
	int32 GetDodgeReactGuardThreshold() const;
	int32 GetDefenseReactGuardThreshold() const;

	// 防御 / 闪避 Ability 生命周期：
	// 当防御或闪避 Ability 开始、结束，或因受击链被强制收尾时，都通过这里同步运行时状态。
	void ApplyDefenseAbilityStarted(UAnimMontage* DefenseMontage);
	void FinalizeDefenseAbilityRuntime(bool bCombatReactResetInProgress);
	void RequireDefenseReleaseBeforeReactivation();
	void ClearDefenseReleaseRequirement();
	void ApplyDodgeAbilityStarted(UAnimMontage* DodgeMontage);
	void FinalizeDodgeAbilityRuntime(bool bCombatReactResetInProgress);

	// 防御 / 闪避状态与闪反资格：
	// 这层负责维护 HeroCombatWindowRuntimeState 对应的本地开关与闪避反击资格。
	void BeginDefenseState();
	void EndDefenseState();
	void BeginDodgeState();
	void EndDodgeState();
	void ClearDodgeCounterAvailability();
	bool ConsumeDodgeCounterAvailability();

	// 输入恢复与受击联动：
	// 防御或闪避结束后，输入恢复会延后到下一帧；受击链重置时也会走这里统一收尾。
	void RequestRecoverCombatInputAfterDodge();
	void RequestRecoverCombatInputAfterDefense();
	void HandleCombatReactStateReset();

	// 受击对防御链的即时反馈：
	// 这一层负责把来袭命中解析成格挡、完防、完闪等防御结果，并处理成功奖励。
	bool TryHandleIncomingDamageDefenseReaction(
		const FActionDamagePayload& InDamagePayload,
		FActionHitResolveResult& OutResult);
	void HandleSuccessfulParry();
	void HandleSuccessfulPerfectDodge();
	void ResetRuntimeStateForHeroStartup();
	bool TryHandleDefenseInputByEvent(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent);

protected:
	// 依赖访问与下一帧恢复工具：
	// 统一封装战斗总控访问，以及“登记延后恢复请求 / 下一帧真正执行”的公共逻辑。
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	void RequestDeferredCombatInputRecovery(
		bool& bRequestFlag,
		void (UHeroDefenseComponent::*Handler)());
	void HandleDeferredCombatInputRecovery(bool& bRequestFlag);

	// 防御窗口命中工具：
	// 用统一入口处理“窗口是否命中、命中后回调谁、最终写什么受击结果”。
	bool TryHandleDefenseWindowReaction(
		bool bShouldTrigger,
		void (UHeroDefenseComponent::*SuccessHandler)(),
		EActionHitResultType SuccessResultType,
		FActionHitResolveResult* OutResult = nullptr);
	bool TryHandleBlockedDefenseReaction(
		bool bShouldBlock,
		EActionHitResultType BlockResultType,
		FActionHitResolveResult* OutResult = nullptr);
	void PlayBlockedHitReaction() const;
	void RestoreWalkingMovementIfLocked() const;
	void RestoreRunMovementIfFastRun() const;

	// 定时窗口开关工具：
	// 完防、完闪与闪反资格都共用这一层定时开窗 / 关窗逻辑，避免每条链重复管理 TimerHandle。
	void OpenTimedDefenseWindow(
		FTimerHandle& TimerHandle,
		void (UHeroDefenseComponent::*ExpireCallback)(),
		float Duration,
		const FGameplayTag& OpenEventTag);
	void CloseTimedDefenseWindow(
		FTimerHandle& TimerHandle,
		bool bWasActive,
		const FGameplayTag& CloseEventTag);
	void OpenParryWindow();
	void CloseParryWindow();
	void OpenPerfectDodgeWindow();
	void ClosePerfectDodgeWindow();
	void EnableDodgeCounterWindow();
	void DisableDodgeCounterWindow();
	void HandleDeferredCombatInputRecoveryAfterDodge();
	void HandleDeferredCombatInputRecoveryAfterDefense();

	// 非攻击输入分发：
	// 负责把 Dodge / Defense 输入继续拆到 Pressed、Held、Released 等不同阶段处理。
	bool HandleDodgeInput(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent);
	bool HandleCombatModeOrDefensePressed(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag);
	bool HandleCombatModeOrDefenseHeld(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag);
	bool TryForwardReleasedAbilityInput(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag);

private:
	// 输入过滤助手：
	// 仅负责判断输入标签语义，不承担运行时状态修改。
	bool IsDefenseInputTag(const FGameplayTag& InputTag) const;
	bool ShouldForwardReleaseToAbilitySystem(const FGameplayTag& InputTag) const;

	// 防御 / 闪避链配置：
	// 这些是窗口时长、奖励值与成功后持续修正效果的静态调参入口。
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense")
	float ParryWindowDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge")
	float PerfectDodgeWindowDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge")
	float DodgeCounterWindowDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Energy")
	float ParrySpecialWeaponSwitchEnergyReward = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Energy")
	float PerfectDodgeSpecialWeaponSwitchEnergyReward = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense")
	FActionCombatModifierEffectSpec ParryWindowCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense")
	FActionCombatModifierEffectSpec ParrySuccessCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge")
	FActionCombatModifierEffectSpec PerfectDodgeSuccessCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge")
	FActionCombatModifierEffectSpec DodgeCounterReadyCombatModifierEffect;

	// 运行时效果句柄与定时器：
	// 用于在窗口结束、状态结束或受击重置时准确移除对应效果并关闭对应窗口。
	FActiveGameplayEffectHandle ParryWindowEffectHandle;
	FActiveGameplayEffectHandle ParrySuccessEffectHandle;
	FActiveGameplayEffectHandle PerfectDodgeSuccessEffectHandle;
	FActiveGameplayEffectHandle DodgeCounterReadyEffectHandle;
	FTimerHandle ParryWindowTimerHandle;
	FTimerHandle PerfectDodgeWindowTimerHandle;
	FTimerHandle DodgeCounterWindowTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDefenseMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDodgeMontage = nullptr;

	// 下一帧恢复与输入门禁标记：
	// 用于避开收尾同帧时序冲突，以及防止防御按住不松时被立刻重复激活。
	UPROPERTY(Transient)
	bool bDeferredCombatInputRecoveryAfterDodgeRequested = false;

	UPROPERTY(Transient)
	bool bRequireDefenseReleaseBeforeReactivation = false;

	UPROPERTY(Transient)
	bool bDeferredCombatInputRecoveryAfterDefenseRequested = false;
};
