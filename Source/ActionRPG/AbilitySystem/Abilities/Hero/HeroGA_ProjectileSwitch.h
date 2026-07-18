// 文件说明：声明 Hero 侧法杖切换发射物能力壳。

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "HeroGA_ProjectileSwitch.generated.h"

/**
 * Hero 侧切换发射物主动能力壳。
 * 它只负责承接 `ProjectileSwitch` 输入标签进入 GAS 后的一次主动切换请求，
 * 并把这次请求落到正式宿主，不持有跨激活的发射物选择状态。
 *
 * 它不是发射物静态模板入口，也不是“当前已选发射物标签”的正式状态源。
 * 当前已选发射物配置标签仍回到 HeroLoadoutContextComponent，
 * 可切换配置入口和静态模板仍回到 WeaponDefinition.SwitchableProjectileConfigs。
 */
UCLASS()
class ACTIONRPG_API UHeroGA_ProjectileSwitch : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_ProjectileSwitch();

protected:
	/** 校验当前武器 / 当前配置是否允许切换发射物。它只做这条能力壳的补充前置校验，不替代正式输入裁决。 */
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;

	/** 承接一次切换请求，并把目标切换结果提交给正式宿主。它不是发射物配置本体，也不持有跨激活状态。 */
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
