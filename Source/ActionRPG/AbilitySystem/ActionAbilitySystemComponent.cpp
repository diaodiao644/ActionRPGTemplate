// 文件说明：实现 AbilitySystemComponent 侧的战斗输入转发、关系裁决与调试辅助。

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "AbilitySystem/Abilities/ActionGameplayAbilityBase.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "AbilitySystem/Effects/ActionGE_CombatModifier.h"
#include "AbilitySystem/Effects/ActionGE_ExecutionProtection.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "DataAssets/Effects/DataAsset_StatusEffectDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "GameplayEffect.h"

namespace
{
	FString DescribeGameplayTagsForDebug(const FGameplayTagContainer& InTags)
	{
		TArray<FString> TagNames;
		TagNames.Reserve(InTags.Num());

		for (const FGameplayTag& Tag : InTags)
		{
			TagNames.Add(Tag.ToString());
		}

		return TagNames.Num() > 0
			? FString::Join(TagNames, TEXT(", "))
			: TEXT("None");
	}

	FString AbilitySystemComponentBoolToDebugText(const bool bValue)
	{
		return bValue ? TEXT("是") : TEXT("否");
	}

	bool IsExpectedNonAttackGateBlockReason(const FString& InFailureReason)
	{
		return InFailureReason.Contains(TEXT("layer=WeaponSwitchPresentation"))
			|| InFailureReason.Contains(TEXT("layer=WeaponSwitchChainWindow"));
	}

	const TArray<FGameplayTag>& GetHeroCombatAbilityAuditTags()
	{
		static const TArray<FGameplayTag> HeroCombatAbilityTags =
		{
			ActionGameplayTags::Player_Ability_ProjectileSwitch,
			ActionGameplayTags::Player_Ability_Attack,
			ActionGameplayTags::Player_Ability_SpiritSkill,
			ActionGameplayTags::Player_Ability_CombatModeOrDefense,
			ActionGameplayTags::Player_Ability_WeaponSwitch,
			ActionGameplayTags::Player_Ability_Dodge,
			ActionGameplayTags::Player_Ability_Execution
		};

		return HeroCombatAbilityTags;
	}

	bool SupportsNonAttackInputGateDebug(const FGameplayTag& InputTag)
	{
		return InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense
			|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge
			|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_Execution
			|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch
			|| ActionGameplayTags::IsSpiritSkillInputTag(InputTag)
			|| InputTag == ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch;
	}

}

UActionAbilitySystemComponent::UActionAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CombatModifierGameplayEffectClass = UActionGE_CombatModifier::StaticClass();
	ExecutionProtectionGameplayEffectClass = UActionGE_ExecutionProtection::StaticClass();
}

bool UActionAbilitySystemComponent::OnAbilityInputPressed(const FGameplayTag& InInputTag)
{
	if (!InInputTag.IsValid())
	{
		return false;
	}

	// 输入命中只负责把“这个输入标签被按下了”转发给对应 Spec。
	// 如果 Spec 已经处于激活态，则继续走 GAS 自带的 InputPressed 通知；
	// 如果还未激活，才进入关系系统裁决，判断这次按下是否能正式起手。
	if (FGameplayAbilitySpec* Spec = FindBestAbilitySpecByInputTag(InInputTag))
	{
		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			// 正在运行的 Ability 继续收到按下事件，供保持输入、蓄力或内部连段判断使用。
			AbilitySpecInputPressed(*Spec);
			return true;
		}
		else
		{
			return TryActivateAbilitySpecWithRelationship(*Spec);
		}
	}

	return false;
}

bool UActionAbilitySystemComponent::OnAbilityInputHeld(const FGameplayTag& InInputTag)
{
	if (!InInputTag.IsValid())
	{
		return false;
	}

	// Held 与 Pressed 共享同一条查找链，但不会跳过关系裁决。
	// 这保证了“长按触发起手”和“长按维持已激活能力”仍然由正式激活链统一裁决。
	if (FGameplayAbilitySpec* Spec = FindBestAbilitySpecByInputTag(InInputTag))
	{
		Spec->InputPressed = true;

		if (Spec->IsActive())
		{
			// 已激活时沿用 GAS 的 Held 语义，让能力实例自行决定是否持续响应。
			AbilitySpecInputPressed(*Spec);
			return true;
		}
		else
		{
			return TryActivateAbilitySpecWithRelationship(*Spec);
		}
	}

	return false;
}

void UActionAbilitySystemComponent::OnAbilityInputReleased(const FGameplayTag& InInputTag)
{
	if (!InInputTag.IsValid())
	{
		return;
	}

	// Released 从不主动尝试激活新能力。
	// 它只把释放事件回传给已经激活的实例，避免“松开按键”反向触发新的正式激活链。
	if (FGameplayAbilitySpec* Spec = FindBestAbilitySpecByInputTag(InInputTag))
	{
		Spec->InputPressed = false;

		if (Spec->IsActive())
		{
			// 释放事件继续交给正在运行的 Ability，让其自行处理松键收尾。
			AbilitySpecInputReleased(*Spec);
		}
	}
}

FGameplayAbilitySpecHandle UActionAbilitySystemComponent::GrantAbility(
	const TSubclassOf<UGameplayAbility>& InGrantedAbilities,
	FGameplayTag AbilityTag,
	int32 ApplyLevel)
{
	if (!InGrantedAbilities)
	{
		return FGameplayAbilitySpecHandle();
	}

	FGameplayAbilitySpec AbilitySpec(InGrantedAbilities, ApplyLevel);
	AbilitySpec.SourceObject = GetAvatarActor();
	if (AbilityTag.IsValid())
	{
		AbilitySpec.DynamicAbilityTags.AddTag(AbilityTag);
	}

	return GiveAbility(AbilitySpec);
}

void UActionAbilitySystemComponent::RemovedAbility(FGameplayAbilitySpecHandle& InSpecHandlesToRemove)
{
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(InSpecHandlesToRemove);
	if (!AbilitySpec)
	{
		return;
	}

	if (AbilitySpec->IsActive())
	{
		CancelAbilityHandle(InSpecHandlesToRemove);
	}

	ClearAbility(InSpecHandlesToRemove);
}

bool UActionAbilitySystemComponent::TryActivateAbilitySpecWithRelationship(FGameplayAbilitySpec& Spec)
{
	TArray<FGameplayAbilitySpecHandle> AbilityHandlesToCancel;
	FString FailureReason;
	const UActionGameplayAbilityBase* CandidateAbility = ResolveActionAbilityFromSpec(Spec);
	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetAvatarActor());
	const UActionCombatReactComponent* CombatReactComponent =
		OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;

	// 正式激活前，先把“是否允许起手”和“需要取消谁”一次性算清楚。
	// 这里故意分成“先收集取消目标 -> 再统一取消 -> 最后尝试激活”，
	// 避免边遍历边取消把当前 ASC 的活跃能力集合改乱，也避免先取消成功后才发现候选 Ability 根本不能起手。
	if (!ResolveAbilityRelationshipBeforeActivation(Spec, AbilityHandlesToCancel, FailureReason))
	{
		if (!FailureReason.IsEmpty())
		{
			const FString BlockedDebugText =
				BuildRelationshipBlockedDebugText(CandidateAbility, CombatReactComponent, FailureReason);
			Debug::Print(BlockedDebugText, FColor::Orange, 2.5f);
			if (IsExpectedNonAttackGateBlockReason(FailureReason))
			{
				UE_LOG(ActionRPG, Log, TEXT("%s"), *BlockedDebugText);
			}
			else
			{
				UE_LOG(ActionRPG, Warning, TEXT("%s"), *BlockedDebugText);
			}
		}

		return false;
	}

	for (const FGameplayAbilitySpecHandle AbilityHandleToCancel : AbilityHandlesToCancel)
	{
		if (AbilityHandleToCancel.IsValid())
		{
			CancelAbilityHandle(AbilityHandleToCancel);
		}
	}

	const bool bActivated = TryActivateAbility(Spec.Handle);
	if (!bActivated)
	{
		const FString ActivationFailedDebugText = FString::Printf(
			TEXT("[GA Relationship] Activation failed after relationship precheck passed. Candidate=%s Priority=%d Activation=%s"),
			CandidateAbility ? *CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility"),
			CandidateAbility ? CandidateAbility->GetAbilityPriority() : 0,
			CandidateAbility
				? *CandidateAbility->DescribeCombatReactActivationDecision(CombatReactComponent)
				: TEXT("No candidate ability."));
		Debug::Print(ActivationFailedDebugText, FColor::Orange, 2.5f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *ActivationFailedDebugText);
	}

	return bActivated;
}

bool UActionAbilitySystemComponent::ResolveAbilityRelationshipBeforeActivation(
	const FGameplayAbilitySpec& CandidateSpec,
	TArray<FGameplayAbilitySpecHandle>& OutAbilityHandlesToCancel,
	FString& OutFailureReason) const
{
	OutAbilityHandlesToCancel.Reset();
	OutFailureReason.Reset();

	// 这一步只做“关系系统自己的前置裁决”：
	// 1. 候选 Ability 自身是否通过 CommitCheck / 受击规则 / 运行时依赖检查；
	// 2. 当前活跃 Ability 里哪些会阻断它；
	// 3. 哪些活跃 Ability 可以被它正式打断。
	// 真正的 TryActivate 仍留给 GAS 自己执行，这里不替代最终激活流程。
	UActionGameplayAbilityBase* CandidateAbility = ResolveActionAbilityFromSpec(CandidateSpec);
	if (HasMatchingGameplayTag(ActionGameplayTags::State_Combat_ExecutionVictim_HardLock) && CandidateAbility)
	{
		OutFailureReason = FString::Printf(
			TEXT("%s is blocked by execution victim hard lock."),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	if (!CandidateAbility || !CandidateAbility->UsesAbilityRelationshipSystem())
	{
		return true;
	}

	const int32 CandidatePriority = CandidateAbility->GetAbilityPriority();
	const FString CandidateDebugName = CandidateAbility->GetAbilityDebugName();
	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetAvatarActor());
	const UActionCombatReactComponent* CombatReactComponent =
		OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	FGameplayTagContainer PrecheckFailureTags;
	if (!ActorInfo)
	{
		OutFailureReason = FString::Printf(
			TEXT("%s has no valid actor info before relationship cancellation."),
			*CandidateDebugName);
		return false;
	}

	if (!CandidateAbility->CommitCheck(CandidateSpec.Handle, ActorInfo, FGameplayAbilityActivationInfo(), &PrecheckFailureTags))
	{
		OutFailureReason = FString::Printf(
			TEXT("%s failed commit precheck before relationship cancellation. FailureTags=%s"),
			*CandidateDebugName,
			*DescribeGameplayTagsForDebug(PrecheckFailureTags));
		return false;
	}

	if (!CandidateAbility->IsActivationAllowedByCombatReact(CombatReactComponent))
	{
		OutFailureReason = FString::Printf(
			TEXT("%s is rejected by combat react activation rules before relationship cancellation."),
			*CandidateDebugName);
		return false;
	}

	FString RuntimePrecheckFailureReason;
	if (!CandidateAbility->ValidateRelationshipActivationPreconditions(RuntimePrecheckFailureReason))
	{
		OutFailureReason = RuntimePrecheckFailureReason.IsEmpty()
			? FString::Printf(TEXT("%s failed runtime precheck before relationship cancellation."), *CandidateDebugName)
			: RuntimePrecheckFailureReason;
		return false;
	}

	const FGameplayTag CandidateCombatReactInputTag = CandidateAbility->GetPrimaryInputTagForCombatReact();
	const bool bCandidateCanTransitionFromRecoveryCancelWindow =
		CandidateAbility->AllowsActivationDuringRecoveryCancelWindow()
		&& CandidateCombatReactInputTag.IsValid()
		&& CombatReactComponent
		&& CombatReactComponent->CanActivateAbilityDuringRecoveryCancelWindow(CandidateCombatReactInputTag);

	for (const FGameplayAbilitySpec& ActiveSpec : GetActivatableAbilities())
	{
		if (!ActiveSpec.IsActive() || ActiveSpec.Handle == CandidateSpec.Handle)
		{
			continue;
		}

		UActionGameplayAbilityBase* ActiveAbility = ResolveActionAbilityFromSpec(ActiveSpec);
		if (!ActiveAbility || !ActiveAbility->UsesAbilityRelationshipSystem())
		{
			continue;
		}

		const int32 ActivePriority = ActiveAbility->GetAbilityPriority();
		const FString ActiveDebugName = ActiveAbility->GetAbilityDebugName();
		const bool bCandidateTargetsActiveAbility =
			CandidateAbility->CanCancelAbilitiesWithTags(ActiveAbility->GetAbilityIdentityTags());

		if (!bCandidateTargetsActiveAbility
			&& CandidateAbility->IsBlockedByActivationOwnedTags(ActiveAbility->GetActivationOwnedTagsForRelationship()))
		{
			OutFailureReason = FString::Printf(
				TEXT("%s is blocked by active ability %s through activation tags."),
				*CandidateDebugName,
				*ActiveDebugName);
			return false;
		}

		if (!bCandidateTargetsActiveAbility)
		{
			continue;
		}

		if (CandidatePriority >= ActivePriority
			&& ActiveAbility->IsProtectedByCombatReact(CombatReactComponent))
		{
			OutFailureReason = FString::Printf(
				TEXT("%s is protected during combat react recovery and cannot be interrupted by %s. %s"),
				*ActiveDebugName,
				*CandidateDebugName,
				*ActiveAbility->DescribeCombatReactInterruptDecision(CombatReactComponent));
			return false;
		}

		if (CandidatePriority > ActivePriority)
		{
			const bool bCanInterruptThisActiveAbility =
				CandidateAbility->CanInterruptLowerPriorityAbilitiesInCurrentReact(CombatReactComponent)
				&& ActiveAbility->CanBeInterruptedByHigherPriorityInCurrentReact(CombatReactComponent)
				&& bCandidateTargetsActiveAbility;

			if (!bCanInterruptThisActiveAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s has higher priority but still cannot interrupt %s. Candidate=%s Target=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*CandidateAbility->DescribeCombatReactInterruptDecision(CombatReactComponent),
					*ActiveAbility->DescribeCombatReactInterruptDecision(CombatReactComponent));
				return false;
			}

			OutAbilityHandlesToCancel.AddUnique(ActiveSpec.Handle);
			continue;
		}

		if (CandidatePriority == ActivePriority)
		{
			const bool bCanInterruptSamePriorityAbility =
				CandidateAbility->CanInterruptSamePriorityAbilitiesInCurrentReact(CombatReactComponent)
				&& ActiveAbility->CanBeInterruptedBySamePriorityInCurrentReact(CombatReactComponent)
				&& bCandidateTargetsActiveAbility;

			if (!bCanInterruptSamePriorityAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s and %s share the same priority and cannot interrupt each other now. Candidate=%s Target=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*CandidateAbility->DescribeCombatReactInterruptDecision(CombatReactComponent),
					*ActiveAbility->DescribeCombatReactInterruptDecision(CombatReactComponent));
				return false;
			}

			OutAbilityHandlesToCancel.AddUnique(ActiveSpec.Handle);
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("%s has lower priority than active ability %s and cannot interrupt it."),
			*CandidateDebugName,
			*ActiveDebugName);
		return false;
	}

	return true;
}

FString UActionAbilitySystemComponent::BuildRelationshipBlockedDebugText(
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	const FString& FailureReason) const
{
	// 所有关系裁决失败文本统一从这里收口，目的是让屏幕调试和日志输出始终使用同一套口径。
	// 后续如果补字段或换排查重点，只需要维护这一处，不必分散到每个失败分支。
	if (!CandidateAbility)
	{
		return FString::Printf(
			TEXT("[GA Relationship] Activation blocked. Candidate=UnknownAbility Reason=%s"),
			*FailureReason);
	}

	return FString::Printf(
		TEXT("[GA Relationship] Activation blocked. Candidate=%s Priority=%d Reason=%s Activation=%s Interrupt=%s"),
		*CandidateAbility->GetAbilityDebugName(),
		CandidateAbility->GetAbilityPriority(),
		*FailureReason,
		*CandidateAbility->DescribeCombatReactActivationDecision(CombatReactComponent),
		*CandidateAbility->DescribeCombatReactInterruptDecision(CombatReactComponent));
}

UActionGameplayAbilityBase* UActionAbilitySystemComponent::ResolveActionAbilityFromSpec(const FGameplayAbilitySpec& Spec) const
{
	if (UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance())
	{
		if (UActionGameplayAbilityBase* ActionAbility = Cast<UActionGameplayAbilityBase>(PrimaryInstance))
		{
			return ActionAbility;
		}
	}

	const TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();
	for (UGameplayAbility* AbilityInstance : AbilityInstances)
	{
		if (UActionGameplayAbilityBase* ActionAbility = Cast<UActionGameplayAbilityBase>(AbilityInstance))
		{
			return ActionAbility;
		}
	}

	return Cast<UActionGameplayAbilityBase>(Spec.Ability);
}

FGameplayAbilitySpec* UActionAbilitySystemComponent::FindBestAbilitySpecByInputTag(const FGameplayTag& InInputTag)
{
	if (!InInputTag.IsValid())
	{
		return nullptr;
	}

	// 输入标签到 AbilitySpec 的映射以“最后授予的优先”为准。
	// 倒序扫描可以让后授予、后覆盖的输入壳自然优先命中，而不需要额外维护一层排序缓存。
	TArray<FGameplayAbilitySpec>& LocalAbilitySpecs = GetActivatableAbilities();
	for (int32 Index = LocalAbilitySpecs.Num() - 1; Index >= 0; --Index)
	{
		FGameplayAbilitySpec& Spec = LocalAbilitySpecs[Index];
		if (Spec.Ability && Spec.DynamicAbilityTags.HasTagExact(InInputTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}

FGameplayAbilitySpec* UActionAbilitySystemComponent::GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(AbilityTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}

const FGameplayAbilitySpec* UActionAbilitySystemComponent::GetActivatableAbilitySpecByTag(const FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(AbilityTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}

bool UActionAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return false;
	}

	FGameplayAbilitySpec* AbilitySpec = GetActivatableAbilitySpecByTag(AbilityTag);
	if (!AbilitySpec)
	{
		return false;
	}

	return TryActivateAbilitySpecWithRelationship(*AbilitySpec);
}

void UActionAbilitySystemComponent::CancelAbilityByTag(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return;
	}

    Debug::Print(TEXT("Cancel tagged abilities"));

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.DynamicAbilityTags.HasTagExact(AbilityTag))
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void UActionAbilitySystemComponent::CancelAbilityByAbilityTag(FGameplayTag AbilityTag)
{
	if (!AbilityTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability)
		{
			continue;
		}

		if (Spec.Ability->AbilityTags.HasTag(AbilityTag))
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void UActionAbilitySystemComponent::CancelAbilitiesByAbilityTags(const FGameplayTagContainer& AbilityTags)
{
	if (AbilityTags.Num() <= 0)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability)
		{
			continue;
		}

		if (Spec.Ability->AbilityTags.HasAny(AbilityTags))
		{
			CancelAbilityHandle(Spec.Handle);
		}
	}
}

void UActionAbilitySystemComponent::PrintHeroCombatAbilityRelationshipAudit() const
{
	TArray<FString> AuditLines;
	GetHeroCombatAbilityRelationshipAuditLines(AuditLines);

	if (AuditLines.Num() <= 0)
	{
		const FString EmptyAuditText = TEXT("[GA Audit] 当前 ASC 上未找到任何 Hero 战斗 GA 关系配置。");
		Debug::Print(EmptyAuditText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *EmptyAuditText);
		return;
	}

	for (const FString& AuditLine : AuditLines)
	{
		Debug::Print(AuditLine, FColor::Cyan, 6.0f);
		UE_LOG(ActionRPG, Log, TEXT("%s"), *AuditLine);
	}
}

bool UActionAbilitySystemComponent::ApplyAbilityCooldownByInputTag(const FGameplayTag& AbilityInputTag)
{
	if (!AbilityInputTag.IsValid())
	{
		UE_LOG(ActionRPG, Warning, TEXT("[Cooldown] 外部补冷却失败：输入标签无效。"));
		return false;
	}

	if (!AbilityActorInfo.IsValid())
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：ASC ActorInfo 无效。InputTag=%s"),
			*AbilityInputTag.ToString());
		return false;
	}

	// 这个入口不是普通首段激活路径的冷却提交口。
	// 它主要服务组件侧超时、待命收尾或跨动作链断开后的“补正式冷却”场景，
	// 让组件在不重新激活 Ability 的前提下，仍能复用 Ability 自己配置好的冷却语义。
	FGameplayAbilitySpec* AbilitySpec = GetActivatableAbilitySpecByTag(AbilityInputTag);
	if (!AbilitySpec)
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：未找到 AbilitySpec。InputTag=%s"),
			*AbilityInputTag.ToString());
		return false;
	}

	const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(*AbilitySpec);
	if (!ActionAbility)
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：Spec 未解析到 ActionGameplayAbility。InputTag=%s Ability=%s"),
			*AbilityInputTag.ToString(),
			*GetNameSafe(AbilitySpec->Ability));
		return false;
	}

	const bool bApplied = ActionAbility->ApplyConfiguredCooldownExternally(
		AbilitySpec->Handle,
		AbilityActorInfo.Get());
	if (!bApplied)
	{
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：Ability 拒绝应用冷却。InputTag=%s Ability=%s"),
			*AbilityInputTag.ToString(),
			*ActionAbility->GetAbilityDebugName());
	}

	return bApplied;
}

bool UActionAbilitySystemComponent::PrintHeroCombatAbilityDebugByInputTag(const FGameplayTag AbilityInputTag) const
{
	if (!AbilityInputTag.IsValid())
	{
		const FString InvalidInputText = TEXT("[GA Debug] 输入标签无效，无法打印 Hero 战斗 GA 调试摘要。");
		Debug::Print(InvalidInputText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *InvalidInputText);
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = GetActivatableAbilitySpecByTag(AbilityInputTag);
	if (!AbilitySpec)
	{
		const FString MissingAbilityText = FString::Printf(
			TEXT("[GA Debug] 当前 ASC 未授予输入=%s 对应的战斗 GA。"),
			*AbilityInputTag.ToString());
		Debug::Print(MissingAbilityText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *MissingAbilityText);
		return false;
	}

	// 这条调试链把“授予关系配置、当前受击规则判定、HeroCombat 输入门禁”串到一起输出。
	// 目的是让联调时能从一个输入标签直接看到：
	// 1. 当前 ASC 授予了谁；
	// 2. 关系系统会怎么判它；
	// 3. 如果它是非攻击输入，HeroCombatComponent 还会不会在恢复窗口外再次拦住它。
	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetAvatarActor());
	const UActionCombatReactComponent* CombatReactComponent =
		OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;
	const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(*AbilitySpec);

	if (ActionAbility)
	{
		const FString AuditLine = BuildHeroCombatAbilityRelationshipAuditLine(*AbilitySpec, ActionAbility, CombatReactComponent);
		Debug::Print(AuditLine, FColor::Green, 6.0f);
		UE_LOG(ActionRPG, Log, TEXT("%s"), *AuditLine);

		const FString DecisionText = FString::Printf(
			TEXT("[GA Debug] Ability=%s Activation=%s Interrupt=%s Rule=%s"),
			*ActionAbility->GetAbilityDebugName(),
			*ActionAbility->DescribeCombatReactActivationDecision(CombatReactComponent),
			*ActionAbility->DescribeCombatReactInterruptDecision(CombatReactComponent),
			*ActionAbility->DescribeCombatReactRule());
		Debug::Print(DecisionText, FColor::Green, 6.0f);
		UE_LOG(ActionRPG, Log, TEXT("%s"), *DecisionText);
	}

	if (const UActionHeroGameplayAbility* HeroAbility = Cast<UActionHeroGameplayAbility>(AbilitySpec->GetPrimaryInstance()))
	{
		HeroAbility->PrintCurrentCombatReactDebug();
		return true;
	}

	if (const AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActor()))
	{
		if (const UActionCombatReactComponent* HeroCombatReactComponent = HeroCharacter->GetActionCombatReactComponent())
		{
			const FString CombatReactStateText = FString::Printf(
				TEXT("[GA Debug] 当前受击摘要：%s"),
				*HeroCombatReactComponent->DescribeCurrentCombatReactState());
			Debug::Print(CombatReactStateText, FColor::Green, 6.0f);
			UE_LOG(ActionRPG, Log, TEXT("%s"), *CombatReactStateText);
		}

		if (SupportsNonAttackInputGateDebug(AbilityInputTag))
		{
			if (const UHeroCombatComponent* HeroCombatComponent = HeroCharacter->GetHeroCombatComponent())
			{
				const FString GateText = HeroCombatComponent->DescribeNonAttackInputGateForDebug(AbilityInputTag);
				Debug::Print(GateText, FColor::Green, 6.0f);
				UE_LOG(ActionRPG, Log, TEXT("%s"), *GateText);
			}
		}
	}

	return true;
}

UGameplayAbility* UActionAbilitySystemComponent::GetActiveAbilityInstanceByAbilityTag(const FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}

	// 这里按 Ability 身份标签查“当前已经在运行的实例”，
	// 用来给外部调试、链路校验或需要访问实例态的流程提供只读入口。
	// 优先返回主实例，其次回退到实例数组的第一个实例，最后才返回类默认对象。
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || !Spec.Ability->AbilityTags.HasTagExact(AbilityTag))
		{
			continue;
		}

		if (UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance())
		{
			return PrimaryInstance;
		}

		const TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();
		if (AbilityInstances.Num() > 0)
		{
			return AbilityInstances[0];
		}
	}

	return nullptr;
}

void UActionAbilitySystemComponent::GetHeroCombatAbilityRelationshipAuditLines(TArray<FString>& OutAuditLines) const
{
	OutAuditLines.Reset();

	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetAvatarActor());
	const UActionCombatReactComponent* CombatReactComponent =
		OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;

	for (const FGameplayTag& AbilityIdentityTag : GetHeroCombatAbilityAuditTags())
	{
		bool bFoundAnyGrantedAbility = false;

		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(AbilitySpec);
			if (!ActionAbility || !ActionAbility->GetAbilityIdentityTags().HasTagExact(AbilityIdentityTag))
			{
				continue;
			}

			bFoundAnyGrantedAbility = true;
			OutAuditLines.Add(BuildHeroCombatAbilityRelationshipAuditLine(AbilitySpec, ActionAbility, CombatReactComponent));
		}

		if (!bFoundAnyGrantedAbility)
		{
			OutAuditLines.Add(FString::Printf(
				TEXT("[GA Audit] Ability=%s Granted=否"),
				*AbilityIdentityTag.ToString()));
		}
	}
}

FString UActionAbilitySystemComponent::BuildHeroCombatAbilityRelationshipAuditLine(
	const FGameplayAbilitySpec& AbilitySpec,
	const UActionGameplayAbilityBase* Ability,
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (!Ability)
	{
		return TEXT("[GA Audit] Ability=Unknown Granted=否");
	}

	return FString::Printf(
		TEXT("[GA Audit] Input=%s Class=%s Ability=%s Priority=%d Active=%s InterruptLower=%s InterruptSame=%s BeInterruptedHigher=%s BeInterruptedSame=%s Owned=%s Blocked=%s Cancel=%s Activation=%s Interrupt=%s"),
		*DescribeGameplayTagsForDebug(AbilitySpec.DynamicAbilityTags),
		*GetNameSafe(AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr),
		*DescribeGameplayTagsForDebug(Ability->GetAbilityIdentityTags()),
		Ability->GetAbilityPriority(),
		*AbilitySystemComponentBoolToDebugText(AbilitySpec.IsActive()),
		*AbilitySystemComponentBoolToDebugText(Ability->CanInterruptLowerPriorityAbilities()),
		*AbilitySystemComponentBoolToDebugText(Ability->CanInterruptSamePriorityAbilities()),
		*AbilitySystemComponentBoolToDebugText(Ability->CanBeInterruptedByHigherPriority()),
		*AbilitySystemComponentBoolToDebugText(Ability->CanBeInterruptedBySamePriority()),
		*DescribeGameplayTagsForDebug(Ability->GetActivationOwnedTagsForRelationship()),
		*DescribeGameplayTagsForDebug(Ability->GetActivationBlockedTagsForRelationship()),
		*DescribeGameplayTagsForDebug(Ability->GetCancelAbilitiesWithTagsForRelationship()),
		*Ability->DescribeCombatReactActivationDecision(CombatReactComponent),
		*Ability->DescribeCombatReactInterruptDecision(CombatReactComponent));
}

UDataAsset_StatusEffectDefinition* UActionAbilitySystemComponent::FindStatusEffectDefinitionByTag(
	const FGameplayTag StatusEffectTag) const
{
	if (!StatusEffectTag.IsValid())
	{
		return nullptr;
	}

	// 状态效果定义是 ASC 侧展示与调试的配置映射表。
	// 这里只做标签到定义资产的查询，不把显示层语义写死在 GameplayEffect 里。
	for (const UDataAsset_StatusEffectDefinition* Definition : StatusEffectDefinitions)
	{
		if (Definition && Definition->GetStatusEffectTag() == StatusEffectTag)
		{
			return const_cast<UDataAsset_StatusEffectDefinition*>(Definition);
		}
	}

	return nullptr;
}

FActiveGameplayEffectHandle UActionAbilitySystemComponent::ApplyCombatModifierEffect(
	const FActionCombatModifierEffectSpec& EffectSpec)
{
	return ApplyRuntimeCombatModifierEffect(CombatModifierGameplayEffectClass, EffectSpec);
}

FActiveGameplayEffectHandle UActionAbilitySystemComponent::ApplyExecutionProtectionEffect(
	const FActionCombatModifierEffectSpec& EffectSpec)
{
	return ApplyRuntimeCombatModifierEffect(ExecutionProtectionGameplayEffectClass, EffectSpec);
}

FActiveGameplayEffectHandle UActionAbilitySystemComponent::ApplyRuntimeCombatModifierEffect(
	const TSubclassOf<UGameplayEffect>& GameplayEffectClass,
	const FActionCombatModifierEffectSpec& EffectSpec)
{
	if (!EffectSpec.IsValidSpec() || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActor());

	FGameplayEffectSpecHandle EffectSpecHandle =
		MakeOutgoingSpec(GameplayEffectClass, 1.f, EffectContext);
	if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpec& GameplayEffectSpec = *EffectSpecHandle.Data.Get();
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Effect_Duration,
		FMath::Max(EffectSpec.Duration, 0.f));
	ApplyCombatModifierSetByCallerValues(GameplayEffectSpec, EffectSpec);
	AddStatusEffectTagsToSpec(GameplayEffectSpec, EffectSpec.StatusEffectTag);
	GameplayEffectSpec.DynamicGrantedTags.AppendTags(EffectSpec.GrantedTags);

	return ApplyGameplayEffectSpecToSelf(GameplayEffectSpec);
}

void UActionAbilitySystemComponent::GetActiveStatusEffectInfos(
	TArray<FActionActiveStatusEffectInfo>& OutStatusEffects) const
{
	OutStatusEffects.Reset();

	if (!GetWorld())
	{
		return;
	}

	// 这里把运行中的 GameplayEffect 快照整理成 UI / 调试可直接消费的状态效果信息。
	// 正式来源仍然是当前 ASC 上的 Active Gameplay Effects，
	// DataAsset_StatusEffectDefinition 只负责把标签补成可读显示数据。
	const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles = GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle ActiveEffectHandle : ActiveEffectHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(ActiveEffectHandle);
		if (!ActiveEffect)
		{
			continue;
		}

		FGameplayTagContainer EffectAssetTags;
		ActiveEffect->Spec.GetAllAssetTags(EffectAssetTags);

		FGameplayTag ResolvedStatusEffectTag;
		for (const FGameplayTag& EffectTag : EffectAssetTags)
		{
			if (EffectTag != ActionGameplayTags::StatusEffect
				&& EffectTag.MatchesTag(ActionGameplayTags::StatusEffect))
			{
				ResolvedStatusEffectTag = EffectTag;
				break;
			}
		}

		if (!ResolvedStatusEffectTag.IsValid())
		{
			continue;
		}

		FActionActiveStatusEffectInfo& StatusEffectInfo = OutStatusEffects.AddDefaulted_GetRef();
		StatusEffectInfo.StatusEffectTag = ResolvedStatusEffectTag;
		StatusEffectInfo.StackCount = GetCurrentStackCount(ActiveEffectHandle);
		StatusEffectInfo.ActiveEffectHandle = ActiveEffectHandle;

		float EffectStartTime = 0.f;
		float EffectDuration = 0.f;
		GetGameplayEffectStartTimeAndDuration(ActiveEffectHandle, EffectStartTime, EffectDuration);
		StatusEffectInfo.TotalDuration = EffectDuration;
		StatusEffectInfo.RemainingTime =
			EffectDuration < 0.f
				? EffectDuration
				: FMath::Max((EffectStartTime + EffectDuration) - GetWorld()->GetTimeSeconds(), 0.f);

		if (const UDataAsset_StatusEffectDefinition* Definition = FindStatusEffectDefinitionByTag(ResolvedStatusEffectTag))
		{
			const FActionStatusEffectDisplayData& DisplayData = Definition->GetDisplayData();
			StatusEffectInfo.DisplayName = DisplayData.DisplayName;
			StatusEffectInfo.Description = DisplayData.Description;
			StatusEffectInfo.DisplayColor = DisplayData.DisplayColor;
			StatusEffectInfo.Icon = DisplayData.Icon;
		}
		else
		{
			// 即便暂时没有配置展示定义，也保留标签名作为兜底显示，
			// 避免 UI 或调试列表因为缺少文案资产而直接丢失这条运行时效果。
			StatusEffectInfo.DisplayName = FText::FromName(ResolvedStatusEffectTag.GetTagName());
		}
	}
}

bool UActionAbilitySystemComponent::HasActiveStatusEffectTag(const FGameplayTag StatusEffectTag) const
{
	if (!StatusEffectTag.IsValid())
	{
		return false;
	}

	const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles = GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle ActiveEffectHandle : ActiveEffectHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(ActiveEffectHandle);
		if (!ActiveEffect)
		{
			continue;
		}

		FGameplayTagContainer EffectAssetTags;
		ActiveEffect->Spec.GetAllAssetTags(EffectAssetTags);
		if (EffectAssetTags.HasTagExact(StatusEffectTag))
		{
			return true;
		}
	}

	return false;
}

void UActionAbilitySystemComponent::AddStatusEffectTagsToSpec(
	FGameplayEffectSpec& GameplayEffectSpec,
	const FGameplayTag& StatusEffectTag) const
{
	if (!StatusEffectTag.IsValid())
	{
		return;
	}

	GameplayEffectSpec.AddDynamicAssetTag(ActionGameplayTags::StatusEffect);
	GameplayEffectSpec.AddDynamicAssetTag(StatusEffectTag);
	GameplayEffectSpec.DynamicGrantedTags.AddTag(ActionGameplayTags::StatusEffect);
	GameplayEffectSpec.DynamicGrantedTags.AddTag(StatusEffectTag);
}

void UActionAbilitySystemComponent::ApplyCombatModifierSetByCallerValues(
	FGameplayEffectSpec& GameplayEffectSpec,
	const FActionCombatModifierEffectSpec& EffectSpec) const
{
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Attribute_DamageVulnerability,
		ActionRoundRatioValueToFourPlaces(EffectSpec.DamageVulnerabilityDelta));
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Attribute_HealthDamageResistance,
		ActionRoundRatioValueToFourPlaces(EffectSpec.HealthDamageResistanceDelta));
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Attribute_GuardStaminaCostResistance,
		ActionRoundRatioValueToFourPlaces(EffectSpec.GuardStaminaCostResistanceDelta));
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Attribute_PoiseDamageResistance,
		ActionRoundRatioValueToFourPlaces(EffectSpec.PoiseDamageResistanceDelta));
	GameplayEffectSpec.SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Attribute_ExecutionDamageMultiplier,
		ActionRoundRatioValueToFourPlaces(EffectSpec.ExecutionDamageMultiplierDelta));
}

void UActionAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		// 闂傚倷娴囧畷鐢稿窗閹扮増鍋￠弶鍫氭櫇娑撳秹鏌熸潏鍓хシ濞?InputPressed 濠电姷鏁搁崑娑㈡偤閵娧冨灊鐎光偓閸曞灚鏅為梺鍛婃处閸嬧偓闁哄閰ｉ弻鏇＄疀鐎ｎ亖鍋撻弴锛勭闂傚倷鑳堕…鍫ュ嫉椤掑倹宕查柛鏇ㄥ幘娑撳秶鎲搁悧鍫濈瑲闁抽攱鍨块弻鐔虹磼濡櫣鐟ㄥ銈冨妽閻熝呮閹烘嚦鏃堝焵椤掑媻鍥箥椤斿墽鐓撴繝銏ｆ硾閺堫剛鈧碍宀搁弻鐔虹磼濡椽鍞跺┑掳鍊愰崑鎾淬亜椤撯剝纭堕柟鐟板閹煎綊宕滈幇鈺佺伈闁哄本鐩崺锟犲磼閵堝棛鏉归梻浣芥〃缁€浣虹矓閹绢喖鐓″璺侯煬濞笺劑鏌涢埄鍏╂垿骞冨▎鎾粹拻濞达絽鎲￠幆鍫熴亜閹存繃顥犻柛鎺撳笒椤撳ジ宕崘顓涘亾閸喓鈧帒顫濋敐鍛闂備浇顕栭崹鎵偓姘嵆閻涱喚鈧綆浜栭弨浠嬫煕閵夋垵鍟ˉ鎾绘⒒閸屾瑧顦﹂柛姘儏椤灝顫滈埀顒勭嵁韫囨稑绠ｉ柣妯诲墯濞肩喎鈹戦悩缁樻锭妞ゆ垵鎳橀幆灞轿旀担鍏哥盎闂佸搫绉查崝搴ｇ不閹惧绠鹃柛顐ｇ矊瀹撳棝鏌熼鎸庣【闁宠棄顦灒缂備焦蓱鐎氬啿鈹戦悩娈挎毌闁逞屽墲濞呮洟宕悙鐢电＜闁稿本姘ㄨ倴闂侀€炲苯澧剧紒鍙夋そ瀵彃顭ㄩ崼顐ｆ櫇婵炲濮撮鍡涙偂閺囩喐鍙忔慨妤€妫楁晶顖涚節閳ь剟鎮ч崼娑楃盎闂佸湱鍎ら弻銊︾鏉堚斁鍋撶憴鍕鐎光偓閹间礁绠栫憸鏂跨暦婵傚憡鍋勯柧蹇氼潐濠㈡垿姊婚崒娆戭槮闁硅绻濆濠氭晸閻樿尙鍔﹀銈嗗笒閸燁垶鎮甸鍫熺厽婵犻潧瀚埀顒佺箞瀵?
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
	}
}

void UActionAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		// 闂傚倷娴囧畷鐢稿窗閹扮増鍋￠弶鍫氭櫇娑撳秹鏌熸潏鍓хシ濞?InputReleased 濠电姷鏁搁崑娑㈡偤閵娧冨灊鐎光偓閸曞灚鏅為梺鍛婃处閸嬧偓闁哄閰ｉ弻鏇＄疀鐎ｎ亖鍋撻弴锛勭闂傚倷鑳堕…鍫ュ嫉椤掑倹宕查柛鏇ㄥ幘娑撳秶鎲搁悧鍫濈瑲闁抽攱鍨块弻鐔虹磼濡櫣鐟ㄥ銈冨妽閻熝呮閹烘嚦鏃堝焵椤掑媻鍥箥椤斿墽鐓撴繝銏ｆ硾閺堫剛鈧碍宀搁弻鐔虹磼濡椽鍞跺┑掳鍊愰崑鎾淬亜椤撯剝纭堕柟鐟板閹煎綊宕滈幇鈺佺伈闁哄本鐩崺锟犲磼閵堝棛鏉归梻浣芥〃缁€浣虹矓閹绢喖鐓″璺侯煬濞笺劑鏌涢埄鍏╂垿骞冨▎鎾粹拻濞达絽鎲￠幆鍫熴亜閹存繃顥犻柛鎺撳笒椤撳ジ宕崘顓涘亾閸喓鈧帒顫濋敐鍛闂備浇顕栭崹鎵偓姘嵆閻涱喚鈧綆浜栭弨浠嬫煕閵夋垵鍟ˉ鎾绘⒒閸屾瑧顦﹂柛姘儏椤灝顫滈埀顒勭嵁韫囨稑绠ｉ柣妯诲墯濞肩喎鈹戦悩缁樻锭妞ゆ垵鎳橀幆灞轿旀担鍏哥盎闂佸搫绉查崝搴ｇ不閹惧绠鹃柛顐ｇ矊瀹撳棝鏌熼鎸庣【闁宠棄顦灒缂備焦蓱鐎氬啿鈹戦悩娈挎毌闁逞屽墲濞呮洟宕悙鐢电＜闁稿本姘ㄨ倴闂侀€炲苯澧剧紒鍙夋そ瀵彃顭ㄩ崼顐ｆ櫇婵炲濮撮鍡涙偂閺囩喐鍙忔慨妤€妫楁晶顖涚節閳ь剟鎮ч崼娑楃盎闂佸湱鍎ら弻銊︾鏉堚斁鍋撶憴鍕鐎光偓閹间礁绠栫憸鏂跨暦婵傚憡鍋勯柧蹇氼潐濠㈡垿姊婚崒娆戭槮闁硅绻濆濠氭晸閻樿尙鍔﹀銈嗗笒閸燁垶鎮甸鍫熺厽婵犻潧瀚埀顒佺箞瀵?
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
	}
}
