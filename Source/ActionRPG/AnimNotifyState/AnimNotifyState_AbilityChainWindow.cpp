#include "AnimNotifyState/AnimNotifyState_AbilityChainWindow.h"

#include "Animation/AnimMontage.h"
#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"

namespace
{
	static bool IsCurrentCombatMontageNotifyForChainWindow(
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

UAnimNotifyState_AbilityChainWindow::UAnimNotifyState_AbilityChainWindow()
	: Super()
{
	AllowedInputTags.AddTag(ActionGameplayTags::InputTag_GameplayAbility_Attack);
}

FString UAnimNotifyState_AbilityChainWindow::GetNotifyName_Implementation() const
{
	return TEXT("AbilityChainWindow");
}

void UAnimNotifyState_AbilityChainWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		// 这里只在当前仍由这段战斗蒙太奇主导时打开接段窗口，
		// 避免旧蒙太奇迟到回调污染已经切到别的动作的新运行态。
		if (!IsCurrentCombatMontageNotifyForChainWindow(HeroCombatComponent, Animation))
		{
			return;
		}

		if (bEnableAttackInputOnBegin)
		{
			HeroCombatComponent->SetAttackEnabled(true);
		}

		HeroCombatComponent->OpenAbilityChainWindow(AllowedInputTags, bConsumeBufferedInputOnBegin);
	}
}

void UAnimNotifyState_AbilityChainWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		// 攻击 GA 结束时会先清掉 CurrentRunningMontage 并恢复攻击输入。
		// 如果旧通知在这之后才到达，这里必须直接忽略，
		// 否则会把已经恢复好的输入再次关掉，造成“站在 Combat 姿态但所有输入失效”。
		if (!IsCurrentCombatMontageNotifyForChainWindow(HeroCombatComponent, Animation))
		{
			return;
		}

		HeroCombatComponent->CloseAbilityChainWindow();

		if (bDisableAttackInputOnEnd)
		{
			HeroCombatComponent->SetAttackEnabled(false);
		}
	}
}

UHeroCombatComponent* UAnimNotifyState_AbilityChainWindow::ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const
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
