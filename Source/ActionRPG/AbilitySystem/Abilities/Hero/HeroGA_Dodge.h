// 文件说明：声明闪避 Gameplay Ability 的接口。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "ActionType/ActionEffectTypes.h"
#include "HeroGA_Dodge.generated.h"

/**
 * 闪避 Ability 主链壳。
 * 这层只负责“本次闪避激活”的通用时序：
 * 1. 补充关系预检里的闪避前置条件；
 * 2. 解析当前武器给出的闪避蒙太奇并启动闪避运行态；
 * 3. 在 Ability 生命周期内附加临时战斗修正效果；
 * 4. 在动作结束时把移动恢复、输入恢复和正式收尾统一交回组件侧。
 *
 * 它不持有长期闪避状态源。
 * 正式闪避运行态、窗口、输入恢复与最终收尾仍由 HeroDefenseComponent /
 * HeroCombatComponent 维护；这个 GA 只缓存“本次是否已经正式收尾”和
 * “起手时是否属于移动闪避”这类短生命周期的局部时序信息。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_Dodge : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_Dodge();

public:
	/** Ability 激活入口：重置本次闪避的局部时序状态，并驱动完整的闪避起手流程。它不在这里持有长期闪避状态源。 */
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	/** Ability 结束入口：兜底处理外部强制结束，确保正式收尾不依赖单一蒙太奇回调。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** 在关系系统真正放行前，先校验闪避门禁与表现资源。这里只做补充预检，不写正式运行态。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	/** 蒙太奇回调侧的统一结束入口，避免多个结束回调各自重复触发正式收尾。 */
	void FinishDodgeAbility();
	/** 真正执行闪避收尾：清理闪避运行态、恢复移动、并在合适时序下交还输入系统。它只收这次激活，不额外定义新的闪避状态机。 */
	void FinalizeDodgeAbility();
	/** 根据闪避起手与结束时的移动输入快照，判断是否要在收尾后保留 FastRun。这里读取的是局部快照，不替代正式输入运行态。 */
	bool ShouldKeepFastRunAfterDodge();

	/** 共享蒙太奇回调：无论正常结束、混出、中断还是取消，都只负责收束到同一条结束链。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

	/** 防止蒙太奇多个结束回调重复进入闪避收尾。它只描述“本次正式收尾是否已经执行过”。 */
	bool bDodgeAbilityFinished = false;
	/** 只记录这次闪避起手时是否需要移动闪避，用来决定结束时是否继续保留 FastRun。 */
	bool bRequestedFastRunFromDodge = false;

protected:
	/** 闪避演出期间附加的临时战斗修正效果。它是资产侧可配置内容，不是运行时自动推导状态，也不是正式闪避窗口来源。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ToolTip = "闪避演出期间附加的临时战斗修正效果。它是资产侧可配置内容，不是运行时自动推导状态，也不是正式闪避窗口来源；留空时闪避主链仍可正常工作。"))
	FActionCombatModifierEffectSpec DodgeCombatModifierEffect;
};
