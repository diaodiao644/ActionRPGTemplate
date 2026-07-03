// File: Projectile switch ability for ranged staff weapons.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "HeroGA_ProjectileSwitch.generated.h"

UCLASS()
class ACTIONRPG_API UHeroGA_ProjectileSwitch : public UActionHeroGameplayAbility
{
	GENERATED_BODY()

public:
	UHeroGA_ProjectileSwitch();

protected:
	virtual bool ValidateRelationshipActivationPreconditions(FString& OutFailureReason) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
