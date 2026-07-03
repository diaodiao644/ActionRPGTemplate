// 文件说明：声明 ActionPlayerState 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GameplayTagAssetInterface.h"

#include "ActionPlayerState.generated.h"

class UActionAbilitySystemComponent;
class UActionAttributeSetBase;

UCLASS()
class ACTIONRPG_API AActionPlayerState : public APlayerState, public IAbilitySystemInterface, public IGameplayCueInterface, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AActionPlayerState();

public:
	// IAbilitySystemInterface：向外暴露能力系统组件。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IGameplayTagAssetInterface：透传标签查询能力。
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

public:
	// 获取英雄能力系统组件与属性集。
	UActionAbilitySystemComponent* GetHeroAbilitySystemComponent() const { return HeroAbilitySystemComponent; }
	UActionAttributeSetBase* GetHeroAttributeSet() const { return HeroAttributeSet; }

protected:
	// PlayerState 持有的能力系统组件。
	UPROPERTY()
	UActionAbilitySystemComponent* HeroAbilitySystemComponent = nullptr;

	// PlayerState 持有的属性集。
	UPROPERTY()
	UActionAttributeSetBase* HeroAttributeSet = nullptr;
};
