#include "AnimNotifyState/AnimNotifyState_AbilityCancelWindow.h"

#include "Animation/AnimMontage.h"
#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"

namespace
{
	static bool IsCurrentCombatMontageNotifyForCancelWindow(
		const UHeroCombatComponent* HeroCombatComponent,
		UAnimSequenceBase* Animation)
	{
		if (!HeroCombatComponent)
		{
			return false;
		}

		UAnimMontage* CurrentRunningMontage = HeroCombatComponent->GetCurrentRunningAnimMontage();
		if (!CurrentRunningMontage)
		{
			return false;
		}

		if (UAnimMontage* NotifyMontage = Cast<UAnimMontage>(Animation))
		{
			return CurrentRunningMontage == NotifyMontage;
		}

		return true;
	}
}

UAnimNotifyState_AbilityCancelWindow::UAnimNotifyState_AbilityCancelWindow()
	: Super()
{
	AllowedInputTags.AddTag(ActionGameplayTags::InputTag_GameplayAbility_Dodge);
	AllowedInputTags.AddTag(ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense);
	AllowedInputTags.AddTag(ActionGameplayTags::InputTag_GameplayAbility_Execution);
}

FString UAnimNotifyState_AbilityCancelWindow::GetNotifyName_Implementation() const
{
	return TEXT("AbilityCancelWindow");
}

void UAnimNotifyState_AbilityCancelWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		// 取消窗口只应该作用在当前仍然有效的战斗蒙太奇上。
		if (!IsCurrentCombatMontageNotifyForCancelWindow(HeroCombatComponent, Animation))
		{
			return;
		}

		if (bEnableAttackInputOnBegin)
		{
			HeroCombatComponent->SetAttackEnabled(true);
		}

		HeroCombatComponent->OpenAbilityCancelWindow(AllowedInputTags);

		if (bConsumeBufferedInputOnBegin)
		{
			if (UHeroCombatInputComponent* HeroCombatInputComponent = ResolveHeroCombatInputComponent(MeshComp))
			{
				if (!HeroCombatInputComponent->ConsumeBufferedInput())
				{
					HeroCombatInputComponent->RequestConsumeBufferedInputOnNextTick();
				}
			}
		}
	}
}

void UAnimNotifyState_AbilityCancelWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		// 如果当前战斗蒙太奇已经结束或切到了别的动作，
		// 旧取消窗口的尾通知不应该再改写窗口和攻击开关。
		if (!IsCurrentCombatMontageNotifyForCancelWindow(HeroCombatComponent, Animation))
		{
			return;
		}

		HeroCombatComponent->CloseAbilityCancelWindow();

		if (bDisableAttackInputOnEnd)
		{
			HeroCombatComponent->SetAttackEnabled(false);
		}
	}
}

UHeroCombatComponent* UAnimNotifyState_AbilityCancelWindow::ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(MeshComp->GetOwner());
	if (!HeroCharacter)
	{
		return nullptr;
	}

	return HeroCharacter->GetHeroCombatComponent();
}

UHeroCombatInputComponent* UAnimNotifyState_AbilityCancelWindow::ResolveHeroCombatInputComponent(
	USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(MeshComp->GetOwner());
	if (!HeroCharacter)
	{
		return nullptr;
	}

	return HeroCharacter->GetHeroCombatInputComponent();
}
