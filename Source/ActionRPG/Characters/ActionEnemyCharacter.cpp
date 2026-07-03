// 文件说明：实现 ActionEnemyCharacter 相关逻辑。


#include "Characters/ActionEnemyCharacter.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Components/Attribute/EnemyAttributeComponent.h"

AActionEnemyCharacter::AActionEnemyCharacter()
	:Super()
{
	ActionAbilitySystemComponent = CreateDefaultSubobject<UActionAbilitySystemComponent>(TEXT("EnemyASC"));
	ActionAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	ActionAttributeSet = CreateDefaultSubobject<UActionAttributeSetBase>(TEXT("EnemyAttributeSet"));

	EnemyAttributeComponent = CreateDefaultSubobject<UEnemyAttributeComponent>(TEXT("EnemyAttributeComponent"));

}

void AActionEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ActionAbilitySystemComponent->InitAbilityActorInfo(this, this);

	InitializeDefaultAttributes();

	if (ActionAttributeSet)
	{
		ActionAttributeSet->InitBindAttributeChangeDelegate();
	}

	if (EnemyAttributeComponent)
	{
		EnemyAttributeComponent->InitializeWithAbilitySystem(ActionAbilitySystemComponent);
	}

	if (EnemyAttributeComponent)
	{
		EnemyAttributeComponent->ForceRefreshAttributeChange();
	}
}
