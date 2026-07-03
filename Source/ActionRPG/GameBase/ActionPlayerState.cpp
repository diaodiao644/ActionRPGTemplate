// 文件说明：实现 ActionPlayerState 相关逻辑。


#include "GameBase/ActionPlayerState.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

AActionPlayerState::AActionPlayerState()
	:Super()
{
	HeroAbilitySystemComponent = CreateDefaultSubobject<UActionAbilitySystemComponent>(TEXT("HeroAbilitySystemComponent"));
	HeroAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	HeroAttributeSet = CreateDefaultSubobject<UActionAttributeSetBase>(TEXT("HeroAttributeSet"));

	NetUpdateFrequency = 100.f; //设置服务端将此对象每秒更新的次数
}

UAbilitySystemComponent* AActionPlayerState::GetAbilitySystemComponent() const
{
	return GetHeroAbilitySystemComponent();
}

void AActionPlayerState::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (auto ASC = GetHeroAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AActionPlayerState::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return GetHeroAbilitySystemComponent() ? GetHeroAbilitySystemComponent()->HasMatchingGameplayTag(TagToCheck) : false;
}

bool AActionPlayerState::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return GetHeroAbilitySystemComponent() ? GetHeroAbilitySystemComponent()->HasAllMatchingGameplayTags(TagContainer) : false;
}

bool AActionPlayerState::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	return GetHeroAbilitySystemComponent() ? GetHeroAbilitySystemComponent()->HasAnyMatchingGameplayTags(TagContainer) : false;
}
