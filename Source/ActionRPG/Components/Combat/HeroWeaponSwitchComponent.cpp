#include "Components/Combat/HeroWeaponSwitchComponent.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "ActionGameplayTags.h"
#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "Debug/ActionDebugHelper.h"
#include "Engine/World.h"
#include "Items/Weapons/HeroWeaponBase.h"

UHeroWeaponSwitchComponent::UHeroWeaponSwitchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
void UHeroWeaponSwitchComponent::BeginPlay()
{
	Super::BeginPlay();

	// 切武组件本身不拥有装备状态，它只消费装备组件给出的“当前已装备结果”。
	// 因此一进入运行时就先绑定装备侧广播，并立刻拉一次当前快照，
	// 避免角色出生后第一帧里战斗侧还拿着空武器上下文。
	BindEquipmentStateDelegates();
	InitializeCurrentWeaponStateFromEquipment();
}

void UHeroWeaponSwitchComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 角色销毁或组件退场时，把切武请求、事务态、表现态和延迟恢复标记一起清掉。
	// 这里的目标不是“回退业务状态”，而是防止下一帧晚到回调继续命中一个已失效组件。
	SpecialWeaponSwitchPresentationState.Clear();
	WeaponSwitchTransactionState.Clear();
	WeaponSwitchRequest.Clear();
	bDeferredCombatInputRecoveryAfterPresentationRequested = false;
	ClearPendingNormalWeaponSwitchAttackHandoff();
	UnbindEquipmentStateDelegates();
	Super::EndPlay(EndPlayReason);
}

bool UHeroWeaponSwitchComponent::EquipWeaponByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!CombatComponent || !EquipmentComponent)
	{
		return false;
	}

	if (WeaponSwitchTransactionState.bSwitchInProgress)
	{
		// 同一时刻只允许一笔正式切武事务推进。
		// 若这里仍允许直接改装备目标，会把“当前事务正在等谁收口”这件事彻底打乱。
		return false;
	}

	if (EquipmentComponent->GetCurrentEquippedLoadoutSlot() == InLoadoutSlot)
	{
		// 再次请求当前槽位属于明确空操作。
		// 这里直接吞掉请求并清空旧排队状态，避免旧切武意图被无意义保留。
		WeaponSwitchRequest.Clear();
		return true;
	}

	if (InLoadoutSlot != EHeroWeaponLoadoutSlot::Unarmed
		&& !EquipmentComponent->HasWeaponAssignedToLoadoutSlot(InLoadoutSlot))
	{
		// 固定武器槽当前没有武器时，切到该槽位同样视为“已处理但不推进事务”。
		// 这样输入层不会反复重试一个本就无可装备结果的请求。
		WeaponSwitchRequest.Clear();
		return true;
	}

	// 正式切武前，先把战斗总控里会污染切武首帧的残留状态统一收掉。
	// 例如旧输入缓冲、攻击窗口、延迟恢复标记都不应该继续带进新的武器上下文。
	// 这样新武器首帧看到的永远是一次干净的切武起点，而不是旧武器留下的半截战斗状态。
	CombatComponent->ResetCombatStateForWeaponSwitch();

	const FHeroEquippedWeaponState EquippedState = EquipmentComponent->GetCurrentEquippedWeaponState();
	// 是否广播切武生命周期事件，取决于“这次切武是否真的从一个有效上下文切到另一个上下文”。
	// 这样出生初始化和无意义空状态不会把外部演出链一并误唤起。
	const bool bShouldBroadcastLifecycle =
		EquippedState.EquippedWeaponDefinition != nullptr
		|| EquippedState.EquippedLoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed;

	// 从这里开始，切武正式进入“事务态”。
	// 后续只有当装备组件真的把当前装备态落到目标槽位，或明确失败时，这笔事务才会收口。
	WeaponSwitchTransactionState.BeginByLoadoutSlot(InLoadoutSlot, bShouldBroadcastLifecycle);
	CombatComponent->BroadcastCombatEventIf(
		bShouldBroadcastLifecycle,
		ActionGameplayTags::Player_Event_WeaponSwitch_Begin);

	if (!EquipmentComponent->EquipWeaponByLoadoutSlot(InLoadoutSlot))
	{
		// 如果装备链连入口都没受理成功，就不能保留这笔事务态。
		// 否则后续既没有成功回调，也没有失败回调来帮它收尾。
		WeaponSwitchTransactionState.Clear();
		return false;
	}

	return true;
}

FGameplayTag UHeroWeaponSwitchComponent::GetInputTagForWeaponLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	switch (InLoadoutSlot)
	{
	case EHeroWeaponLoadoutSlot::Unarmed:
		return ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Unarmed;

	case EHeroWeaponLoadoutSlot::MeleeWeapon:
		return ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Melee;

	case EHeroWeaponLoadoutSlot::RangedWeapon:
		return ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Ranged;

	case EHeroWeaponLoadoutSlot::HybridWeapon:
		return ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Hybrid;

	default:
		break;
	}

	return FGameplayTag();
}

bool UHeroWeaponSwitchComponent::IsWeaponSwitchInputTag(const FGameplayTag InputTag) const
{
	return InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Unarmed
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Melee
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Ranged
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Hybrid
		|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch;
}

bool UHeroWeaponSwitchComponent::IsWeaponSwitchBlockedByCombatState(const FGameplayTag InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return true;
	}

	if (CombatComponent->IsNonAttackInputBlockedByCombatReact(InputTag))
	{
		// 受击主段或恢复段期间，当前框架统一禁止切武，
		// 避免在受击接管还没结束时把武器上下文切到新槽位。
		return true;
	}

	if (!InputTag.IsValid() || IsSpecialWeaponSwitchPresentationActive() || IsWeaponSwitchTransactionInProgress())
	{
		// 无效输入、表现期播放中、事务推进中三种状态都不允许再次起一笔新切武。
		return true;
	}

	if (CombatComponent->WindowRuntimeState.HasCombatLockState())
	{
		// 处决、硬锁动作或其它总控级强锁期间，不允许切武抢占。
		return true;
	}

	if (CombatComponent->IsInputOverrideContextActive())
	{
		// 如果当前动作处在输入改写上下文里，切武是否允许，要受当前抢断窗或恢复窗白名单控制。
		// 这样切武能和攻击 / 防御 / 闪避共用一套“当前动作是否允许被抢占”的语义。
		return !CombatComponent->IsInputAllowedByCurrentOverrideContext(InputTag);
	}

	// 普通情况下，只有在战斗输入整体可用时才允许切武。
	return !CombatComponent->IsAttackEnabled();
}

bool UHeroWeaponSwitchComponent::IsWeaponSwitchBlockedByCooldown() const
{
	const UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!OwnerASC)
	{
		return false;
	}

	return OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::Cooldown_Ability_WeaponSwitch);
}

bool UHeroWeaponSwitchComponent::CanActivateWeaponSwitchAbility(const FGameplayTag InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return false;
	}

	return CombatComponent->PassesSharedNonAttackAbilityHardGate(InputTag) && !IsWeaponSwitchBlockedByCooldown();
}

bool UHeroWeaponSwitchComponent::TryCommitWeaponSwitchAbilityInput(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag InputTag)
{
	if (!InActionASC || !CanActivateWeaponSwitchAbility(InputTag))
	{
		return false;
	}

	// 固定四槽输入只负责表达“我要切到哪个槽位”。
	// 真正进入 GAS 的正式切武能力输入统一收口成 WeaponSwitch 这一条。
	return InActionASC->OnAbilityInputPressed(ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch);
}

bool UHeroWeaponSwitchComponent::HandleWeaponLoadoutSlotInput(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag InputTag,
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!CombatComponent || !CombatInputComponent || !EquipmentComponent || !InputTag.IsValid())
	{
		return false;
	}

	// 槽位输入一旦进入切武组件，就先标记为已消费。
	// 后续无论它是空操作、表现期排队还是冷却延迟重试，都不应该再回流成普通输入。
	CombatInputComponent->MarkInputConsumedByTag(InputTag);

	if (EquipmentComponent->GetCurrentEquippedLoadoutSlot() == InLoadoutSlot)
	{
		// 当前已经在目标槽位时，直接视为成功处理。
		WeaponSwitchRequest.Clear();
		return true;
	}

	if (InLoadoutSlot != EHeroWeaponLoadoutSlot::Unarmed
		&& !EquipmentComponent->HasWeaponAssignedToLoadoutSlot(InLoadoutSlot))
	{
		// 非空手槽若当前没有武器，同样吞掉这次输入。
		// 这里不报失败，是为了保持“玩家按空槽键不会打断当前战斗链”的体验语义。
		WeaponSwitchRequest.Clear();
		return true;
	}

	if (SpecialWeaponSwitchPresentationState.bPresentationActive)
	{
		WeaponSwitchRequest.Clear();
		return true;
	}

	FString FailureReason;
	if (!CanSynchronouslyEquipLoadoutSlot(InLoadoutSlot, &FailureReason))
	{
		WeaponSwitchRequest.Clear();
		const FString WarningMessage = FString::Printf(
			TEXT("[WeaponSwitch] sync-ready precheck failed. slot=%d reason=%s"),
			static_cast<int32>(InLoadoutSlot),
			*FailureReason);
		Debug::Print(WarningMessage, FColor::Orange, 2.5f);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *WarningMessage);
		return true;
	}

	if (!QueueWeaponLoadoutSlotSwitch(InLoadoutSlot))
	{
		// 这里返回 true，表示输入已经被切武组件接管。
		// 即便这次没有写入新请求，也不能再让它回退给别的普通输入链。
		return true;
	}

	// 只有在当前不处于表现期时，才允许这次槽位输入直接尝试起新的切武能力。
	const bool bCommitted = TryCommitWeaponSwitchAbilityInput(InActionASC, InputTag);
	if (!bCommitted)
	{
		WeaponSwitchRequest.Clear();
	}

	return true;
}

void UHeroWeaponSwitchComponent::InitializeCurrentWeaponStateFromEquipment()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!CombatComponent || !EquipmentComponent || !OwnerHeroCharacter)
	{
		return;
	}

	const FHeroEquippedWeaponState EquippedState = EquipmentComponent->GetCurrentEquippedWeaponState();
	if (CombatComponent->IsEquippedWeaponStateMeaningfulForCombatCache(EquippedState))
	{
		// 切武组件启动时，不主动“推装备流程”，只同步当前已经成立的装备结果。
		// 这样战斗总控可以在不重新切武的前提下，拿到稳定的当前武器快照。
		CombatComponent->ApplyEquippedWeaponState(EquippedState);
	}

	if (UHeroAssemblyComponent* HeroAssemblyComponent = OwnerHeroCharacter->GetHeroAssemblyComponent())
	{
		const bool bVisualStateApplied = HeroAssemblyComponent->ApplyCurrentWeaponVisualState(
			EquippedState.EquippedWeaponInstance,
			EquippedState.EquippedWeaponDefinition,
			EquippedState.EquippedWeaponInstance != nullptr);
		if (!bVisualStateApplied)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[WeaponSwitch] 启动同步当前武器表现失败。slot=%d weapon_definition=%s weapon_instance=%s"),
				static_cast<int32>(EquippedState.EquippedLoadoutSlot),
				*GetNameSafe(EquippedState.EquippedWeaponDefinition.Get()),
				*GetNameSafe(EquippedState.EquippedWeaponInstance.Get()));
		}
	}
}

bool UHeroWeaponSwitchComponent::ResolveWeaponSwitchTargetLoadoutSlot(
	EHeroWeaponLoadoutSlot& OutLoadoutSlot,
	const bool bConsumeQueuedRequest)
{
	// 切武组件自身不额外维护第二份目标槽缓存，
	// GA、直接槽位输入以及表现期后的延迟消费都统一从这一份 request 解析目标，
	// 避免三条入口各自记一份“下一个想切到哪”而发生时序分叉。
	return WeaponSwitchRequest.ResolveQueuedLoadoutSlot(OutLoadoutSlot, bConsumeQueuedRequest);
}

bool UHeroWeaponSwitchComponent::CanSynchronouslyEquipLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	FString* OutFailureReason) const
{
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	const UHeroLoadoutRuntimeComponent* LoadoutRuntimeComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<UHeroLoadoutRuntimeComponent>() : nullptr;
	if (!EquipmentComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("equipment component is invalid");
		}
		return false;
	}

	if (EquipmentComponent->GetCurrentEquippedLoadoutSlot() == InLoadoutSlot)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("target loadout slot is already equipped");
		}
		return false;
	}

	if (InLoadoutSlot != EHeroWeaponLoadoutSlot::Unarmed
		&& !EquipmentComponent->HasWeaponAssignedToLoadoutSlot(InLoadoutSlot))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("target loadout slot has no assigned weapon definition");
		}
		return false;
	}

	if (!LoadoutRuntimeComponent)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("loadout runtime component is invalid");
		}
		return false;
	}

	const UDataAsset_WeaponDefinition* LoadedWeaponDefinition =
		LoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(InLoadoutSlot);
	if (!LoadedWeaponDefinition)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = TEXT("target loadout definition is not loaded");
		}
		return false;
	}

	if (!LoadoutRuntimeComponent->IsLoadoutSlotRuntimeReady(InLoadoutSlot))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LoadoutRuntimeComponent->DescribeWeaponRuntimeAssetReadiness(LoadedWeaponDefinition);
		}
		return false;
	}

	return true;
}

bool UHeroWeaponSwitchComponent::CanUseSpecialWeaponSwitchForLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		if (EquipmentComponent->GetCurrentEquippedLoadoutSlot() == InLoadoutSlot)
		{
			return false;
		}

		return EquipmentComponent->IsSpecialWeaponSwitchReady()
			&& IsValid(EquipmentComponent->GetSpecialWeaponSwitchMontageForLoadoutSlot(InLoadoutSlot));
	}

	return false;
}

UAnimMontage* UHeroWeaponSwitchComponent::GetSpecialWeaponSwitchMontageForLoadoutSlot(
	const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (const UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent())
	{
		return EquipmentComponent->GetSpecialWeaponSwitchMontageForLoadoutSlot(InLoadoutSlot);
	}

	return nullptr;
}

void UHeroWeaponSwitchComponent::BeginSpecialWeaponSwitchPresentation()
{
	// 表现期开始前先清一次旧状态，保证这次演出不会继承上一轮特殊切武标记。
	// 这里重置的只有 Presentation 层，不会回滚已经成立的切武请求或真实事务结果。
	SpecialWeaponSwitchPresentationState.Clear();
	SpecialWeaponSwitchPresentationState.BeginPresentation();
}

void UHeroWeaponSwitchComponent::EndSpecialWeaponSwitchPresentation()
{
	if (!SpecialWeaponSwitchPresentationState.bPresentationActive)
	{
		return;
	}

	// 表现期结束并不代表上一笔切武事务失败，只是这层演出门禁已经收口。
	SpecialWeaponSwitchPresentationState.Clear();
	WeaponSwitchRequest.Clear();

	if (bDeferredCombatInputRecoveryAfterPresentationRequested || !GetWorld())
	{
		return;
	}

	// 特殊切武表现结束时，对应的 WeaponSwitch GA 仍可能在本帧退场，
	// 因此这里只给特殊切武保留一层最小的下一帧恢复。
	bDeferredCombatInputRecoveryAfterPresentationRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredRecoverCombatInputAfterPresentation);
}

void UHeroWeaponSwitchComponent::ArmPendingNormalWeaponSwitchAttackHandoff(
	const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (!GetWorld())
	{
		return;
	}

	DeferredNormalWeaponSwitchAttackTargetLoadoutSlot = InLoadoutSlot;
	DeferredNormalWeaponSwitchAttackHandoffToken = ++NextNormalWeaponSwitchAttackHandoffToken;
	if (bDeferredNormalWeaponSwitchAttackHandoffRequested)
	{
		return;
	}

	bDeferredNormalWeaponSwitchAttackHandoffRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredNormalWeaponSwitchAttackHandoff);
}

void UHeroWeaponSwitchComponent::ResetRuntimeStateForHeroStartup()
{
	WeaponSwitchRequest.Clear();
	WeaponSwitchTransactionState.Clear();
	SpecialWeaponSwitchPresentationState.Clear();
	bDeferredCombatInputRecoveryAfterPresentationRequested = false;
	ClearPendingNormalWeaponSwitchAttackHandoff();
}

void UHeroWeaponSwitchComponent::HandleBufferedInputExpired()
{
	ClearQueuedWeaponLoadoutSlotSwitch();
}

bool UHeroWeaponSwitchComponent::QueueWeaponLoadoutSlotSwitch(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (WeaponSwitchTransactionState.bSwitchInProgress)
	{
		// 正式事务推进中时，不允许再覆盖当前排队目标，
		// 否则“当前事务究竟为谁收尾”会被破坏。
		return false;
	}

	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent)
	{
		return false;
	}

	const float RequestWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const int32 RequestOrder = CombatInputComponent->GenerateNextCombatIntentOrder();
	// 切武请求统一使用“最后一次输入覆盖前一次”的策略。
	// 对固定四槽来说，运行时真正重要的是玩家此刻最终想切去哪一槽。
	// 因此 Request 层只保留一个最终目标，不把槽位按键继续扩成多笔排队事务。
	WeaponSwitchRequest.SetRequest(InLoadoutSlot, RequestWorldTime, RequestOrder);
	return true;
}

void UHeroWeaponSwitchComponent::HandleEquippedWeaponStateChanged(
	const FHeroEquippedWeaponState& InEquippedWeaponState)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent)
	{
		return;
	}

	CombatComponent->ApplyEquippedWeaponState(InEquippedWeaponState);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[WeaponSwitch] Equipped state broadcast consumed. slot=%d weapon_tag=%s linked_layer=%s should_use=%s weapon_instance=%s combat_mode=%d running_montage=%s"),
		static_cast<int32>(InEquippedWeaponState.EquippedLoadoutSlot),
		*InEquippedWeaponState.EquippedWeaponTag.ToString(),
		*GetNameSafe(CombatComponent->GetCurrentWeaponLinkedAnimLayerClass()),
		CombatComponent->HasEquippedWeaponLinkedLayerPresentation() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(InEquippedWeaponState.EquippedWeaponInstance.Get()),
		static_cast<int32>(CombatComponent->GetCombatMode()),
		*GetNameSafe(CombatComponent->GetCurrentRunningAnimMontage()));
	if (AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter())
	{
		if (UHeroAssemblyComponent* HeroAssemblyComponent = OwnerHeroCharacter->GetHeroAssemblyComponent())
		{
			const bool bVisualStateApplied = HeroAssemblyComponent->ApplyCurrentWeaponVisualState(
				InEquippedWeaponState.EquippedWeaponInstance,
				InEquippedWeaponState.EquippedWeaponDefinition,
				InEquippedWeaponState.EquippedWeaponInstance != nullptr);
			if (!bVisualStateApplied)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[WeaponSwitch] 当前装备武器表现刷新失败。slot=%d weapon_definition=%s weapon_instance=%s"),
					static_cast<int32>(InEquippedWeaponState.EquippedLoadoutSlot),
					*GetNameSafe(InEquippedWeaponState.EquippedWeaponDefinition.Get()),
					*GetNameSafe(InEquippedWeaponState.EquippedWeaponInstance.Get()));
			}
		}
	}

	if (!WeaponSwitchTransactionState.MatchesEquippedState(InEquippedWeaponState))
	{
		// 装备组件广播的状态变化不一定都属于“当前这笔切武事务”的完成信号。
		// 只有目标槽位真正对上了，才允许这里关闭事务。
		return;
	}

	const bool bShouldBroadcastLifecycle = WeaponSwitchTransactionState.bShouldBroadcastLifecycle;
	WeaponSwitchTransactionState.Clear();
	CombatComponent->BroadcastCombatEventIf(
		bShouldBroadcastLifecycle,
		ActionGameplayTags::Player_Event_WeaponSwitch_End);

	if (SpecialWeaponSwitchPresentationState.bPresentationActive)
	{
		return;
	}

	CombatComponent->FinalizeWeaponSwitchRuntimeState();
}

void UHeroWeaponSwitchComponent::HandleWeaponLoadoutEquipFailed(const EHeroWeaponLoadoutSlot InLoadoutSlot)
{
	if (!WeaponSwitchTransactionState.bSwitchInProgress
		|| WeaponSwitchTransactionState.TargetLoadoutSlot != InLoadoutSlot)
	{
		return;
	}

	// 装备失败时必须显式把事务清掉，
	// 否则这笔“永远等不到成功回调”的旧事务会一直阻塞后续切武。
	WeaponSwitchTransactionState.Clear();
	WeaponSwitchRequest.Clear();
	if (UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent())
	{
		if (SpecialWeaponSwitchPresentationState.bPresentationActive)
		{
			return;
		}

		CombatInputComponent->RecoverCombatInputAfterWeaponSwitch();
	}
}

void UHeroWeaponSwitchComponent::HandleDeferredRecoverCombatInputAfterPresentation()
{
	if (!bDeferredCombatInputRecoveryAfterPresentationRequested)
	{
		return;
	}

	bDeferredCombatInputRecoveryAfterPresentationRequested = false;

	if (SpecialWeaponSwitchPresentationState.bPresentationActive
		|| WeaponSwitchTransactionState.bSwitchInProgress)
	{
		return;
	}

	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		CombatComponent->FinalizeWeaponSwitchRuntimeState();
		if (UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent())
		{
			CombatInputComponent->RecoverCombatInputAfterWeaponSwitch();
		}
	}
}

void UHeroWeaponSwitchComponent::HandleDeferredNormalWeaponSwitchAttackHandoff()
{
	if (!bDeferredNormalWeaponSwitchAttackHandoffRequested)
	{
		return;
	}

	const EHeroWeaponLoadoutSlot TargetLoadoutSlot = DeferredNormalWeaponSwitchAttackTargetLoadoutSlot;
	const int32 HandoffToken = DeferredNormalWeaponSwitchAttackHandoffToken;
	ClearPendingNormalWeaponSwitchAttackHandoff();

	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	UActionAbilitySystemComponent* OwnerASC = GetOwningActionAbilitySystemComponent();
	if (!CombatComponent || !CombatInputComponent || !EquipmentComponent || !OwnerASC)
	{
		return;
	}

	const bool bSpecialPresentationActive = SpecialWeaponSwitchPresentationState.bPresentationActive;
	const bool bTransactionInProgress = WeaponSwitchTransactionState.bSwitchInProgress;
	const EHeroWeaponLoadoutSlot CurrentLoadoutSlot = EquipmentComponent->GetCurrentEquippedLoadoutSlot();
	const bool bWeaponSwitchAbilityStillActive =
		OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_WeaponSwitch_Active);
	if (bSpecialPresentationActive
		|| bTransactionInProgress
		|| CurrentLoadoutSlot != TargetLoadoutSlot
		|| bWeaponSwitchAbilityStillActive)
	{
		const FString WarningMessage = FString::Printf(
			TEXT("[WeaponSwitch] deferred normal switch attack handoff skipped. token=%d target_slot=%d current_slot=%d special_presentation=%d transaction=%d weapon_switch_active=%d"),
			HandoffToken,
			static_cast<int32>(TargetLoadoutSlot),
			static_cast<int32>(CurrentLoadoutSlot),
			bSpecialPresentationActive ? 1 : 0,
			bTransactionInProgress ? 1 : 0,
			bWeaponSwitchAbilityStillActive ? 1 : 0);
		Debug::Print(WarningMessage, FColor::Orange, 2.5f);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *WarningMessage);
		CombatInputComponent->RecoverCombatInputAfterWeaponSwitch();
		return;
	}

	UHeroAttackComponent* HeroAttackComponent = CombatComponent->GetOwningHeroAttackComponent();
	const bool bAttackTriggered = HeroAttackComponent
		&& HeroAttackComponent->TryTriggerDefaultLightAttackAfterNormalWeaponSwitch(OwnerASC);
	if (!bAttackTriggered)
	{
		const FString WarningMessage = FString::Printf(
			TEXT("[WeaponSwitch] deferred normal switch attack handoff failed. token=%d target_slot=%d current_slot=%d request=%s"),
			HandoffToken,
			static_cast<int32>(TargetLoadoutSlot),
			static_cast<int32>(CurrentLoadoutSlot),
			*ActionGameplayTags::Attack_Request_Default.GetTag().ToString());
		Debug::Print(WarningMessage, FColor::Orange, 2.5f);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *WarningMessage);
		CombatInputComponent->RecoverCombatInputAfterWeaponSwitch();
	}
}

void UHeroWeaponSwitchComponent::ClearPendingNormalWeaponSwitchAttackHandoff()
{
	bDeferredNormalWeaponSwitchAttackHandoffRequested = false;
	DeferredNormalWeaponSwitchAttackTargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
	DeferredNormalWeaponSwitchAttackHandoffToken = 0;
}

void UHeroWeaponSwitchComponent::BindEquipmentStateDelegates()
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!EquipmentComponent)
	{
		return;
	}

	if (!EquippedWeaponStateChangedHandle.IsValid())
	{
		EquippedWeaponStateChangedHandle = EquipmentComponent->OnEquippedWeaponStateChanged().AddUObject(
			this,
			&ThisClass::HandleEquippedWeaponStateChanged);
	}

	if (!WeaponLoadoutEquipFailedHandle.IsValid())
	{
		WeaponLoadoutEquipFailedHandle = EquipmentComponent->OnWeaponLoadoutEquipFailed().AddUObject(
			this,
			&ThisClass::HandleWeaponLoadoutEquipFailed);
	}
}

void UHeroWeaponSwitchComponent::UnbindEquipmentStateDelegates()
{
	UHeroEquipmentComponent* EquipmentComponent = GetOwningHeroEquipmentComponent();
	if (!EquipmentComponent)
	{
		EquippedWeaponStateChangedHandle.Reset();
		WeaponLoadoutEquipFailedHandle.Reset();
		return;
	}

	if (EquippedWeaponStateChangedHandle.IsValid())
	{
		EquipmentComponent->OnEquippedWeaponStateChanged().Remove(EquippedWeaponStateChangedHandle);
		EquippedWeaponStateChangedHandle.Reset();
	}

	if (WeaponLoadoutEquipFailedHandle.IsValid())
	{
		EquipmentComponent->OnWeaponLoadoutEquipFailed().Remove(WeaponLoadoutEquipFailedHandle);
		WeaponLoadoutEquipFailedHandle.Reset();
	}
}

AActionHeroCharacter* UHeroWeaponSwitchComponent::GetOwningHeroCharacter() const
{
	return Cast<AActionHeroCharacter>(GetOwner());
}

UHeroCombatComponent* UHeroWeaponSwitchComponent::GetOwningHeroCombatComponent() const
{
	if (AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		return OwnerCharacter->GetHeroCombatComponent();
	}

	return nullptr;
}

UHeroCombatInputComponent* UHeroWeaponSwitchComponent::GetOwningHeroCombatInputComponent() const
{
	if (AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		return OwnerCharacter->GetHeroCombatInputComponent();
	}

	return nullptr;
}

UHeroEquipmentComponent* UHeroWeaponSwitchComponent::GetOwningHeroEquipmentComponent() const
{
	if (AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		return OwnerCharacter->FindComponentByClass<UHeroEquipmentComponent>();
	}

	return nullptr;
}

UActionAbilitySystemComponent* UHeroWeaponSwitchComponent::GetOwningActionAbilitySystemComponent() const
{
	if (AActionHeroCharacter* OwnerCharacter = GetOwningHeroCharacter())
	{
		return OwnerCharacter->GetActionAbilitySystemComponent();
	}

	return nullptr;
}
