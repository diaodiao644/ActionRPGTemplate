#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "ActionType/ActionEffectTypes.h"
#include "HeroGA_CombatModeOrDefense.generated.h"

/**
 * “进入普通战斗姿态 / 正式防御”共用输入壳。
 * 这层只负责“本次激活”的通用时序：
 * 1. 根据当前 CombatMode 把同一输入分流成姿态切换或正式防御；
 * 2. 在进入防御时启动运行态、附加临时战斗修正效果并监听输入释放；
 * 3. 在退出防御时把正式收尾、重入门禁和输入恢复统一交回组件侧。
 *
 * 它不持有长期防御状态源。
 * 正式防御运行态、窗口、输入恢复与最终收尾仍由 HeroDefenseComponent /
 * HeroCombatComponent 维护；这个 GA 只缓存“这次是否真正进入过防御态”“是否已正式收尾”
 * 以及“释放兜底轮询当前是否在运行”这类短生命周期局部状态。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_CombatModeOrDefense : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_CombatModeOrDefense();

public:
	/** Ability 激活入口：重置本次局部时序状态，并按当前 CombatMode 分流姿态切换或防御起手。它不在这里创建长期防御状态源。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** Ability 结束入口：兜底处理外部强制结束，确保正式收尾不依赖单一释放回调或蒙太奇回调。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** 在关系系统真正放行前，先校验非攻击输入门禁与当前语义所需资源。这里只做补充预检，不写正式运行态。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;

	/** 输入释放主入口：它只消费本次 `WaitInputRelease` 已绑定的 `OnRelease` 回调；只有松手才算正常退出防御，不把蒙太奇自然结束当作正式退出信号。 */
	UFUNCTION()
	void HandleDefenseInputReleased(float HeldTime);

	/** 启动防御释放的兜底轮询。它只兜“释放事件漏掉”的极端时序，不是第二套正式输入源。 */
	void StartDefenseReleaseWatch();
	/** 停止防御释放兜底轮询。 */
	void StopDefenseReleaseWatch();
	/** 轮询检查防御输入是否仍在按住；若未按住，则主动转入统一释放链。 */
	void HandleDeferredDefenseReleaseWatch();
	/** 从正式输入组件读取当前防御输入是否仍然按住，仅供释放兜底轮询使用。 */
	bool IsDefenseInputStillHeld() const;

	/** 只有已处于 Combo 时，才把这次输入解释成正式防御并启动后续状态型流程；这里会创建并绑定 `WaitInputRelease`，但不会在 GA 本地维护第二套正式输入状态。 */
	void DefenseLogic();
	/** “准备退出防御”的统一入口：先做正式收尾，再决定是否立刻结束 Ability 对象。 */
	void FinishDefenseAbility(bool bShouldEndAbility);
	/** 真正执行防御收尾：清理防御运行态、停止释放兜底并把最终状态交还组件侧。它只收这次激活，不替代组件侧长期门禁与窗口收尾。 */
	void FinalizeDefenseAbility();

	/** 共享蒙太奇回调：防御没有攻击那种双层过渡语义，各回调只负责汇入防御退出链或维持持续态，不单独定义新的防御阶段机。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

private:
	/** 只表示这次激活是否已真正进入防御态；Idle -> Combo 的轻量姿态切换不会把它置为 true。 */
	bool bDefenseStateStarted = false;
	/** 只表示本次正式防御收尾是否已经执行过，避免多路径重入。 */
	bool bDefenseAbilityFinished = false;
	/** 只记录这次退出是否来自真实输入释放，用来决定是否要求先松手再允许重新进入防御。 */
	bool bDefenseEndedByInputRelease = false;
	/** 只表示释放兜底轮询当前是否在运行，不是新的输入状态源。 */
	bool bDefenseReleaseWatchActive = false;

protected:
	/** 防御期间附加的临时战斗修正效果。它是资产侧可配置内容，不是运行时自动推导状态，也不是正式防御态来源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense", meta = (ToolTip = "防御期间附加的临时战斗修正效果。它是资产侧可配置内容，不是运行时自动推导状态，也不是正式防御态来源；留空时防御仍可工作，只是不额外挂额外修正。"))
	FActionCombatModifierEffectSpec DefenseCombatModifierEffect;
};
