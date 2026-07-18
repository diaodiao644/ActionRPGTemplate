// 文件说明：声明英雄切武 Ability，负责统一处理手动切槽后的普通切武与特殊切武。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "HeroGA_WeaponSwitch.generated.h"

class UAnimMontage;

/**
 * 英雄切武表现壳。
 * 这层只负责“真实切武结果已经落地之后”的单次表现期流程：
 * 1. 读取当前手动指定的目标固定槽；
 * 2. 先让正式宿主完成真实装备切换；
 * 3. 若切换成立，再决定特殊切武是否进入表现期，或普通切武是否登记下一帧轻攻击首段 handoff。
 *
 * 它不持有切武事务状态源。
 * 正式目标槽、当前已装备结果、真实切武事务与特殊切武资格，
 * 仍由 HeroWeaponSwitchComponent / HeroEquipmentComponent 维护；
 * 这个 GA 只缓存“本次表现期是否已开始”“本次表现壳是否已收尾”
 * 和“本次真正播放了哪段切武蒙太奇”这类短生命周期局部状态。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_WeaponSwitch : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_WeaponSwitch();

	/**
	 * 激活切武 Ability。
	 * 执行顺序如下：
	 * 1. 从战斗组件解析本次玩家手动指定的目标固定武器槽；
	 * 2. 先完成目标武器的真实装备切换；
	 * 3. 若目标武器已完成正式切换，则普通切武先结束 WeaponSwitch，再由组件下一帧 handoff 到默认轻攻击首段；
	 *    特殊切武继续进入独立表现期。
	 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** Ability 结束入口：兜底处理外部强制结束，确保表现期收尾不依赖单一蒙太奇回调。它只收表现壳，不回滚真实切武事务。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	/** 在关系系统真正放行前，先校验目标固定槽、切武门禁和表现资源。这里只做补充预检，不推进真实切武事务。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	/** 切武表现蒙太奇正常播放完毕后的回调。它只汇流到统一表现期收尾。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;

	/** 切武表现蒙太奇自然 BlendOut 时的回调。它只汇流到统一表现期收尾。 */
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;

	/** 切武表现蒙太奇被打断时的回调。它只汇流到统一表现期收尾。 */
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;

	/** 切武表现蒙太奇被取消时的回调。它只汇流到统一表现期收尾。 */
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

private:
	/** 切武表现真正开播前，复用 Targeting 正式策略做一次朝向裁决。它只服务本次表现期，不改变真实切武事务结果，也不形成新的持续朝向状态。 */
	void TryApplySpecialWeaponSwitchPresentationFacing();

	/** 只锁切武表现期移动；真实切武事务状态仍由 HeroWeaponSwitchComponent 持有。 */
	void ApplySpecialWeaponSwitchPresentationMoveLock(bool bLocked);
	/** 真正执行切武 Ability 收尾：只收表现期壳，不回滚已经成功的切武事务。进入这里时真实装备切换结果应继续由组件侧保持。 */
	void FinalizeWeaponSwitchAbilityRuntime(bool bWasCancelled);

	/** 统一收口 Ability 结束逻辑，避免多个动画回调重复结束同一条 Ability。它只负责表现期汇流，不替代组件侧事务状态。 */
	void FinishWeaponSwitchAbility(bool bWasCancelled);

private:
	/** 防止多个动画结束回调重复收尾。它只描述本次表现期收尾是否已经执行过。 */
	bool bAbilityFinished = false;

	/** 记录本次 Ability 是否进入了切武表现播放期。它只描述本次表现壳生命周期，不代表真实切武事务是否成立。 */
	bool bSpecialWeaponSwitchPresentationStarted = false;

	/** 缓存本次切武真正播放的蒙太奇，仅用于收尾时精确清理动画上下文。它只是本次表现层上下文，不是新的切武状态源。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveWeaponSwitchMontage = nullptr;
};
