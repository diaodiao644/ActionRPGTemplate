// 文件说明：实现英雄切武 Ability，负责复用统一的固定武器槽切换链与特殊切武表现逻辑。

#include "AbilitySystem/Abilities/Hero/HeroGA_WeaponSwitch.h"

#include "ActionGameplayTags.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/Weapons/HeroWeaponBase.h"

UHeroGA_WeaponSwitch::UHeroGA_WeaponSwitch()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	// 在原生构造函数里固定声明切武 AbilityTag，供后续冷却、优先级和打断规则复用。
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_WeaponSwitch);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_WeaponSwitch_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Dodge_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Execution_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_SpiritSkill_Active);

	// 切武 Ability 本身不单独做一份 GE 资产，直接复用通用冷却 GE。
	// 这里仅声明本 Ability 自己的冷却标签和默认时长。
	AbilityCooldownTags.AddTag(ActionGameplayTags::Cooldown_Ability_WeaponSwitch);
	AbilityCooldownDuration = 5.0f;

	// 切武允许在被取消窗口明确放行时抢占攻击 / 防御，
	// 但仍低于闪避和处决，不会打断更高优先级动作。
	AbilityPriority = 26;
	bCanInterruptLowerPriorityAbilities = true;
	bCanInterruptSamePriorityAbilities = false;
	bCanBeInterruptedByHigherPriority = true;
	bCanBeInterruptedBySamePriority = false;
	CancelAbilitiesWithTag.AddTag(ActionGameplayTags::Player_Ability_Attack);
	CancelAbilitiesWithTag.AddTag(ActionGameplayTags::Player_Ability_CombatModeOrDefense);
}

bool UHeroGA_WeaponSwitch::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		OutFailureReason = TEXT("hero combat component is invalid");
		return false;
	}

	UHeroWeaponSwitchComponent* HeroWeaponSwitchComponent = GetHeroWeaponSwitchComponentFromActorInfo();
	if (!HeroWeaponSwitchComponent)
	{
		OutFailureReason = TEXT("hero weapon switch component is invalid");
		return false;
	}

	if (!HeroWeaponSwitchComponent->CanActivateWeaponSwitchAbility(ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch))
	{
		// 切武的演出期、事务态、冷却和取消窗口白名单约束继续由既有战斗态门禁维护，
		// 这里只把它同步接进关系预检，保证失败语义统一回到 ASC 关系链。
		OutFailureReason =
			HeroCombatComponent->DescribeNonAttackInputGateForDebug(ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch);
		return false;
	}

	EHeroWeaponLoadoutSlot TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
	if (!HeroWeaponSwitchComponent->ResolveWeaponSwitchTargetLoadoutSlot(TargetLoadoutSlot, false))
	{
		OutFailureReason = TEXT("weapon switch target loadout slot cannot be resolved");
		return false;
	}

	return true;
}

void UHeroGA_WeaponSwitch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 这批字段都只描述“本次切武 Ability 自己走到了哪一步”。
	// 正式切武请求、事务态和表现期状态都在切武组件侧，
	// GA 本地只缓存“本次是否已进入表现壳、当前播的是哪条蒙太奇、收尾是否已执行过”这一层短生命周期状态。
	bAbilityFinished = false;
	bWeaponSwitchPresentationStarted = false;
	ActiveWeaponSwitchMontage = nullptr;

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroWeaponSwitchComponent* HeroWeaponSwitchComponent = GetHeroWeaponSwitchComponentFromActorInfo();
	if (!HeroCombatComponent || !HeroWeaponSwitchComponent)
	{
		Debug::Print(TEXT("[GA][WeaponSwitch] Begin Failed: CombatComponent 或 WeaponSwitchComponent 无效"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UHeroEquipmentComponent* HeroEquipmentComponent = HeroCombatComponent->GetOwningHeroEquipmentComponent();

	// 先用 Invalid 占位，只有在成功解析到明确目标槽位后才进入真正的切武流程。
	EHeroWeaponLoadoutSlot TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
	if (!HeroWeaponSwitchComponent->ResolveWeaponSwitchTargetLoadoutSlot(TargetLoadoutSlot, true))
	{
		Debug::Print(TEXT("[GA][WeaponSwitch] Begin Failed: 未解析到目标槽位"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][WeaponSwitch] Begin TargetSlot=%d"),
			static_cast<int32>(TargetLoadoutSlot)),
		FColor::Cyan,
		2.0f);

	// 先提交 Ability，让 GAS 统一执行冷却检查与冷却应用。
	// 只有提交成功后，才允许真正切武。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		Debug::Print(TEXT("[GA][WeaponSwitch] Begin Failed: CommitAbility 失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const bool bCanUseSpecialSwitch = HeroWeaponSwitchComponent->CanUseSpecialWeaponSwitchForLoadoutSlot(TargetLoadoutSlot);
	UAnimMontage* SpecialSwitchMontage = bCanUseSpecialSwitch
		? HeroWeaponSwitchComponent->GetSpecialWeaponSwitchMontageForLoadoutSlot(TargetLoadoutSlot)
		: nullptr;
	UAnimMontage* NormalSwitchMontage = HeroWeaponSwitchComponent->GetNormalWeaponSwitchMontageForLoadoutSlot(TargetLoadoutSlot);

	// 先做真实切槽，再决定要不要播普通/特殊切武表现。
	// 这样“武器已经真的切到哪一槽”与“是否有独立演出”被正式拆成两层，
	// 避免表现层反向驱动装备状态。
	// 先尝试切到目标槽位。
	// 注意：这里返回 true 既可能代表“已经同步切完”，也可能只是“异步加载 / 预热链已成功挂起”。
	if (!HeroCombatComponent->EquipWeaponByLoadoutSlot(TargetLoadoutSlot))
	{
		Debug::Print(TEXT("[GA][WeaponSwitch] Begin Failed: 装备目标槽位失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const bool bSwitchCompletedImmediately =
		HeroEquipmentComponent
		&& HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot() == TargetLoadoutSlot;

	if (!bSwitchCompletedImmediately)
	{
		if ((bCanUseSpecialSwitch && SpecialSwitchMontage) || NormalSwitchMontage)
		{
			// 普通与特殊切武表现都必须建立在“目标武器已经真实切到手上”这一前提下。
			// 如果本次切武只是进入异步加载链，就先回退为无独立演出，避免旧武器上提前播目标武器的切武动画。
			Debug::Print(TEXT("[GA][WeaponSwitch] Presentation Skipped: 本次切武进入异步链，已回退为无独立演出"), FColor::Yellow, 2.0f);
		}

		Debug::Print(
			FString::Printf(
				TEXT("[GA][WeaponSwitch] End NormalSwitch TargetSlot=%d"),
				static_cast<int32>(TargetLoadoutSlot)),
			FColor::Green,
			2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	bool bUseSpecialPresentation = false;
	UAnimMontage* PresentationMontage = nullptr;
	int32 PresentationReactGuardThreshold = 0;
	FName PresentationContextName = NAME_None;

	if (bCanUseSpecialSwitch && SpecialSwitchMontage)
	{
		if (!HeroCombatComponent->ConsumeSpecialWeaponSwitchEnergy())
		{
			// 提交 Ability 到真正播放表现之间可能存在极短的状态变化窗口。
			// 如果能量在这个窗口里被别的逻辑消耗掉，就降级为普通切武，而不是把已经完成的切武误报成失败。
			Debug::Print(TEXT("[GA][WeaponSwitch] Special Skipped: 能量状态已变化，已回退为普通切武"), FColor::Yellow, 2.0f);
		}
		else
		{
			// 特殊切武能量只在“确定本次真的要进入特殊表现”这一刻消费。
			// 如果提交 Ability 后到真正开始表现前资格变化，就直接降级普通切武，
			// 不把已经成功的真实切武误判成整次切武失败。
			bUseSpecialPresentation = true;
			PresentationMontage = SpecialSwitchMontage;
			PresentationReactGuardThreshold = HeroCombatComponent->GetCurrentWeaponDefinition()
				? HeroCombatComponent->GetCurrentWeaponDefinition()->GetSpecialWeaponSwitchReactGuardThreshold()
				: 0;
			PresentationContextName = TEXT("SpecialWeaponSwitch");
			HeroCombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_WeaponSwitch_Special);
		}
	}

	if (!bUseSpecialPresentation && NormalSwitchMontage)
	{
		PresentationMontage = NormalSwitchMontage;
		PresentationReactGuardThreshold = HeroCombatComponent->GetCurrentWeaponDefinition()
			? HeroCombatComponent->GetCurrentWeaponDefinition()->GetNormalWeaponSwitchReactGuardThreshold()
			: 0;
		PresentationContextName = TEXT("NormalWeaponSwitch");
	}

	if (!PresentationMontage)
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][WeaponSwitch] End NormalSwitch TargetSlot=%d"),
				static_cast<int32>(TargetLoadoutSlot)),
			FColor::Green,
			2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (UHeroAttackComponent* HeroAttackComponent = HeroCombatComponent->GetOwningHeroAttackComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = HeroCombatComponent->GetCurrentWeaponDefinition())
		{
			HeroAttackComponent->SetCurrentAttackHitConfig(
				bUseSpecialPresentation
					? CurrentWeaponDefinition->GetSpecialWeaponSwitchHitConfig()
					: CurrentWeaponDefinition->GetNormalWeaponSwitchHitConfig());
		}
		else
		{
			HeroAttackComponent->ClearCurrentAttackHitConfig();
		}
	}

	// 到这里说明真实切武已经完成，后续这段 GA 只剩“切武表现壳”职责：
	// 1. 设置当前表现蒙太奇和受击保护阈值；
	// 2. 开启表现期门禁与移动锁；
	// 3. 在蒙太奇结束后只收演出壳，不回滚已经完成的武器切换事务。
	HeroCombatComponent->UpdateRunningAnimMontage(PresentationMontage);
	HeroCombatComponent->SetRunningAnimationReactGuardContext(
		PresentationMontage,
		EActionRunningAnimationSemantic::NonReact,
		PresentationReactGuardThreshold);

	// 从这里开始，逻辑层的切武事务与表现层的切武动画分离。
	// 即使武器状态同步和切武事务已经完成，只要表现期还在播放，依然由 Ability 负责收尾。
	TryApplyWeaponSwitchPresentationFacing();
	HeroWeaponSwitchComponent->BeginWeaponSwitchPresentation(bUseSpecialPresentation);
	bWeaponSwitchPresentationStarted = true;
	ActiveWeaponSwitchMontage = PresentationMontage;
	ApplyWeaponSwitchPresentationMoveLock(true);
	const FString BeginLogMessage = bUseSpecialPresentation
		? FString::Printf(TEXT("[GA][WeaponSwitch] Special Begin TargetSlot=%d"), static_cast<int32>(TargetLoadoutSlot))
		: FString::Printf(TEXT("[GA][WeaponSwitch] Normal Begin TargetSlot=%d"), static_cast<int32>(TargetLoadoutSlot));

	Debug::Print(
		BeginLogMessage,
		FColor::Cyan,
		2.0f);

	if (!PlayHeroMontage(
		PresentationMontage,
		bUseSpecialPresentation ? TEXT("PlaySpecialWeaponSwitchMontage") : TEXT("PlayNormalWeaponSwitchMontage"),
		PresentationContextName))
	{
		Debug::Print(
			bUseSpecialPresentation
				? TEXT("[GA][WeaponSwitch] Special Failed: 蒙太奇播放失败")
				: TEXT("[GA][WeaponSwitch] Normal Failed: 蒙太奇播放失败"),
			FColor::Red,
			2.0f);
		FinishWeaponSwitchAbility(true);
	}
}

void UHeroGA_WeaponSwitch::OnHeroMontageCompleted(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][WeaponSwitch] Montage Completed"), FColor::Yellow, 2.0f);
	FinishWeaponSwitchAbility(false);
}

void UHeroGA_WeaponSwitch::OnHeroMontageBlendOut(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][WeaponSwitch] Montage BlendOut"), FColor::Yellow, 2.0f);
	FinishWeaponSwitchAbility(false);
}

void UHeroGA_WeaponSwitch::OnHeroMontageInterrupted(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][WeaponSwitch] Montage Interrupted"), FColor::Red, 2.0f);
	FinishWeaponSwitchAbility(true);
}

void UHeroGA_WeaponSwitch::OnHeroMontageCancelled(FName MontageContext)
{
	(void)MontageContext;
	Debug::Print(TEXT("[GA][WeaponSwitch] Montage Cancelled"), FColor::Red, 2.0f);
	FinishWeaponSwitchAbility(true);
}

void UHeroGA_WeaponSwitch::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bAbilityFinished)
	{
		FinalizeWeaponSwitchAbilityRuntime(bWasCancelled);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_WeaponSwitch::TryApplyWeaponSwitchPresentationFacing()
{
	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	UHeroTargetingComponent* HeroTargetingComponent =
		HeroCharacter ? HeroCharacter->GetHeroTargetingComponent() : nullptr;
	if (!HeroCharacter || !HeroTargetingComponent)
	{
		return;
	}

	const bool bFacingApplied =
		HeroTargetingComponent->TryApplyOffensiveFacing(EActionSimpleTurnAssistTriggerSource::Attack);
	Debug::Print(
		FString::Printf(
			TEXT("[GA][WeaponSwitch] Facing %s"),
			bFacingApplied ? TEXT("Applied") : TEXT("Skipped")),
		bFacingApplied ? FColor::Yellow : FColor::Silver,
		1.5f);
}

void UHeroGA_WeaponSwitch::ApplyWeaponSwitchPresentationMoveLock(const bool bLocked)
{
	// 这层锁只服务普通/特殊切武表现期。
	// 真正“当前是否仍有切武事务未完成”继续只认 HeroWeaponSwitchComponent 的事务态。
	AActionHeroCharacter* OwnerHeroCharacter = GetHeroCharacterFromActorInfo();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	if (AController* OwnerController = OwnerHeroCharacter->GetController())
	{
		OwnerController->SetIgnoreMoveInput(bLocked);
	}

	if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

void UHeroGA_WeaponSwitch::FinalizeWeaponSwitchAbilityRuntime(bool bWasCancelled)
{
	if (bAbilityFinished)
	{
		return;
	}

	bAbilityFinished = true;

	if (bWeaponSwitchPresentationStarted)
	{
		if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
		{
			HeroCombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
				ActiveWeaponSwitchMontage,
				EActionRunningAnimationSemantic::NonReact);
			HeroCombatComponent->ClearRunningAnimMontageReferenceIfMatches(
				ActiveWeaponSwitchMontage);
			HeroCombatComponent->CloseAbilityChainWindow();
			HeroCombatComponent->CloseAbilityCancelWindow();
			HeroCombatComponent->SetAttackEnabled(true);

			if (AHeroWeaponBase* CurrentWeapon = HeroCombatComponent->GetCurrentEquippedWeapon())
			{
				CurrentWeapon->EndAttackDetection();
			}

			if (UHeroHitSourceComponent* HitSourceComponent = HeroCombatComponent->GetOwningHeroHitSourceComponent())
			{
				HitSourceComponent->ResetHitWindowRuntime();
			}

			if (UHeroAttackComponent* HeroAttackComponent = HeroCombatComponent->GetOwningHeroAttackComponent())
			{
				HeroAttackComponent->ClearCurrentAttackHitConfig();
			}
		}

		if (UHeroWeaponSwitchComponent* HeroWeaponSwitchComponent = GetHeroWeaponSwitchComponentFromActorInfo())
		{
			// 切武 Ability 的职责只覆盖“表现期”这一段。
			// 这里结束的是表现层状态，不会回滚前面已经完成的切武事务与装备状态。
			HeroWeaponSwitchComponent->EndWeaponSwitchPresentation();
		}

		ApplyWeaponSwitchPresentationMoveLock(false);
		bWeaponSwitchPresentationStarted = false;
		ActiveWeaponSwitchMontage = nullptr;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][WeaponSwitch] End Cancelled=%d"),
			bWasCancelled ? 1 : 0),
		FColor::Green,
		2.0f);
}

void UHeroGA_WeaponSwitch::FinishWeaponSwitchAbility(bool bWasCancelled)
{
	if (bAbilityFinished)
	{
		return;
	}

	FinalizeWeaponSwitchAbilityRuntime(bWasCancelled);
	K2_EndAbility();
}
