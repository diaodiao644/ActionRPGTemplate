#include "AnimNotifyState/AnimNotifyState_AbilityInterruptWindow.h"

#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "ActionGameplayTags.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"

namespace
{
	// InterruptWindow 只允许当前仍由 HeroCombatComponent 认定为“正在运行”的那条战斗蒙太奇打开窗口。
	// 这样可以避免旧蒙太奇残帧或并行动画通知把例外窗口错误地桥接回当前主动链。
	static bool IsCurrentCombatMontageNotifyForInterruptWindow(
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

	// 运行时窗口的 owner 必须回指到当前活跃主动 GA。
	// Begin/End 都靠这层 owner 解析配对，避免旧实例、旧蒙太奇或并行实例把不属于自己的窗口关掉。
	static UActionHeroGameplayAbility* ResolveActiveHeroAbilityOwnerForMontage(
		UHeroCombatComponent* HeroCombatComponent,
		UAnimMontage* RunningMontage)
	{
		if (!HeroCombatComponent || !RunningMontage)
		{
			return nullptr;
		}

		UActionAbilitySystemComponent* OwnerASC = HeroCombatComponent->GetOwningActionAbilitySystemComponent();
		if (!OwnerASC)
		{
			return nullptr;
		}

		for (const FGameplayAbilitySpec& ActiveSpec : OwnerASC->GetActivatableAbilities())
		{
			if (!ActiveSpec.IsActive())
			{
				continue;
			}

			if (UActionHeroGameplayAbility* PrimaryHeroAbility =
					Cast<UActionHeroGameplayAbility>(ActiveSpec.GetPrimaryInstance()))
			{
				if (PrimaryHeroAbility->OwnsHeroMontage(RunningMontage))
				{
					return PrimaryHeroAbility;
				}
			}

			for (UGameplayAbility* AbilityInstance : ActiveSpec.GetAbilityInstances())
			{
				UActionHeroGameplayAbility* HeroAbility = Cast<UActionHeroGameplayAbility>(AbilityInstance);
				if (HeroAbility && HeroAbility->OwnsHeroMontage(RunningMontage))
				{
					return HeroAbility;
				}
			}
		}

		return nullptr;
	}
}

UAnimNotifyState_AbilityInterruptWindow::UAnimNotifyState_AbilityInterruptWindow()
	: Super()
{
}

FString UAnimNotifyState_AbilityInterruptWindow::GetNotifyName_Implementation() const
{
	return TEXT("AbilityInterruptWindow");
}

void UAnimNotifyState_AbilityInterruptWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	CachedRuntimeByMesh.Remove(MeshComp);

	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		// 例外抢断窗口只应该作用在当前仍然有效的战斗蒙太奇上。
		// 这层过滤保证窗口只服务“当前活跃主动 GA 的这段显式例外帧”，不被旧通知串台。
		if (!IsCurrentCombatMontageNotifyForInterruptWindow(HeroCombatComponent, Animation))
		{
			return;
		}

		UAnimMontage* RunningMontage = HeroCombatComponent->GetCurrentRunningAnimMontage();
		UActionHeroGameplayAbility* OwnerHeroAbility =
			ResolveActiveHeroAbilityOwnerForMontage(HeroCombatComponent, RunningMontage);
		if (!OwnerHeroAbility)
		{
			return;
		}

		// 打开的不是“全局 interrupt-window”，而是“这条主动 GA 当前这段蒙太奇持有的例外窗口”。
		// 因此后面必须连同 owner spec / montage / serial 一起缓存下来，供 NotifyEnd 做 owner-aware close。
		FAbilityInterruptWindowRuntimeEntry RuntimeEntry;
		RuntimeEntry.OwnerSpecHandle = OwnerHeroAbility->GetCurrentHeroAbilitySpecHandle();
		RuntimeEntry.OwnerMontage = RunningMontage;
		RuntimeEntry.WindowSerial = HeroCombatComponent->OpenAbilityInterruptWindow(
			RuntimeEntry.OwnerSpecHandle,
			RuntimeEntry.OwnerMontage,
			ResolveAllowedInterruptCategories());
		RuntimeEntry.bOpenedWindow = RuntimeEntry.WindowSerial != 0;
		if (!RuntimeEntry.bOpenedWindow)
		{
			return;
		}

		CachedRuntimeByMesh.Add(MeshComp, RuntimeEntry);

		if (bEnableAttackInputOnBegin)
		{
			HeroCombatComponent->SetAttackEnabled(true);
		}

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

void UAnimNotifyState_AbilityInterruptWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const FAbilityInterruptWindowRuntimeEntry* RuntimeEntry = CachedRuntimeByMesh.Find(MeshComp);
	if (!RuntimeEntry)
	{
		return;
	}

	bool bClosedWindow = false;
	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		if (RuntimeEntry->bOpenedWindow)
		{
			// End 帧只允许真正的 owner 把窗口收掉。
			// 这样即便旧蒙太奇或旧通知晚到，也不会把当前主动链后来重新打开的新窗口误关掉。
			bClosedWindow = HeroCombatComponent->CloseAbilityInterruptWindowIfOwned(
				RuntimeEntry->OwnerSpecHandle,
				RuntimeEntry->OwnerMontage,
				RuntimeEntry->WindowSerial);
		}

		if (bDisableAttackInputOnEnd && bClosedWindow)
		{
			HeroCombatComponent->SetAttackEnabled(false);
		}
	}

	CachedRuntimeByMesh.Remove(MeshComp);
}

UHeroCombatComponent* UAnimNotifyState_AbilityInterruptWindow::ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const
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

UHeroCombatInputComponent* UAnimNotifyState_AbilityInterruptWindow::ResolveHeroCombatInputComponent(
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

TArray<EActionAbilityCategory> UAnimNotifyState_AbilityInterruptWindow::ResolveAllowedInterruptCategories() const
{
	return AllowedInterruptCategories;
}
