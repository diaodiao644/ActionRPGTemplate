// 文件说明：实现处决命中帧动画通知，在精确帧回调当前激活的处决 Ability。

#include "AnimNotify/AnimNotify_ExecutionHit.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Execution.h"
#include "Characters/ActionCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_ExecutionHit::UAnimNotify_ExecutionHit()
	: Super()
{
}

FString UAnimNotify_ExecutionHit::GetNotifyName_Implementation() const
{
	return TEXT("ExecutionHit");
}

void UAnimNotify_ExecutionHit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(MeshComp->GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	UActionAbilitySystemComponent* ActionASC =
		Cast<UActionAbilitySystemComponent>(OwnerCharacter->GetAbilitySystemComponent());
	if (!ActionASC)
	{
		return;
	}

	// 只寻找“当前正在激活的处决 Ability 实例”，
	// 避免错误触发到未激活的 CDO 或其它输入共享的能力。
	UHeroGA_Execution* ExecutionAbility =
		Cast<UHeroGA_Execution>(ActionASC->GetActiveAbilityInstanceByAbilityTag(ActionGameplayTags::Player_Ability_Execution));
	if (!ExecutionAbility)
	{
		return;
	}

	ExecutionAbility->NotifyExecutionHitFrame();
}
