#include "Components/Combat/HeroExecutionCoordinatorComponent.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Combat/ActionHitResolver.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/Weapons/HeroWeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroExecutionCoordinatorComponent, Log, All);

namespace HeroExecutionDamageRuntime
{
	static bool TryGetCurrentWeaponAttributeCache(
		const UHeroCombatComponent* InCombatComponent,
		FActionWeaponAttributeCacheData& OutWeaponAttributeCache)
	{
		OutWeaponAttributeCache.Reset();
		const UHeroEquipmentComponent* EquipmentComponent =
			InCombatComponent ? InCombatComponent->GetOwningHeroEquipmentComponent() : nullptr;
		const UHeroLoadoutContextComponent* LoadoutContextComponent =
			InCombatComponent ? InCombatComponent->GetOwner()->FindComponentByClass<UHeroLoadoutContextComponent>() : nullptr;
		if (!EquipmentComponent || !LoadoutContextComponent)
		{
			return false;
		}

		return LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
			EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
			OutWeaponAttributeCache);
	}

	static void BuildCombatAttributeSnapshot(
		const UActionAttributeSetBase* InAttributeSet,
		const FActionWeaponAttributeCacheData* InWeaponAttributeCache,
		FActionCombatAttributeSnapshot& OutAttributeSnapshot)
	{
		OutAttributeSnapshot = FActionCombatAttributeSnapshot();

		const float AttackPowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->AttackPowerBonus : 0.f;
		const float DefensePowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->DefensePowerBonus : 0.f;
		const float MaxHealthBonus = InWeaponAttributeCache ? InWeaponAttributeCache->MaxHealthBonus : 0.f;
		const float CriticalChanceBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalChanceBonus : 0.f;
		const float CriticalDamageBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalDamageBonus : 0.f;
		const float OutgoingDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->OutgoingDamageMultiplierBonus : 0.f;
		const float ExtraDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->ExtraDamageMultiplierBonus : 0.f;

		OutAttributeSnapshot.AttackPower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetAttackPower() : 0.f) + AttackPowerBonus),
			0.f);
		OutAttributeSnapshot.DefensePower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetDefensePower() : 0.f) + DefensePowerBonus),
			0.f);
		OutAttributeSnapshot.MaxHealth = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetMaxHealth() : 0.f) + MaxHealthBonus),
			0.f);
		OutAttributeSnapshot.OutgoingDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetOutgoingDamageMultiplier() : 1.f) + OutgoingDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.ExtraDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetExtraDamageMultiplier() : 1.f) + ExtraDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.CriticalChance = ActionRoundRatioValueToFourPlaces(FMath::Clamp(
			(InAttributeSet ? InAttributeSet->GetCriticalChance() : 0.f) + CriticalChanceBonus,
			0.f,
			100.f));
		OutAttributeSnapshot.CriticalDamage = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetCriticalDamage() : 150.f) + CriticalDamageBonus),
			100.f);
	}
}

UHeroExecutionCoordinatorComponent::UHeroExecutionCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHeroExecutionCoordinatorComponent::CanExecuteTarget(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!GetOwner())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前执行者无效，无法查询处决目标。");
		}
		return false;
	}

	if (!IsTargetWithinExecutionActivationRange(InTargetActor, OutFailureReason))
	{
		return false;
	}

	if (const AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(InTargetActor))
	{
		if (const UAbilitySystemComponent* TargetASC = TargetCharacter->GetAbilitySystemComponent())
		{
			if (TargetASC->HasMatchingGameplayTag(ActionGameplayTags::State_Combat_SuperArmor_Active))
			{
				if (OutFailureReason)
				{
					*OutFailureReason = FString::Printf(
						TEXT("目标 %s 当前处于霸体状态，破韧期间也不能作为可处决目标。"),
						*GetNameSafe(InTargetActor));
				}
				return false;
			}
		}
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		if (!ExecutionWindowComponent->CanBeExecutedByWithReason(GetOwner(), OutFailureReason))
		{
			return false;
		}

		return HasValidExecutionConfigForTarget(InTargetActor, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
	}
	return false;
}

bool UHeroExecutionCoordinatorComponent::CanActivateExecutionAbility(
	AActor*& OutTargetActor,
	FString* OutFailureReason) const
{
	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (IsExecutionAbilityBlockedByCombatState(OutFailureReason))
	{
		return false;
	}

	if (!TryFindExecutionTarget(OutTargetActor, OutFailureReason))
	{
		return false;
	}

	// 这里先把“当前武器的处决数据 + 目标侧被处决演出数据”一起校验完整。
	// 这样可以把“有窗口但数据不完整”的情况提前挡在激活前，而不是等到预占甚至开播后才暴露。
	return HasValidExecutionConfigForTarget(OutTargetActor, OutFailureReason);
}

bool UHeroExecutionCoordinatorComponent::RevalidateExecutionAbilityAfterActivation(
	AActor*& OutTargetActor,
	FString* OutFailureReason) const
{
	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	// 这条入口只服务“处决已经正式激活之后”的运行时二次复核。
	// 这里仍然重新解析最新目标、范围、窗口和配置完整性，
	// 但不再重复经过输入门禁，否则处决刚挂上 ExecutionActive 时会把自己反向挡回去。
	return TryFindExecutionTarget(OutTargetActor, OutFailureReason);
}

namespace
{
	FString DescribeExecutionWeaponSubtypeForDebug(const FGameplayTag& WeaponSubtypeTag)
	{
		return WeaponSubtypeTag.IsValid() ? WeaponSubtypeTag.ToString() : TEXT("None");
	}
}

bool UHeroExecutionCoordinatorComponent::TryFindExecutionTarget(
	AActor*& OutTargetActor,
	FString* OutFailureReason) const
{
	OutTargetActor = nullptr;
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (const UHeroTargetingComponent* HeroTargetingComponent = GetOwningHeroTargetingComponent())
	{
		if (HeroTargetingComponent->IsTargetLockActive())
		{
			OutTargetActor = HeroTargetingComponent->GetLockedTargetActor();
			if (!IsValid(OutTargetActor))
			{
				if (OutFailureReason)
				{
					*OutFailureReason = TEXT("当前锁定目标无效，无法作为处决目标。");
				}
				return false;
			}

			return CanExecuteTarget(OutTargetActor, OutFailureReason);
		}
	}

	OutTargetActor = FindBestExecutionTarget(OutFailureReason);
	return IsValid(OutTargetActor);
}

bool UHeroExecutionCoordinatorComponent::TryReserveExecutionTarget(AActor* InTargetActor) const
{
	if (!GetOwner())
	{
		return false;
	}

	FVector SearchForward = FVector::ZeroVector;
	if (!ResolveExecutionSearchForward(InTargetActor, SearchForward, nullptr, false)
		|| !IsTargetWithinExecutionActivationRange(InTargetActor)
		|| !HasValidExecutionConfigForTarget(InTargetActor))
	{
		return false;
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		// 预占阶段只锁“这扇处决窗口当前归谁尝试使用”，
		// 不代表已经正式提交处决，也不代表处决伤害已经成立。
		// 执行者侧这里只是桥接目标窗口的正式入口，
		// 不自己平行维护一份“目标是否已被我预占”的业务状态。
		const bool bDidReserve = ExecutionWindowComponent->TryReserveExecutionWindow(GetOwner());
		if (bDidReserve)
		{
			CacheReservedExecutionSearchForward(InTargetActor, SearchForward);
			CancelExecutionTargetActiveCombatAbilities(InTargetActor);
		}

		return bDidReserve;
	}

	return false;
}

bool UHeroExecutionCoordinatorComponent::TryCommitExecutionTarget(AActor* InTargetActor) const
{
	if (!GetOwner())
	{
		return false;
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		// 提交阶段把“我准备处决这个目标”正式升级成执行权落地。
		// 只有提交成功，后续命中帧才允许真正把这次处决结算出去。
		// 这里仍然只是请求目标组件裁决，不在执行者侧自己改写窗口消费状态。
		const bool bDidCommit = ExecutionWindowComponent->TryCommitExecutionWindow(GetOwner());
		if (bDidCommit)
		{
			ClearReservedExecutionSearchForwardRuntime();
		}

		return bDidCommit;
	}

	return false;
}

void UHeroExecutionCoordinatorComponent::CancelReservedExecutionTarget(AActor* InTargetActor) const
{
	if (!GetOwner())
	{
		return;
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		ExecutionWindowComponent->CancelReservedExecutionWindow(GetOwner());
	}

	ClearReservedExecutionSearchForwardRuntime();
}

void UHeroExecutionCoordinatorComponent::ReleaseExecutionTargetVictimLock(AActor* InTargetActor) const
{
	if (!GetOwner())
	{
		return;
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		ExecutionWindowComponent->ReleaseExecutionVictimLock(GetOwner());
	}

	ClearReservedExecutionSearchForwardRuntime();
}

void UHeroExecutionCoordinatorComponent::AbortConsumedExecutionTargetPresentation(AActor* InTargetActor) const
{
	if (!GetOwner())
	{
		return;
	}

	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		ExecutionWindowComponent->AbortConsumedExecutionPresentation(GetOwner());
	}

	ClearReservedExecutionSearchForwardRuntime();
}

bool UHeroExecutionCoordinatorComponent::TryExecuteReservedExecutionTarget(
	AActor* InTargetActor,
	FActionHitResolveResult& OutResolveResult) const
{
	OutResolveResult = FActionHitResolveResult();

	// 命中帧真正执行处决前，再次构造一次正式伤害载荷。
	// 这一步不能复用前面激活期的临时校验结果，因为目标、武器上下文和处决倍率都可能已经变化。
	FActionDamagePayload ExecutionDamagePayload;
	if (!BuildExecutionDamagePayload(InTargetActor, ExecutionDamagePayload))
	{
		return false;
	}

	TWeakObjectPtr<UExecutionWindowComponent> ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor);
	if (!ExecutionWindowComponent.IsValid())
	{
		return false;
	}

	if (!HasValidReservedExecutionOwnership(InTargetActor))
	{
		// 命中帧前必须再次确认三件事仍然同时成立：
		// 1. 窗口还开着；
		// 2. 当前执行者仍是预占者；
		// 3. 受害者锁仍然归当前执行者。
		// 少任何一个，都说明这次处决资格已经在演出途中失效了。
		return false;
	}

	OutResolveResult = UActionHitResolver::ResolveHit(InTargetActor, ExecutionDamagePayload);
	if (!OutResolveResult.WasResolved() || OutResolveResult.AppliedDamage <= 0.f)
	{
		return false;
	}

	if (!ExecutionWindowComponent->TryCommitExecutionWindow(GetOwner()))
	{
		// 正式命中已经成功时，不允许执行者侧在这里直接抢跑目标侧收尾。
		// 提交失败统一按异常路径中止当前双边演出，让目标侧走 abort 语义关闭窗口并恢复 Poise。
		ExecutionWindowComponent->AbortConsumedExecutionPresentation(GetOwner());
		return false;
	}

	OutResolveResult.AppliedHitReactType = EActionHitReactType::None;

	return true;
}

UAnimMontage* UHeroExecutionCoordinatorComponent::GetExecutionMontageForCurrentWeapon() const
{
	if (const FActionExecutionConfig* ExecutionConfig = GetCurrentWeaponExecutionConfig())
	{
		return ExecutionConfig->GetExecutionMontage();
	}

	return nullptr;
}

float UHeroExecutionCoordinatorComponent::GetExecutionVictimTurnDurationForCurrentWeapon() const
{
	if (const FActionExecutionConfig* ExecutionConfig = GetCurrentWeaponExecutionConfig())
	{
		return FMath::Max(ExecutionConfig->VictimTurnDurationSeconds, 0.f);
	}

	return 0.f;
}

float UHeroExecutionCoordinatorComponent::GetExecutionStartDistanceForCurrentWeapon() const
{
	if (const FActionExecutionConfig* ExecutionConfig = GetCurrentWeaponExecutionConfig())
	{
		return FMath::Max(ExecutionConfig->ExecutionStartDistance, 0.f);
	}

	return 0.f;
}

bool UHeroExecutionCoordinatorComponent::CanStartReservedExecutionPresentation(
	AActor* InTargetActor,
	FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	return HasValidReservedExecutionOwnership(InTargetActor, OutFailureReason)
		&& IsTargetWithinExecutionActivationRange(InTargetActor, OutFailureReason)
		&& HasValidExecutionConfigForTarget(InTargetActor, OutFailureReason);
}

bool UHeroExecutionCoordinatorComponent::TryBeginExecutionTargetPreparation(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (!CanStartReservedExecutionPresentation(InTargetActor, OutFailureReason))
	{
		return false;
	}

	// 执行者侧协调器只负责把“当前武器小类 + 当前执行者 + 当前目标”
	// 这一组上下文转交给目标组件，让目标自己启动准备链。
	// 目标是否已经准备完成、什么时候真正开播，都继续由目标组件权威维护。
	UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor);
	const FGameplayTag WeaponSubtypeTag = GetCurrentWeaponSubtypeTag();
	if (!ExecutionWindowComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
		}
		return false;
	}

	if (!WeaponSubtypeTag.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前武器 WeaponSubtypeTag 无效，无法启动处决前准备。");
		}
		return false;
	}

	return ExecutionWindowComponent->TryBeginExecutionPresentationPreparation(
		GetOwner(),
		WeaponSubtypeTag,
		GetExecutionVictimTurnDurationForCurrentWeapon(),
		OutFailureReason);
}

bool UHeroExecutionCoordinatorComponent::IsExecutionTargetPreparationReady(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (!CanStartReservedExecutionPresentation(InTargetActor, OutFailureReason))
	{
		return false;
	}

	if (const UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		return ExecutionWindowComponent->IsExecutionPresentationPreparationReady(GetOwner(), OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
	}
	return false;
}

bool UHeroExecutionCoordinatorComponent::TryStartExecutionTargetPresentation(AActor* InTargetActor, FString* OutFailureReason) const
{
	if (!CanStartReservedExecutionPresentation(InTargetActor, OutFailureReason))
	{
		return false;
	}

	// 到这里说明执行者侧资格、距离和预占归属都还成立。
	// 具体“目标是否已经准备好正式开播”仍然交给目标组件自己的准备运行态判断。
	if (UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		return ExecutionWindowComponent->TryStartPreparedExecutionPresentation(GetOwner(), OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
	}
	return false;
}

bool UHeroExecutionCoordinatorComponent::DescribeExecutionTargetPreparationState(
	AActor* InTargetActor,
	FString& OutDescription,
	FString* OutFailureReason) const
{
	OutDescription.Reset();
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsValid(InTargetActor))
	{
		OutDescription = TEXT("处决准备状态：当前处决目标无效。");
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前处决目标无效。");
		}
		return false;
	}

	const UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor);
	if (!ExecutionWindowComponent)
	{
		OutDescription = FString::Printf(TEXT("处决准备状态：Target=%s，ExecutionWindowComponent 无效。"), *GetNameSafe(InTargetActor));
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
		}
		return false;
	}

	FString TargetPreparationDescription;
	FString TargetPreparationFailureReason;
	const bool bHasTargetPreparationState = ExecutionWindowComponent->DescribeExecutionPresentationPreparationState(
		GetOwner(),
		TargetPreparationDescription,
		&TargetPreparationFailureReason);
	const FGameplayTag WeaponSubtypeTag = GetCurrentWeaponSubtypeTag();
	const bool bReservedByOwner = GetOwner() && ExecutionWindowComponent->IsExecutionWindowReservedBy(GetOwner());
	const bool bVictimLockedByOwner = GetOwner() && ExecutionWindowComponent->IsExecutionVictimLockedBy(GetOwner());
	// 这里把执行者侧已知状态和目标组件自己的准备摘要拼成一条统一描述，
	// 方便 GA 轮询时不用分别查“目标窗口归属”和“目标准备链阶段”两套来源。
	OutDescription = FString::Printf(
		TEXT("处决准备状态：Target=%s WeaponSubtype=%s Reserved=%s VictimLock=%s %s"),
		*GetNameSafe(InTargetActor),
		*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag),
		bReservedByOwner ? TEXT("是") : TEXT("否"),
		bVictimLockedByOwner ? TEXT("是") : TEXT("否"),
		TargetPreparationDescription.IsEmpty() ? TEXT("PreparationState=Unavailable") : *TargetPreparationDescription);

	if (OutFailureReason && !TargetPreparationFailureReason.IsEmpty())
	{
		*OutFailureReason = TargetPreparationFailureReason;
	}

	return bHasTargetPreparationState;
}

bool UHeroExecutionCoordinatorComponent::HandleExecutionLogic(UActionAbilitySystemComponent* InActionASC, FGameplayTag InputTag)
{
	(void)InputTag;

	if (!InActionASC)
	{
		return false;
	}

	return TryCommitExecutionAbilityInput(InActionASC);
}

bool UHeroExecutionCoordinatorComponent::TryCommitExecutionAbilityInput(UActionAbilitySystemComponent* InActionASC)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !InActionASC)
	{
		return false;
	}

	AActor* ExecutionTargetActor = nullptr;
	FString FailureReason;
	if (!CanActivateExecutionAbility(ExecutionTargetActor, &FailureReason))
	{
		const FString ResolvedFailureReason =
			FailureReason.IsEmpty() ? TEXT("当前没有可处决目标，或目标不满足处决条件。") : FailureReason;
		Debug::Print(
			FString::Printf(TEXT("[Execution] 输入拒绝: %s"), *ResolvedFailureReason),
			FColor::Red,
			2.0f);
		UE_LOG(LogHeroExecutionCoordinatorComponent, Warning, TEXT("处决输入被拒绝：%s"), *ResolvedFailureReason);
		return false;
	}

	// 处决真正交给 GAS 前，必须先把“当前有合法目标且当前武器具备处决数据”确认完整。
	// 否则 ASC 虽然可能激活了 GA，但后续演出链会在更晚阶段才暴露出目标或数据缺失。
	const bool bExecutionTriggered =
		InActionASC->OnAbilityInputPressed(ActionGameplayTags::InputTag_GameplayAbility_Execution);
	if (!bExecutionTriggered)
	{
		return false;
	}

	CombatInputComponent->ClearBufferedInputIfMatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Execution);
	// 只有 ASC 明确接受了这次处决输入，才把这次输入记为已消费。
	CombatInputComponent->MarkInputConsumedByTag(ActionGameplayTags::InputTag_GameplayAbility_Execution);
	return true;
}

void UHeroExecutionCoordinatorComponent::RequestRecoverCombatInputAfterExecution()
{
	if (bDeferredCombatInputRecoveryAfterExecutionRequested || !GetWorld())
	{
		return;
	}

	// 处决尾帧里，执行者无敌、VictimLock、窗口状态与 GA 退场可能仍在同帧收尾。
	// 因此输入恢复固定延后一帧，避免处决刚结束就被旧状态再次拦回去。
	bDeferredCombatInputRecoveryAfterExecutionRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredCombatInputRecoveryAfterExecution);
}

void UHeroExecutionCoordinatorComponent::HandleDeferredCombatInputRecoveryAfterExecution()
{
	if (!bDeferredCombatInputRecoveryAfterExecutionRequested)
	{
		return;
	}

	bDeferredCombatInputRecoveryAfterExecutionRequested = false;

	if (UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent())
	{
		// 处决结束后的输入恢复，当前统一复用“受击恢复链”的总恢复入口：
		// 先尝试消费缓冲输入，未命中再回放仍然有效的 Held 输入。
		CombatInputComponent->RecoverCombatInputAfterCombatReact();
	}
}

void UHeroExecutionCoordinatorComponent::ResetRuntimeStateForHeroStartup()
{
	bDeferredCombatInputRecoveryAfterExecutionRequested = false;
	ClearReservedExecutionSearchForwardRuntime();
}

UHeroCombatComponent* UHeroExecutionCoordinatorComponent::GetOwningHeroCombatComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->GetHeroCombatComponent();
	}

	return nullptr;
}

UHeroCombatInputComponent* UHeroExecutionCoordinatorComponent::GetOwningHeroCombatInputComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->GetHeroCombatInputComponent();
	}

	return nullptr;
}

UHeroLoadoutContextComponent* UHeroExecutionCoordinatorComponent::GetOwningHeroLoadoutContextComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>();
	}

	return nullptr;
}

UHeroTargetingComponent* UHeroExecutionCoordinatorComponent::GetOwningHeroTargetingComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->GetHeroTargetingComponent();
	}

	return nullptr;
}

AActionHeroCharacter* UHeroExecutionCoordinatorComponent::GetOwningHeroCharacter() const
{
	return Cast<AActionHeroCharacter>(GetOwner());
}

UExecutionWindowComponent* UHeroExecutionCoordinatorComponent::GetExecutionWindowComponentFromTargetActor(AActor* InTargetActor) const
{
	if (AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(InTargetActor))
	{
		return TargetCharacter->GetExecutionWindowComponent();
	}

	return nullptr;
}

bool UHeroExecutionCoordinatorComponent::IsExecutionAbilityBlockedByCombatState(FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("hero combat component is invalid");
		}
		return true;
	}

	if (!CombatComponent->CanProcessCombatInput())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("处决输入被拒绝：当前武器启动流程尚未就绪。");
		}
		return true;
	}

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("处决输入被拒绝：当前执行者角色无效。");
		}
		return true;
	}

	if (const UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement();
		MovementComponent && MovementComponent->IsFalling())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("处决输入被拒绝：当前执行者处于空中 Falling 状态。");
		}
		return true;
	}

	if (const UActionCombatReactComponent* CombatReactComponent = OwnerHeroCharacter->GetActionCombatReactComponent())
	{
		if (CombatReactComponent->IsPrimaryReactPhaseActive())
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FString::Printf(
					TEXT("处决输入被拒绝：当前仍处于主受击阶段。事件=%s 阶段=%d。"),
					*CombatReactComponent->GetCurrentReactEventTag().ToString(),
					static_cast<int32>(CombatReactComponent->GetCurrentReactPhase()));
			}
			return true;
		}

		if (CombatReactComponent->IsRecoveryPhaseActive()
			&& !CombatReactComponent->CanActivateAbilityDuringRecoveryCancelWindow(
				ActionGameplayTags::InputTag_GameplayAbility_Execution))
		{
			if (OutFailureReason)
			{
				*OutFailureReason = TEXT("处决输入被拒绝：当前处于恢复阶段，但恢复取消窗口未放行处决。");
			}
			return true;
		}
	}

	if (const UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent->GetOwningHeroWeaponSwitchComponent())
	{
		if (WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress()
			|| (WeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive()
				&& !CombatComponent->IsSpecialWeaponSwitchPresentationInterruptInputAllowed(
					ActionGameplayTags::InputTag_GameplayAbility_Execution)))
		{
			if (OutFailureReason)
			{
				*OutFailureReason =
					CombatComponent->DescribeNonAttackInputGateForDebug(ActionGameplayTags::InputTag_GameplayAbility_Execution);
			}
			return true;
		}
	}

	return false;
}

bool UHeroExecutionCoordinatorComponent::IsTargetWithinExecutionActivationRange(
	AActor* InTargetActor,
	FString* OutFailureReason) const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !IsValid(InTargetActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = !OwnerHeroCharacter
				? TEXT("当前执行者角色无效，无法计算处决范围。")
				: TEXT("当前没有有效的处决目标可供计算范围。");
		}
		return false;
	}

	const FGameplayTag WeaponSubtypeTag = GetCurrentWeaponSubtypeTag();
	FVector SearchForward = FVector::ZeroVector;
	if (!ResolveExecutionSearchForward(InTargetActor, SearchForward, OutFailureReason))
	{
		return false;
	}

	const FVector SearchStart =
		OwnerHeroCharacter->GetActorLocation() + FVector(0.f, 0.f, ExecutionSearchHeightOffset);
	FVector ToTarget = InTargetActor->GetActorLocation() - SearchStart;
	ToTarget.Z = 0.f;
	const float DistanceSquared = ToTarget.SizeSquared();
	if (DistanceSquared <= KINDA_SMALL_NUMBER)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 与处决搜索原点过近，无法建立稳定的处决方向。WeaponSubtype=%s"),
				*GetNameSafe(InTargetActor),
				*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag));
		}
		return false;
	}

	const FVector DirectionToTarget = ToTarget.GetSafeNormal();
	const float MinFacingDot =
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ExecutionMaxTargetAngleDegrees, 0.f, 180.f)));
	const float FacingDot = FVector::DotProduct(SearchForward, DirectionToTarget);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FacingDot, -1.f, 1.f)));
	if (FacingDot < MinFacingDot)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 超出处决角度。WeaponSubtype=%s Angle=%.1f/%.1f"),
				*GetNameSafe(InTargetActor),
				*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag),
				AngleDegrees,
				ExecutionMaxTargetAngleDegrees);
		}
		return false;
	}

	const float MaxSearchDistance = FMath::Max(ExecutionSearchDistance, 0.f);
	const float SearchRadius = FMath::Max(ExecutionSearchRadius, 0.f);
	const float DistanceAlongForward = FVector::DotProduct(ToTarget, SearchForward);
	if (DistanceAlongForward < 0.f || DistanceAlongForward > MaxSearchDistance + SearchRadius)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 超出处决前向距离。WeaponSubtype=%s Forward=%.1f Allowed=0~%.1f"),
				*GetNameSafe(InTargetActor),
				*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag),
				DistanceAlongForward,
				MaxSearchDistance + SearchRadius);
		}
		return false;
	}

	const FVector ClosestPointOnSearchLine =
		SearchForward * FMath::Clamp(DistanceAlongForward, 0.f, MaxSearchDistance);
	const FVector LateralOffset = ToTarget - ClosestPointOnSearchLine;
	const float LateralDistance = LateralOffset.Size();
	if (LateralDistance > SearchRadius)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 超出处决横向范围。WeaponSubtype=%s Forward=%.1f/%.1f Lateral=%.1f/%.1f Angle=%.1f/%.1f"),
				*GetNameSafe(InTargetActor),
				*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag),
				DistanceAlongForward,
				MaxSearchDistance,
				LateralDistance,
				SearchRadius,
				AngleDegrees,
				ExecutionMaxTargetAngleDegrees);
		}
		return false;
	}

	return true;
}

bool UHeroExecutionCoordinatorComponent::HasValidExecutionConfigForTarget(
	AActor* InTargetActor,
	FString* OutFailureReason) const
{
	if (!IsValid(InTargetActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前没有有效的处决目标，无法校验处决配置。");
		}
		return false;
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition =
		CombatComponent ? CombatComponent->GetCurrentWeaponDefinition() : nullptr;
	if (!CurrentWeaponDefinition)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前武器定义无效，无法查询处决配置。");
		}
		return false;
	}

	const FGameplayTag WeaponSubtypeTag = GetCurrentWeaponSubtypeTag();
	if (!WeaponSubtypeTag.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("当前武器 %s 的 WeaponSubtypeTag 无效，无法匹配处决配置。"),
				*GetNameSafe(CurrentWeaponDefinition));
		}
		return false;
	}

	const FActionExecutionConfig* ExecutionConfig = GetCurrentWeaponExecutionConfig();
	if (!ExecutionConfig)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("当前武器 %s 的 ExecutionConfig 无效。"),
				*GetNameSafe(CurrentWeaponDefinition));
		}
		return false;
	}

	if (!ExecutionConfig->GetExecutionMontage())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("当前武器 %s 缺少 ExecutionConfig.ExecutionMontage。WeaponSubtype=%s"),
				*GetNameSafe(CurrentWeaponDefinition),
				*DescribeExecutionWeaponSubtypeForDebug(WeaponSubtypeTag));
		}
		return false;
	}

	// 这里继续保留一份“只做校验、不做真正结算”的伤害载荷检查，
	// 确认当前武器的处决 HitConfig 仍然能解析成完整处决伤害数据。
	FActionDamagePayload ValidationPayload;
	if (!BuildExecutionDamagePayload(InTargetActor, ValidationPayload, OutFailureReason))
	{
		return false;
	}

	if (const UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor))
	{
		return ExecutionWindowComponent->CanPrepareExecutionPresentationBy(GetOwner(), WeaponSubtypeTag, OutFailureReason);
	}

	if (OutFailureReason)
	{
		*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
	}
	return false;
}

bool UHeroExecutionCoordinatorComponent::HasValidReservedExecutionOwnership(
	AActor* InTargetActor,
	FString* OutFailureReason) const
{
	if (!GetOwner())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前执行者无效，无法校验处决预占归属。");
		}
		return false;
	}

	const UExecutionWindowComponent* ExecutionWindowComponent = GetExecutionWindowComponentFromTargetActor(InTargetActor);
	if (!ExecutionWindowComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 没有 ExecutionWindowComponent。"), *GetNameSafe(InTargetActor));
		}
		return false;
	}

	if (!ExecutionWindowComponent->IsExecutionWindowOpen())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("目标 %s 的处决窗口当前已关闭。"), *GetNameSafe(InTargetActor));
		}
		return false;
	}

	if (!ExecutionWindowComponent->IsExecutionWindowReservedBy(GetOwner()))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的处决窗口当前并未由当前执行者预占。"),
				*GetNameSafe(InTargetActor));
		}
		return false;
	}

	if (!ExecutionWindowComponent->IsExecutionVictimLockedBy(GetOwner()))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(
				TEXT("目标 %s 的处决受害者锁当前不归当前执行者所有。"),
				*GetNameSafe(InTargetActor));
		}
		return false;
	}

	return true;
}

const FActionExecutionConfig* UHeroExecutionCoordinatorComponent::GetCurrentWeaponExecutionConfig() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			return &CurrentWeaponDefinition->GetExecutionConfig();
		}
	}

	return nullptr;
}

FGameplayTag UHeroExecutionCoordinatorComponent::GetCurrentWeaponSubtypeTag() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
		{
			return CurrentWeaponDefinition->GetWeaponSubtypeTag();
		}
	}

	return FGameplayTag();
}

bool UHeroExecutionCoordinatorComponent::ResolveExecutionSearchForward(
	AActor* InTargetActor,
	FVector& OutSearchForward,
	FString* OutFailureReason,
	const bool bPreferReservedSearchForward) const
{
	OutSearchForward = FVector::ZeroVector;
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前执行者角色无效，无法计算处决范围。");
		}
		return false;
	}

	if (bPreferReservedSearchForward
		&& bHasReservedExecutionSearchForward
		&& (!InTargetActor || ReservedExecutionSearchForwardTarget.Get() == InTargetActor))
	{
		OutSearchForward = ReservedExecutionSearchForward.GetSafeNormal2D();
	}
	else
	{
		OutSearchForward = OwnerHeroCharacter->GetActorForwardVector().GetSafeNormal2D();
		if (const AActionPlayerController* OwnerController = Cast<AActionPlayerController>(OwnerHeroCharacter->GetController()))
		{
			const FRotator ControlYawRotation(0.f, OwnerController->GetControlRotation().Yaw, 0.f);
			OutSearchForward = ControlYawRotation.Vector().GetSafeNormal2D();
		}
	}

	if (OutSearchForward.IsNearlyZero())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前执行者朝向无效，无法计算处决范围。");
		}
		return false;
	}

	return true;
}

void UHeroExecutionCoordinatorComponent::CacheReservedExecutionSearchForward(
	AActor* InTargetActor,
	const FVector& InSearchForward) const
{
	ReservedExecutionSearchForwardTarget = InTargetActor;
	ReservedExecutionSearchForward = InSearchForward.GetSafeNormal2D();
	bHasReservedExecutionSearchForward = !ReservedExecutionSearchForward.IsNearlyZero();
}

void UHeroExecutionCoordinatorComponent::ClearReservedExecutionSearchForwardRuntime() const
{
	bHasReservedExecutionSearchForward = false;
	ReservedExecutionSearchForward = FVector::ZeroVector;
	ReservedExecutionSearchForwardTarget.Reset();
}

AActor* UHeroExecutionCoordinatorComponent::FindBestExecutionTarget(FString* OutFailureReason) const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!OwnerHeroCharacter || !CombatComponent || !GetWorld())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = !OwnerHeroCharacter
				? TEXT("当前执行者角色无效，无法搜索处决目标。")
				: (!CombatComponent
					? TEXT("hero combat component is invalid")
					: TEXT("当前世界无效，无法搜索处决目标。"));
		}
		return nullptr;
	}

	FVector SearchForward = FVector::ZeroVector;
	if (!ResolveExecutionSearchForward(nullptr, SearchForward, OutFailureReason, false))
	{
		return nullptr;
	}

	const FVector SearchStart =
		OwnerHeroCharacter->GetActorLocation() + FVector(0.f, 0.f, ExecutionSearchHeightOffset);
	const FVector SearchEnd =
		SearchStart + SearchForward * FMath::Max(ExecutionSearchDistance, 0.f);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ExecutionTargetSweep), false);
	QueryParams.AddIgnoredActor(OwnerHeroCharacter);

	TArray<FHitResult> HitResults;
	const bool bHasBlockingHit = GetWorld()->SweepMultiByObjectType(
		HitResults,
		SearchStart,
		SearchEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(ExecutionSearchRadius, 0.f)),
		QueryParams);
	if (!bHasBlockingHit)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("当前前方处决搜索范围内没有候选目标。");
		}
		return nullptr;
	}

	const float MinFacingDot =
		FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ExecutionMaxTargetAngleDegrees, 0.f, 180.f)));
	AActor* BestTargetActor = nullptr;
	float BestTargetScore = -FLT_MAX;
	float BestRejectedTargetScore = -FLT_MAX;
	FString BestRejectedReason;
	TSet<const AActor*> EvaluatedTargets;

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* CandidateTarget = HitResult.GetActor();
		if (!IsValid(CandidateTarget) || EvaluatedTargets.Contains(CandidateTarget))
		{
			continue;
		}

		EvaluatedTargets.Add(CandidateTarget);

		FVector ToTarget = CandidateTarget->GetActorLocation() - SearchStart;
		ToTarget.Z = 0.f;
		const float DistanceSquared = ToTarget.SizeSquared();
		const FVector DirectionToTarget = DistanceSquared > KINDA_SMALL_NUMBER ? ToTarget.GetSafeNormal() : SearchForward;
		const float FacingDot = FVector::DotProduct(SearchForward, DirectionToTarget);
		const float TargetScore = FacingDot * 100000.f - DistanceSquared;
		FString CandidateFailureReason;
		if (!IsValid(CandidateTarget) || !CanExecuteTarget(CandidateTarget, &CandidateFailureReason))
		{
			if (TargetScore > BestRejectedTargetScore)
			{
				BestRejectedTargetScore = TargetScore;
				BestRejectedReason = CandidateFailureReason.IsEmpty()
					? FString::Printf(TEXT("目标 %s 不满足当前处决条件。"), *GetNameSafe(CandidateTarget))
					: CandidateFailureReason;
			}
			continue;
		}

		if (DistanceSquared <= KINDA_SMALL_NUMBER || FacingDot < MinFacingDot)
		{
			continue;
		}

		if (TargetScore > BestTargetScore)
		{
			BestTargetScore = TargetScore;
			BestTargetActor = CandidateTarget;
		}
	}

	if (!BestTargetActor && OutFailureReason)
	{
		*OutFailureReason = BestRejectedReason.IsEmpty()
			? TEXT("当前前方处决搜索范围内没有满足资格的处决目标。")
			: BestRejectedReason;
	}

	return BestTargetActor;
}

bool UHeroExecutionCoordinatorComponent::BuildExecutionDamagePayload(
	AActor* InTargetActor,
	FActionDamagePayload& OutDamagePayload,
	FString* OutFailureReason) const
{
	OutDamagePayload = FActionDamagePayload();

	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !GetOwner() || !IsValid(InTargetActor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = !CombatComponent
				? TEXT("hero combat component is invalid")
				: (!GetOwner()
					? TEXT("当前执行者无效，无法构建处决伤害载荷。")
					: TEXT("当前处决目标无效，无法构建处决伤害载荷。"));
		}
		return false;
	}

	if (const AHeroWeaponBase* CurrentWeapon = CombatComponent->GetCurrentEquippedWeapon())
	{
		OutDamagePayload = CurrentWeapon->BuildExecutionDamagePayloadForTarget(InTargetActor);
	}
	else
	{
		const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition();
		const UActionAttributeSetBase* OwnerAttributeSet = CombatComponent->GetOwningActionAttributeSet();
		if (!CurrentWeaponDefinition || !OwnerAttributeSet)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = !CurrentWeaponDefinition
					? TEXT("当前武器定义无效，无法构建处决伤害载荷。")
					: TEXT("当前执行者属性集无效，无法构建处决伤害载荷。");
			}
			return false;
		}

		const FActionExecutionHitConfig& HitConfig = CurrentWeaponDefinition->GetExecutionHitConfig();
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		const bool bHasWeaponAttributeCache =
			HeroExecutionDamageRuntime::TryGetCurrentWeaponAttributeCache(CombatComponent, WeaponAttributeCache);
		FActionCombatAttributeSnapshot AttributeSnapshot;
		HeroExecutionDamageRuntime::BuildCombatAttributeSnapshot(
			OwnerAttributeSet,
			bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr,
			AttributeSnapshot);

		OutDamagePayload.InstigatorActor = GetOwner();
		OutDamagePayload.SourceActor = GetOwner();
		OutDamagePayload.DamageType = CurrentWeaponDefinition->GetDamageType();
		OutDamagePayload.DamageElementTypeTag = CurrentWeaponDefinition->GetDamageElementTypeTag();
		if (HitConfig.HasAnyLevelDrivenDamageConfig())
		{
			bool bDidCritical = false;
			OutDamagePayload.BaseDamage = ActionResolveDrivenDamageValue(
				HitConfig.HealthDamageValueConfig,
				AttributeSnapshot,
				1,
				true,
				bDidCritical);

			bool bIgnoredCritical = false;
			OutDamagePayload.PoiseDamage = ActionResolveDrivenDamageValue(
				HitConfig.PoiseDamageValueConfig,
				AttributeSnapshot,
				1,
				false,
				bIgnoredCritical);
		}
		HitConfig.ApplySharedExecutionPayloadSettings(OutDamagePayload);
		OutDamagePayload.ImpactDirection = (InTargetActor->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		const FName SourceId = ActionHitSourceDefaults::GetExecutionSourceId();
		if (!(CombatComponent
			&& CombatComponent->GetOwningHeroHitSourceComponent()
			&& CombatComponent->GetOwningHeroHitSourceComponent()->TryFillHitSourceInfoFromRegistration(SourceId, OutDamagePayload.HitSource)))
		{
			OutDamagePayload.HitSource.SourceId = SourceId;
			OutDamagePayload.HitSource.SourceType = EActionHitSourceType::Execution;
			OutDamagePayload.HitSource.SourceComponentName = TEXT("Execution");
		}

		OutDamagePayload.HitSource.SourceTag = CurrentWeaponDefinition->WeaponTag;
		OutDamagePayload.HitSource.WeaponTag = CurrentWeaponDefinition->WeaponTag;
	}

	OutDamagePayload.InstigatorActor = GetOwner();
	if (!OutDamagePayload.SourceActor)
	{
		OutDamagePayload.SourceActor = GetOwner();
	}

	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition())
	{
		OutDamagePayload.DamageType = CurrentWeaponDefinition->GetDamageType();
		OutDamagePayload.DamageElementTypeTag = CurrentWeaponDefinition->GetDamageElementTypeTag();
		OutDamagePayload.HitSource.WeaponTag = CurrentWeaponDefinition->WeaponTag;
		if (!OutDamagePayload.HitSource.SourceTag.IsValid())
		{
			OutDamagePayload.HitSource.SourceTag = CurrentWeaponDefinition->WeaponTag;
		}
	}
	else if (const UHeroEquipmentComponent* EquipmentComponent = CombatComponent->GetOwningHeroEquipmentComponent())
	{
		const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
		FActionWeaponAttributeCacheData WeaponAttributeCache;
		if (LoadoutContextComponent
			&& LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
				EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
				WeaponAttributeCache))
		{
			OutDamagePayload.DamageType = WeaponAttributeCache.DamageType;
			OutDamagePayload.DamageElementTypeTag = WeaponAttributeCache.DamageElementTypeTag;
		}
	}
	const FName SourceId = ActionHitSourceDefaults::GetExecutionSourceId();
	if (!(CombatComponent
		&& CombatComponent->GetOwningHeroHitSourceComponent()
		&& CombatComponent->GetOwningHeroHitSourceComponent()->TryFillHitSourceInfoFromRegistration(SourceId, OutDamagePayload.HitSource)))
	{
		OutDamagePayload.HitSource.SourceId = SourceId;
		OutDamagePayload.HitSource.SourceType = EActionHitSourceType::Execution;
		OutDamagePayload.HitSource.SourceComponentName = TEXT("Execution");
	}

	OutDamagePayload.HitSource.LoadoutSlot = CombatComponent->GetCurrentCombatLoadoutSlot();

	if (OutDamagePayload.BaseDamage <= 0.f
		&& OutDamagePayload.PoiseDamage <= 0.f)
	{
		OutDamagePayload.BaseDamage = 1.f;
	}

	if (!OutDamagePayload.IsValidPayload())
	{
		if (OutFailureReason)
		{
			const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = CombatComponent->GetCurrentWeaponDefinition();
			*OutFailureReason = FString::Printf(
				TEXT("当前武器 %s 的处决专用 HitConfig 无法构建合法处决伤害载荷。"),
				*GetNameSafe(CurrentWeaponDefinition));
		}
		return false;
	}

	return true;
}

float UHeroExecutionCoordinatorComponent::GetResolvedAttackPowerForCurrentLoadout() const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return 0.f;
	}

	const UActionAttributeSetBase* AttributeSet = CombatComponent->GetOwningActionAttributeSet();
	if (!AttributeSet)
	{
		return 0.f;
	}

	float ResolvedAttackPower = FMath::Max(ActionRoundBaseValueToInteger(AttributeSet->GetAttackPower()), 0.f);

	const UHeroEquipmentComponent* EquipmentComponent = CombatComponent->GetOwningHeroEquipmentComponent();
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	if (!EquipmentComponent || !LoadoutContextComponent)
	{
		return ResolvedAttackPower;
	}

	FActionWeaponAttributeCacheData WeaponAttributeCache;
	if (LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
		EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
		WeaponAttributeCache))
	{
		ResolvedAttackPower += ActionRoundBaseValueToInteger(WeaponAttributeCache.AttackPowerBonus);
	}

	return FMath::Max(ActionRoundBaseValueToInteger(ResolvedAttackPower), 0.f);
}

void UHeroExecutionCoordinatorComponent::CancelExecutionTargetActiveCombatAbilities(AActor* InTargetActor) const
{
	AActionCharacterBase* TargetCharacter = Cast<AActionCharacterBase>(InTargetActor);
	UActionAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetActionAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	TargetASC->CancelAbilitiesByAbilityTags(BuildExecutionTargetCancelableAbilityTags());
}

FGameplayTagContainer UHeroExecutionCoordinatorComponent::BuildExecutionTargetCancelableAbilityTags() const
{
	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_Attack);
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_Dodge);
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_CombatModeOrDefense);
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_WeaponSwitch);
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_SpiritSkill);
	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_ProjectileSwitch);
	return AbilityTags;
}
