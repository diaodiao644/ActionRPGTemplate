#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "ActionType/ActionEffectTypes.h"
#include "HeroGA_Execution.generated.h"

class UAnimMontage;

/**
 * 处决 Ability 主链壳。
 * 这层只负责“本次处决激活”的通用时序：
 * 1. 校验当前是否存在合法处决目标与处决演出资源；
 * 2. 正式预占目标的处决窗口并启动处决演出；
 * 3. 在 Ability 生命周期内维护执行者保护和命中帧结算；
 * 4. 在动作结束时统一收执行者侧状态，并把目标侧正式收尾交还给目标自己的组件主链。
 *
 * 它不持有长期处决状态源。
 * 正式处决资格、目标预占、命中结算、目标锁与最终收尾仍由 HeroCombatComponent
 * 和目标侧相关链路统一维护；这个 GA 只缓存“本次处决当前锁定了谁、是否已经预占、
 * 是否已经命中、是否已经正式开始和是否已经正式收尾”这类短生命周期局部状态。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_Execution : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_Execution();

public:
	/** Ability 激活入口：重置本次局部时序状态，并把处决资格检查推进到正式预占与演出执行。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** Ability 结束入口：兜底处理外部强制结束，确保正式收尾不依赖单一蒙太奇回调。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** 在关系系统真正放行前，先校验处决目标与表现资源。这里只做补充预检，不写正式运行态。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	/** 共享蒙太奇回调：无论自然结束还是中断，都只负责把不同动画结束路径汇到同一条处决收尾链。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Execution")
	/** 供 AnimNotify 精确驱动的命中帧入口。只有真正进入处决态后，才允许从这里消费预占窗口。 */
	void NotifyExecutionHitFrame();

private:
	/** 正式消费本次已预占的处决窗口，并对本次锁定的目标结算一次处决命中。 */
	bool TryApplyExecutionHit();
	/** 正式发起目标侧处决前准备链。成功后不代表已经开始播放处决演出。 */
	bool TryBeginExecutionPreparation();
	/** 轮询目标侧处决前准备的统一入口；准备完成后再正式启动双边处决演出。 */
	void HandleDeferredExecutionPreparation();
	/** 在目标完成转向后，正式启动执行者与目标侧的双边处决演出。 */
	bool TryStartExecutionPresentationIfReady(FString* OutFailureReason = nullptr);
	/** 目标侧准备完成后，执行者在正式开播前立刻回朝当前已预占目标。 */
	bool FaceExecutionTargetImmediately(FString* OutFailureReason = nullptr) const;
	/** 双边处决真正开播前，按当前武器静态配置尝试补一次水平目标距离。 */
	void TryApplyExecutionStartDistanceAdjustment();
	/** 对单个参与处决的 Actor 做一次带碰撞尝试的瞬时挪位。 */
	bool TryMoveExecutionActorToLocation(AActor* InActor, const FVector& InTargetLocation, FString* OutFailureReason = nullptr) const;
	/** 开关执行者在处决期间的移动与视角输入锁，只服务本次处决 Ability 生命周期。 */
	void SetExecutionPerformerInputLock(bool bLocked);
	/** 请求在下一帧继续检查目标侧处决前准备是否已经完成。 */
	void RequestContinueExecutionPreparationNextTick();
	/** 真正执行处决收尾：关闭执行者侧保护，并按命中结果决定是否回退预占或走目标侧 abort。 */
	void FinalizeExecutionState(bool bWasCancelled);
	/** 开关执行者自身的处决保护效果；它只服务执行者侧，不代表目标侧锁定状态。 */
	void SetExecutionInvulnerabilityEnabled(bool bEnabled);
	/** 动画回调侧的统一结束入口：先做正式收尾，再结束 Ability 对象。 */
	void FinishExecutionAbility(bool bWasCancelled);
	/** 更新准备阶段诊断文本，并按阶段变化输出一次屏幕提示与日志。 */
	void UpdateExecutionPreparationStage(const FString& InStageText, const FColor& InColor, bool bLogAsWarning = false);
	/** 读取本次准备阶段已等待时长。 */
	float GetExecutionPreparationElapsedSeconds() const;
	/** 判断本次准备阶段是否已经超时。 */
	bool HasExecutionPreparationTimedOut() const;

private:
	/** 只缓存本次已经正式锁定并预占的处决目标；命中帧只能对它结算。 */
	TWeakObjectPtr<AActor> CachedExecutionTargetActor;
	/** 只表示本次处决窗口是否已被正式预占但尚未消费完成。 */
	bool bExecutionWindowReserved = false;
	/** 只表示命中帧是否已经完成过正式处决结算，防止重复落伤。 */
	bool bExecutionHitApplied = false;
	/** 只表示这次处决是否已真正进入正式双边演出阶段。 */
	bool bExecutionStarted = false;
	/** 只表示当前是否仍在等待目标完成处决前转向准备。 */
	bool bExecutionPreparationPending = false;
	/** 只表示本次正式收尾是否已经执行过，避免多路径重入。 */
	bool bAbilityFinished = false;
	/** 当前这次准备阶段开始的世界时间。 */
	float ExecutionPreparationStartWorldTime = -1.f;
	/** 当前这次准备阶段已轮询次数。 */
	int32 ExecutionPreparationPollCount = 0;
	/** 当前这次准备阶段最近一次已输出的阶段文本。 */
	FString LastExecutionPreparationStageText;

protected:
	/** 处决期间附加在执行者自身的临时战斗修正效果。它是资产侧可配置内容，不代表目标侧锁定来源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution")
	FActionCombatModifierEffectSpec ExecutionInvulnerabilityCombatModifierEffect;

	/** 处决前准备阶段的正式超时秒数。它独立于目标转向时长，只用于避免准备链无限挂住。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Execution")
	float ExecutionPreparationTimeoutSeconds = 5.f;

private:
	/** 执行者处决保护效果的精确句柄，用于收尾时按句柄移除，而不是按标签粗暴清理。 */
	FActiveGameplayEffectHandle ExecutionInvulnerabilityEffectHandle;

	/** 目标侧处决前准备轮询句柄。 */
	FTimerHandle ExecutionPreparationPollTimerHandle;

	/** 缓存本次真正播放的处决蒙太奇，仅用于精确清理动画上下文。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveExecutionMontage = nullptr;
};
