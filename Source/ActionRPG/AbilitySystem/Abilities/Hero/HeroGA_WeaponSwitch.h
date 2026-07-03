// 文件说明：声明英雄切武 Ability，负责统一处理手动切槽后的普通切武与特殊切武。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "HeroGA_WeaponSwitch.generated.h"

class UAnimMontage;

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
	 * 3. 若目标武器已完成正式切换，则按“特殊切武优先、普通切武兜底”的规则进入对应表现期。
	 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	/** Ability 结束入口：兜底处理外部强制结束，确保表现期收尾不依赖单一蒙太奇回调。 */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;
	/** 切武表现蒙太奇正常播放完毕后的回调。 */
	virtual void OnHeroMontageCompleted(FName MontageContext) override;

	/** 切武表现蒙太奇自然 BlendOut 时的回调。 */
	virtual void OnHeroMontageBlendOut(FName MontageContext) override;

	/** 切武表现蒙太奇被打断时的回调。 */
	virtual void OnHeroMontageInterrupted(FName MontageContext) override;

	/** 切武表现蒙太奇被取消时的回调。 */
	virtual void OnHeroMontageCancelled(FName MontageContext) override;

private:
	/** 切武表现真正开播前，复用 Targeting 正式策略做一次朝向裁决。 */
	void TryApplyWeaponSwitchPresentationFacing();

	/** 只锁切武表现期移动；真实切武事务状态仍由 HeroWeaponSwitchComponent 持有。 */
	void ApplyWeaponSwitchPresentationMoveLock(bool bLocked);
	/** 真正执行切武 Ability 收尾：只收表现期壳，不回滚已经成功的切武事务。 */
	void FinalizeWeaponSwitchAbilityRuntime(bool bWasCancelled);

	/** 统一收口 Ability 结束逻辑，避免多个动画回调重复结束同一条 Ability。 */
	void FinishWeaponSwitchAbility(bool bWasCancelled);

private:
	/** 防止多个动画结束回调重复收尾。 */
	bool bAbilityFinished = false;

	/** 记录本次 Ability 是否进入了切武表现播放期。 */
	bool bWeaponSwitchPresentationStarted = false;

	/** 缓存本次切武真正播放的蒙太奇，仅用于收尾时精确清理动画上下文。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveWeaponSwitchMontage = nullptr;
};
