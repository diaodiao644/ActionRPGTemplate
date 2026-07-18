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
 * 它是防御 / 闪避窗口、完防 / 完闪 / 闪反资格和对应恢复链的正式宿主，
 * 但不是攻击状态源，也不是 GA 生命周期本体。
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
	/** 判断当前时刻是否允许某个非攻击输入正式进入。它只做门禁裁决，不直接推进 Ability 激活。 */
	bool CanActivateNonAttackInputNow(const FGameplayTag& InputTag) const;
	/** 判断“主动接管型非攻击 GA”当前是否通过共享硬门禁，可以进入 ASC 关系裁决。 */
	bool CanEnterRelationshipActivationForNonAttackInput(
		const FGameplayTag& InputTag,
		FString* OutFailureReason = nullptr) const;
	/** 读取当前正式应使用的闪避蒙太奇。 */
	UAnimMontage* GetDodgeAnimMontage() const;
	/** 读取当前正式应使用的防御蒙太奇。 */
	UAnimMontage* GetDefenseAnimMontage() const;
	/** 读取成功格挡后应播放的受击反馈蒙太奇。 */
	UAnimMontage* GetBlockedHitAnimMontage() const;
	/** 读取闪避受击判定中用于区分 Guard 结果的阈值。 */
	int32 GetDodgeReactGuardThreshold() const;
	/** 读取防御受击判定中用于区分 Guard 结果的阈值。 */
	int32 GetDefenseReactGuardThreshold() const;

	// 防御 / 闪避 Ability 生命周期：
	// 当防御或闪避 Ability 开始、结束，或因受击链被强制收尾时，都通过这里同步运行时状态。
	void ApplyDefenseAbilityStarted(UAnimMontage* DefenseMontage);
	void FinalizeDefenseAbilityRuntime(bool bCombatReactResetInProgress);
	/** 要求防御键必须先释放后才能再次激活，避免持续按住导致防御立即重入。 */
	void RequireDefenseReleaseBeforeReactivation();
	void ClearDefenseReleaseRequirement();
	void ApplyDodgeAbilityStarted(UAnimMontage* DodgeMontage);
	void FinalizeDodgeAbilityRuntime(bool bCombatReactResetInProgress);

	// 防御 / 闪避状态与闪反资格：
	// 这层负责维护 HeroCombatWindowRuntimeState 对应的本地开关与闪避反击资格。
	// 它写回的是正式窗口 runtime，不等于 Ability 自身是否仍激活。
	void BeginDefenseState();
	void EndDefenseState();
	void BeginDodgeState();
	void EndDodgeState();
	void ClearDodgeCounterAvailability();
	/** 消费一次闪避反击资格。它只对下一次正式使用生效，消费后即失效。 */
	bool ConsumeDodgeCounterAvailability();

	// 输入恢复与受击联动：
	// 防御或闪避结束后，输入恢复会延后到下一帧；受击链重置时也会走这里统一收尾。
	// 正式按钮阶段、缓冲输入和 Held 回放仍继续回到 HeroCombatInputComponent。
	void RequestRecoverCombatInputAfterDodge();
	void RequestRecoverCombatInputAfterDefense();
	/** 受击链接管前统一回收防御 / 闪避运行态，避免旧窗口残留。 */
	void HandleCombatReactStateReset();

	// 受击对防御链的即时反馈：
	// 这一层负责把来袭命中解析成格挡、完防、完闪等防御结果，并处理成功奖励。
	// 成功奖励只服务本次窗口命中后的局部收益，不构成第二套战斗状态源。
	bool TryHandleIncomingDamageDefenseReaction(
		const FActionDamagePayload& InDamagePayload,
		FActionHitResolveResult& OutResult);
	void HandleSuccessfulParry();
	void HandleSuccessfulPerfectDodge();
	void ResetRuntimeStateForHeroStartup();
	/** 按输入阶段继续处理一次 Dodge / Defense 相关输入。 */
	bool TryHandleDefenseInputByEvent(
		UActionAbilitySystemComponent* InActionASC,
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent);

protected:
	// 依赖访问与下一帧恢复工具：
	// 统一封装战斗总控访问，以及“登记延后恢复请求 / 下一帧真正执行”的公共逻辑。
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;
	UHeroCombatInputComponent* GetOwningHeroCombatInputComponent() const;
	/** 统一登记一条下一帧输入恢复请求，避开本帧收尾冲突。 */
	void RequestDeferredCombatInputRecovery(
		bool& bRequestFlag,
		void (UHeroDefenseComponent::*Handler)());
	/** 执行并清空一条已经登记的下一帧输入恢复请求。 */
	void HandleDeferredCombatInputRecovery(bool& bRequestFlag);

	// 防御窗口命中工具：
	// 用统一入口处理“窗口是否命中、命中后回调谁、最终写什么受击结果”。
	// 它只消费当前窗口 runtime，不回头创造新的命中窗口或攻击状态。
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
	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense", meta = (ClampMin = "0.0", ToolTip = "完防窗口持续时长，单位秒。它只决定防御起手后多长时间内按正式规则视为精准格挡。"))
	float ParryWindowDuration = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge", meta = (ClampMin = "0.0", ToolTip = "完美闪避窗口持续时长，单位秒。它只服务 Dodge 成功后的额外资格判断，不替代闪避主状态本身。"))
	float PerfectDodgeWindowDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge", meta = (ClampMin = "0.0", ToolTip = "闪避反击资格窗口持续时长，单位秒。窗口结束后，这次局部反击授权会被正式回收。"))
	float DodgeCounterWindowDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Energy", meta = (ToolTip = "成功完防后奖励的特殊切武能量。设为 0 只是不奖励能量，不影响完防本身是否成立。"))
	float ParrySpecialWeaponSwitchEnergyReward = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Energy", meta = (ToolTip = "成功完美闪避后奖励的特殊切武能量。设为 0 只是不奖励能量，不影响完美闪避本身是否成立。"))
	float PerfectDodgeSpecialWeaponSwitchEnergyReward = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense", meta = (ToolTip = "完防窗口开启期间施加给拥有者的临时战斗修正。它只服务窗口期资格和表现，不是新的长期状态。"))
	FActionCombatModifierEffectSpec ParryWindowCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Defense", meta = (ToolTip = "完防成功后施加的临时战斗修正。"))
	FActionCombatModifierEffectSpec ParrySuccessCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge", meta = (ToolTip = "完美闪避成功后施加的临时战斗修正。"))
	FActionCombatModifierEffectSpec PerfectDodgeSuccessCombatModifierEffect;

	UPROPERTY(EditDefaultsOnly, Category = "CombatConfig|Dodge", meta = (ToolTip = "闪避反击资格窗口开启时施加的临时战斗修正。它只表达“当前可反击”，不替代攻击或防御正式状态源。"))
	FActionCombatModifierEffectSpec DodgeCounterReadyCombatModifierEffect;

	// 运行时效果句柄与定时器：
	// 用于在窗口结束、状态结束或受击重置时准确移除对应效果并关闭对应窗口。
	// 这些句柄和计时器都只是局部运行时辅助，不是长期正式状态源。
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
	// 它们都只是局部门禁 / 时序标记，不构成新的输入状态源。
	UPROPERTY(Transient)
	bool bDeferredCombatInputRecoveryAfterDodgeRequested = false;

	/** 当前是否要求防御键先释放后才能再次进入正式防御链。 */
	UPROPERTY(Transient)
	bool bRequireDefenseReleaseBeforeReactivation = false;

	/** 下一帧是否需要在防御收尾后恢复战斗输入。 */
	UPROPERTY(Transient)
	bool bDeferredCombatInputRecoveryAfterDefenseRequested = false;
};
