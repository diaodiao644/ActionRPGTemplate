// 文件说明：实现 AbilitySystemComponent 侧的战斗输入转发、关系裁决与调试辅助。

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/ActionAbilityCategoryValidation.h"
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
	constexpr int32 MaxRelationshipActivationFailureHistoryEntries = 16;

	FString DescribeGameplayTagsForDebug(const FGameplayTagContainer& InTags)
	{
		return DescribeActionAbilityValidationGameplayTags(InTags);
	}

	FString DescribeGameplayTagsForDebug(const TArray<FGameplayTag>& InTags)
	{
		return DescribeActionAbilityValidationGameplayTags(InTags);
	}

	FString AbilitySystemComponentBoolToDebugText(const bool bValue)
	{
		return bValue ? TEXT("是") : TEXT("否");
	}

	FString DescribeNetRoleForDebug(const ENetRole InRole)
	{
		if (const UEnum* NetRoleEnum = StaticEnum<ENetRole>())
		{
			return NetRoleEnum->GetNameStringByValue(static_cast<int64>(InRole));
		}

		return TEXT("UnknownRole");
	}

	const UActionCombatReactComponent* ResolveCombatReactComponentFromActorInfo(
		const FGameplayAbilityActorInfo* ActorInfo)
	{
		const AActionCharacterBase* OwnerCharacter =
			ActorInfo ? Cast<AActionCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
		return OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;
	}

	ENetRole ResolveActivationNetRoleFromActorInfo(const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (const APlayerController* PlayerController = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr)
		{
			return PlayerController->GetLocalRole();
		}

		if (const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
		{
			return AvatarActor->GetLocalRole();
		}

		return ROLE_SimulatedProxy;
	}

	bool IsExpectedNonAttackGateBlockReason(const FString& InFailureReason)
	{
		return InFailureReason.Contains(TEXT("layer=WeaponSwitchPresentation"))
			|| InFailureReason.Contains(TEXT("layer=WeaponSwitchChainWindow"));
	}

	const TArray<FGameplayTag>& GetHeroCombatAbilityAuditTags()
	{
		return GetActionAbilityTopLevelIdentityTags();
	}

	FString DescribeHeroCombatRelationshipAbilityShapeProbeSummary()
	{
		return TEXT("HeroCombatGrantShape(BlueprintCanActivateOverride, OnGivenActivationPolicy, NetExecutionPolicy, ReplicationPolicy, InstancingPolicy, RetriggerInstancedAbility, SourceOrTargetActivationTagRequirements, CategoryAudit, MatrixRule)");
	}

	FString DescribeRelationshipPostCancelKnownProbeSummary()
	{
		return FString::Printf(
			TEXT("CandidateSpec, CandidateAbilityObject, ActorInfo, AvatarOrNetRole, HeroAvatarType, %s, OwnerTagGate, OwnedTagSnapshot, Resource, CombatReact, UserActivationInhibited, AbilityTagsBlocked, InputBlocked"),
			*DescribeHeroCombatRelationshipAbilityShapeProbeSummary());
	}

	FString DescribeRelationshipFinalTryKnownProbeSummary()
	{
		return FString::Printf(
			TEXT("%s, FinalTryFailureTagsReclassify, CandidateSpecOnFinalTry, FinalTryActorInfo, FinalTryExecutionContext, FinalTryInstancedPerActorAlreadyActive, FinalTryMissingPrimaryInstance"),
			*DescribeRelationshipPostCancelKnownProbeSummary());
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

	const FActionAbilityRelationshipMatrix& GetHeroCombatAbilityRelationshipMatrix()
	{
		static const FActionAbilityRelationshipMatrix Matrix =
			FActionAbilityRelationshipMatrix::BuildDefaultHeroCombatMatrix();
		return Matrix;
	}

	const FActionAbilityCategoryRelationshipRule* FindCategoryRelationshipRule(
		const EActionAbilityCategory AbilityCategory)
	{
		return GetHeroCombatAbilityRelationshipMatrix().FindRule(AbilityCategory);
	}

	FString DescribeAbilityCategoriesForDebug(const TArray<EActionAbilityCategory>& AbilityCategories)
	{
		TArray<FString> CategoryNames;
		CategoryNames.Reserve(AbilityCategories.Num());

		for (const EActionAbilityCategory AbilityCategory : AbilityCategories)
		{
			CategoryNames.Add(ActionAbilityCategoryToString(AbilityCategory));
		}

		return CategoryNames.Num() > 0
			? FString::Join(CategoryNames, TEXT(", "))
			: TEXT("None");
	}

	bool IsHeroCombatAbilityRelevantForAudit(
		const FGameplayAbilitySpec& AbilitySpec,
		const UActionGameplayAbilityBase* ActionAbility)
	{
		return AbilitySpec.Ability != nullptr
			&& ActionAbility != nullptr
			&& ActionAbility->UsesAbilityRelationshipSystem()
			&& Cast<UActionHeroGameplayAbility>(AbilitySpec.Ability) != nullptr;
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

	const UGameplayAbility* GrantedAbilityCDO = InGrantedAbilities.GetDefaultObject();
	FString GrantValidationFailureReason;
	FString GrantValidationAuditState;
	if (!ValidateGrantedHeroCombatAbilityCategoryBeforeGrant(
		GrantedAbilityCDO,
		AbilityTag,
		GrantValidationFailureReason,
		GrantValidationAuditState))
	{
		const FString FailureText = BuildGrantAbilityCategoryValidationFailureText(
			GrantedAbilityCDO,
			AbilityTag,
			GrantValidationFailureReason,
			GrantValidationAuditState);
		UE_LOG(ActionRPG, Error, TEXT("%s"), *FailureText);
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
	FGameplayTagContainer OwnedTagsBeforeCancel;
	FGameplayTagContainer PredictedOwnedTagsAfterCancel;
	FGameplayTagContainer PredictedReleasedTags;

	if (!CheckAbilityResourcePreconditionsBeforeRelationshipCancel(Spec, CandidateAbility, FailureReason))
	{
		if (!FailureReason.IsEmpty())
		{
			const FString BlockedDebugText =
				BuildRelationshipBlockedDebugText(CandidateAbility, CombatReactComponent, FailureReason);
			Debug::Print(BlockedDebugText, FColor::Orange, 2.5f);
			UE_LOG(ActionRPG, Warning, TEXT("%s"), *BlockedDebugText);
		}

		return false;
	}

	// 正式激活前，先把“关系上是否允许起手”和“需要取消谁”一次性算清楚。
	// 资源预检已经在前一阶段完成；这里不再混做 cooldown / cost / commit 判定。
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

	GetOwnedGameplayTags(OwnedTagsBeforeCancel);
	// 这两份快照只服务“cancel 前预测”和“cancel 后残余归类”：
	// 1. PredictedOwnedTagsAfterCancel 用来回答 owner-side 最终标签门在预测视图下是否已知会失败；
	// 2. PredictedReleasedTags 用来把这次预计释放掉哪些标签解释进日志。
	// 它们都不是新的正式状态源，也不会替代 cancel 后的真实 owned tags。
	BuildPredictedOwnedTagsAfterRelationshipCancel(
		AbilityHandlesToCancel,
		PredictedOwnedTagsAfterCancel,
		PredictedReleasedTags);

	// 只把当前已经能稳定确定、且不会产生副作用的最终 gate 前移到 cancel 前。
	// 这里故意不去猜那些必须真实走到 TryActivateAbility(...) 才会暴露的引擎尾部门。
	if (!CheckPredictiveFinalAbilityActivationPreconditionsBeforeRelationshipCancel(
		Spec,
		CandidateAbility,
		AbilityHandlesToCancel,
		FailureReason))
	{
		if (!FailureReason.IsEmpty())
		{
			const FString BlockedDebugText =
				BuildRelationshipBlockedDebugText(CandidateAbility, CombatReactComponent, FailureReason);
			Debug::Print(BlockedDebugText, FColor::Orange, 2.5f);
			UE_LOG(ActionRPG, Warning, TEXT("%s"), *BlockedDebugText);
		}

		return false;
	}

	// 关系裁决和稳定预测都已通过后，才正式取消旧 GA。
	// 当前主链坚持“先尽量预测，再单向 cancel 并继续前推”，不在这里做事务式回滚。
	for (const FGameplayAbilitySpecHandle AbilityHandleToCancel : AbilityHandlesToCancel)
	{
		if (AbilityHandleToCancel.IsValid())
		{
			CancelAbilityHandle(AbilityHandleToCancel);
		}
	}

	FGameplayTagContainer FinalActivationFailureTags;
	// 旧 GA 已经取消后，再用候选 Ability 自己完整的正式激活门做一轮最终无副作用预检。
	// 这里若失败，说明问题已经进入 post-cancel residual，而不是普通关系拒绝。
	if (!CheckFinalAbilityActivationPreconditionsAfterRelationshipCancel(
			Spec,
			CandidateAbility,
			FailureReason,
			&FinalActivationFailureTags))
	{
		if (!FailureReason.IsEmpty())
		{
			FActionAbilityRelationshipFailureDiagnostic FailureDiagnostic =
				BuildRelationshipActivationFailureDiagnosticForPostCancelFinalPrecheck(
				Spec,
				CandidateAbility,
				CombatReactComponent,
				AbilityHandlesToCancel,
				OwnedTagsBeforeCancel,
				PredictedOwnedTagsAfterCancel,
				PredictedReleasedTags,
				FinalActivationFailureTags,
				FailureReason);
			RecordRelationshipActivationFailureDiagnostic(FailureDiagnostic);

			const FString FailureDebugText =
				BuildRelationshipActivationFailureDebugText(LastRelationshipActivationFailureDiagnostic);
			Debug::Print(FailureDebugText, FColor::Orange, 2.5f);
			UE_LOG(ActionRPG, Warning, TEXT("%s"), *FailureDebugText);
		}

		return false;
	}

	// 只有走到这里才交给 GAS 做最终 TryActivateAbility。
	// 若此处仍失败，说明 residual 之外还存在 final tail failure，需要单独走 final try 归类。
	const bool bActivated = TryActivateAbility(Spec.Handle);
	if (!bActivated)
	{
		FGameplayTagContainer FinalTryFailureTags = InternalTryActivateAbilityFailureTags;
		FailureReason = TEXT("final TryActivateAbility returned false after post-cancel final activation precheck passed.");
		FActionAbilityRelationshipFailureDiagnostic FailureDiagnostic =
			BuildRelationshipActivationFailureDiagnosticForFinalTryActivate(
				Spec,
				CandidateAbility,
				CombatReactComponent,
				AbilityHandlesToCancel,
				OwnedTagsBeforeCancel,
				PredictedOwnedTagsAfterCancel,
				PredictedReleasedTags,
				FinalTryFailureTags,
				FailureReason);
		RecordRelationshipActivationFailureDiagnostic(FailureDiagnostic);

		const FString FailureDebugText =
			BuildRelationshipActivationFailureDebugText(LastRelationshipActivationFailureDiagnostic);
		Debug::Print(FailureDebugText, FColor::Orange, 2.5f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *FailureDebugText);
	}
	else
	{
		ClearLastRelationshipActivationFailureDiagnosticIfSupersededBySuccessfulActivation(Spec.Handle);
	}

	return bActivated;
}

bool UActionAbilitySystemComponent::CheckAbilityResourcePreconditionsBeforeRelationshipCancel(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	if (!CandidateAbility)
	{
		return true;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo)
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel resource precheck failed because actor info is invalid. Candidate=%s"),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	FGameplayTagContainer PrecheckFailureTags;
	if (!CandidateAbility->CheckPreCancelResourcePreconditions(
			CandidateSpec.Handle,
			ActorInfo,
			&PrecheckFailureTags))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel resource precheck failed. Candidate=%s FailureTags=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*DescribeGameplayTagsForDebug(PrecheckFailureTags));
		return false;
	}

	return true;
}

bool UActionAbilitySystemComponent::CheckPredictiveFinalAbilityActivationPreconditionsBeforeRelationshipCancel(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	(void)CandidateSpec;

	if (!CandidateAbility)
	{
		return true;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo)
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because actor info is invalid. Candidate=%s"),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	// 先用“预测取消后”的 owner tags 视图复跑候选 Ability 自己的最终标签门。
	// 这里只有 owner-side 标签门可以被稳定前移，因为它不需要真的进入 TryActivateAbility(...)。
	FGameplayTagContainer PredictedOwnedTags;
	FGameplayTagContainer PredictedReleasedTags;
	BuildPredictedOwnedTagsAfterRelationshipCancel(
		AbilityHandlesToCancel,
		PredictedOwnedTags,
		PredictedReleasedTags);

	FGameplayTagContainer PredictiveFailureTags;
	FString PredictiveFailureDetails;
	if (!CandidateAbility->CheckPredictiveFinalActivationTagGateAgainstOwnedTags(
			PredictedOwnedTags,
			PredictiveFailureDetails,
			&PredictiveFailureTags))
	{
		TArray<FString> CancelAbilityNames;
		CancelAbilityNames.Reserve(AbilityHandlesToCancel.Num());
		for (const FGameplayAbilitySpecHandle AbilityHandleToCancel : AbilityHandlesToCancel)
		{
			const FGameplayAbilitySpec* AbilitySpecToCancel =
				FindAbilitySpecFromHandle(AbilityHandleToCancel);
			const UActionGameplayAbilityBase* AbilityToCancel =
				AbilitySpecToCancel ? ResolveActionAbilityFromSpec(*AbilitySpecToCancel) : nullptr;
			CancelAbilityNames.Add(
				AbilityToCancel
					? AbilityToCancel->GetAbilityDebugName()
					: TEXT("UnknownAbility"));
		}

		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed. Candidate=%s AbilitiesToCancel=%s PredictedReleasedTags=%s PredictedFailureTags=%s Details=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			CancelAbilityNames.Num() > 0 ? *FString::Join(CancelAbilityNames, TEXT(", ")) : TEXT("None"),
			*DescribeGameplayTagsForDebug(PredictedReleasedTags),
			*DescribeGameplayTagsForDebug(PredictiveFailureTags),
			PredictiveFailureDetails.IsEmpty() ? TEXT("None") : *PredictiveFailureDetails);
		return false;
	}

	const UActionCombatReactComponent* CombatReactComponent =
		ResolveCombatReactComponentFromActorInfo(ActorInfo);
	// 继续补共享稳定 gate：
	// 这些门现在失败，取消旧 GA 也不会让它们自己变好，因此应该尽量前移到 cancel 前直接挡下。
	if (!CheckPredictiveSharedFinalActivationPreconditionsBeforeRelationshipCancel(
			CandidateSpec,
			CandidateAbility,
			CombatReactComponent,
			OutFailureReason))
	{
		return false;
	}

	return true;
}

bool UActionAbilitySystemComponent::CheckPredictiveSharedFinalActivationPreconditionsBeforeRelationshipCancel(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	if (!CandidateAbility)
	{
		return true;
	}

	// 这里只收共享稳定 gate，不收需要真实 final TryActivate 才会暴露的尾部门。
	// 这样可以减少“取消后才发现一定起不来”的样本，但不会制造伪预测。
	EActionAbilityRelationshipFailureKind UnsupportedShapeFailureKind =
		EActionAbilityRelationshipFailureKind::None;
	FString UnsupportedShapeFailureReason;
	if (TryClassifyUnsupportedHeroCombatRelationshipAbilityShape(
			CandidateAbility,
			UnsupportedShapeFailureKind,
			UnsupportedShapeFailureReason))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because the candidate ability shape is outside the Hero combat relationship whitelist. Candidate=%s FailureKind=%s Details=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*DescribeRelationshipActivationFailureKind(UnsupportedShapeFailureKind),
			UnsupportedShapeFailureReason.IsEmpty() ? TEXT("None") : *UnsupportedShapeFailureReason);
		return false;
	}

	if (!CandidateSpec.Handle.IsValid() || !FindAbilitySpecFromHandle(CandidateSpec.Handle))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because candidate spec no longer resolves. Candidate=%s Handle=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*CandidateSpec.Handle.ToString());
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because actor info is invalid. Candidate=%s"),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	const AActor* AvatarActorInstance = ActorInfo->AvatarActor.Get();
	if (!AvatarActorInstance || !CandidateAbility->ShouldActivateAbility(AvatarActorInstance->GetLocalRole()))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because avatar/net role is invalid. Candidate=%s AvatarActorValid=%s LocalRole=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*AbilitySystemComponentBoolToDebugText(AvatarActorInstance != nullptr),
			AvatarActorInstance ? *DescribeNetRoleForDebug(AvatarActorInstance->GetLocalRole()) : TEXT("None"));
		return false;
	}

	if (Cast<const UActionHeroGameplayAbility>(CandidateAbility)
		&& !Cast<const AActionHeroCharacter>(AvatarActorInstance))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because hero avatar type is invalid. Candidate=%s AvatarActorClass=%s Expected=AActionHeroCharacter LocalRole=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*GetNameSafe(AvatarActorInstance->GetClass()),
			*DescribeNetRoleForDebug(AvatarActorInstance->GetLocalRole()));
		return false;
	}

	if (!CandidateAbility->IsActivationAllowedByCombatReact(CombatReactComponent))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because combat react currently blocks activation. Candidate=%s Activation=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*CandidateAbility->DescribeCombatReactActivationDecision(CombatReactComponent));
		return false;
	}

	if (GetUserAbilityActivationInhibited())
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because user ability activation is inhibited. Candidate=%s GetUserAbilityActivationInhibited()=true"),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	if (AreAbilityTagsBlocked(CandidateAbility->GetAbilityIdentityTags()))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because ability tags are globally blocked. Candidate=%s AbilityTags=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*DescribeGameplayTagsForDebug(CandidateAbility->GetAbilityIdentityTags()));
		return false;
	}

	if (IsAbilityInputBlocked(CandidateSpec.InputID))
	{
		OutFailureReason = FString::Printf(
			TEXT("pre-cancel predictive final activation gate failed because ability input is blocked. Candidate=%s InputID=%d"),
			*CandidateAbility->GetAbilityDebugName(),
			CandidateSpec.InputID);
		return false;
	}

	return true;
}

bool UActionAbilitySystemComponent::TryClassifyUnsupportedHeroCombatRelationshipAbilityShape(
	const UGameplayAbility* AbilityToInspect,
	EActionAbilityRelationshipFailureKind& OutFailureKind,
	FString& OutFailureReason) const
{
	OutFailureKind = EActionAbilityRelationshipFailureKind::None;
	OutFailureReason.Reset();

	if (!AbilityToInspect)
	{
		return false;
	}

	const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
		BuildActionHeroCombatAbilityCategoryValidationResult(AbilityToInspect);
	if (!ValidationResult.bIsHeroCombatRelationshipAbility || ValidationResult.bPassed)
	{
		return false;
	}

	auto SetFailure = [&OutFailureKind, &OutFailureReason, &ValidationResult](
		const EActionAbilityRelationshipFailureKind FailureKind)
	{
		OutFailureKind = FailureKind;
		OutFailureReason = FString::Printf(
			TEXT("grant-time whitelist drift detected. AuditState=%s Reason=%s Identity=%s Category=%s"),
			ValidationResult.AuditState.IsEmpty() ? TEXT("None") : *ValidationResult.AuditState,
			ValidationResult.FailureReason.IsEmpty() ? TEXT("None") : *ValidationResult.FailureReason,
			*DescribeGameplayTagsForDebug(ValidationResult.IdentityTags),
			*ActionAbilityCategoryToString(ValidationResult.AbilityCategory));
		return true;
	};

	if (ValidationResult.bHasBlueprintCanActivateOverride)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedBlueprintCanActivateOverrideDrift);
	}

	if (ValidationResult.bUsesOnGivenActivationPolicy)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedOnGivenActivationPolicyDrift);
	}

	if (!ValidationResult.bSupportedNetExecutionPolicy)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedNetExecutionPolicyDrift);
	}

	if (!ValidationResult.bSupportedReplicationPolicy)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedReplicationPolicyDrift);
	}

	if (!ValidationResult.bSupportedInstancingPolicy)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedInstancingPolicyDrift);
	}

	if (ValidationResult.bUsesRetriggerInstancedAbility)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedRetriggerInstancedAbilityDrift);
	}

	if (ValidationResult.bHasSourceOrTargetTagRequirements)
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedSourceOrTargetTagRequirementsDrift);
	}

	if (!ValidationResult.bMatrixRuleValid
		|| ValidationResult.AuditState != TEXT("OK"))
	{
		return SetFailure(
			EActionAbilityRelationshipFailureKind::UnsupportedHeroCombatCategoryAuditDrift);
	}

	return false;
}

void UActionAbilitySystemComponent::BuildPredictedOwnedTagsAfterRelationshipCancel(
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
	FGameplayTagContainer& OutPredictedOwnedTags,
	FGameplayTagContainer& OutPredictedReleasedTags) const
{
	OutPredictedOwnedTags.Reset();
	OutPredictedReleasedTags.Reset();
	GetOwnedGameplayTags(OutPredictedOwnedTags);

	TMap<FGameplayTag, int32> ReleasedTagCounts;
	for (const FGameplayAbilitySpecHandle AbilityHandleToCancel : AbilityHandlesToCancel)
	{
		if (!AbilityHandleToCancel.IsValid())
		{
			continue;
		}

		const FGameplayAbilitySpec* AbilitySpecToCancel = FindAbilitySpecFromHandle(AbilityHandleToCancel);
		const UActionGameplayAbilityBase* AbilityToCancel =
			AbilitySpecToCancel ? ResolveActionAbilityFromSpec(*AbilitySpecToCancel) : nullptr;
		if (!AbilityToCancel)
		{
			continue;
		}

		FGameplayTagContainer PredictedReleasedTagsByAbility;
		AbilityToCancel->GetPredictedOwnedTagsToReleaseOnRelationshipCancel(
			PredictedReleasedTagsByAbility);
		OutPredictedReleasedTags.AppendTags(PredictedReleasedTagsByAbility);

		for (const FGameplayTag& ReleasedTag : PredictedReleasedTagsByAbility)
		{
			if (!ReleasedTag.IsValid())
			{
				continue;
			}

			int32& ReleasedTagCount = ReleasedTagCounts.FindOrAdd(ReleasedTag);
			++ReleasedTagCount;
		}
	}

	for (const TPair<FGameplayTag, int32>& ReleasedTagPair : ReleasedTagCounts)
	{
		if (!ReleasedTagPair.Key.IsValid())
		{
			continue;
		}

		const int32 CurrentTagCount = GetTagCount(ReleasedTagPair.Key);
		if (CurrentTagCount > 0 && CurrentTagCount <= ReleasedTagPair.Value)
		{
			OutPredictedOwnedTags.RemoveTag(ReleasedTagPair.Key);
		}
	}
}

bool UActionAbilitySystemComponent::CheckFinalAbilityActivationPreconditionsAfterRelationshipCancel(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	FString& OutFailureReason,
	FGameplayTagContainer* OutFailureTags) const
{
	OutFailureReason.Reset();
	if (OutFailureTags)
	{
		OutFailureTags->Reset();
	}

	if (!CandidateAbility)
	{
		return true;
	}

	// 旧 GA 已取消后，这里只调用候选 Ability 自己正式的 CanActivateAbility(...)
	// 来判断“现在是否仍有最终 gate 挡住它”。分类和审计在失败后单独做，不把解释逻辑混进正式预检。
	if (!CandidateSpec.Handle.IsValid() || !FindAbilitySpecFromHandle(CandidateSpec.Handle))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel final activation precheck failed because candidate spec no longer resolves. Candidate=%s Handle=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*CandidateSpec.Handle.ToString());
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo)
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel final activation precheck failed (unexpected drift) because actor info is invalid. Candidate=%s"),
			*CandidateAbility->GetAbilityDebugName());
		return false;
	}

	FGameplayTagContainer FinalActivationFailureTags;
	if (!CandidateAbility->CanActivateAbility(
			CandidateSpec.Handle,
			ActorInfo,
			nullptr,
			nullptr,
			&FinalActivationFailureTags))
	{
		if (OutFailureTags)
		{
			OutFailureTags->AppendTags(FinalActivationFailureTags);
		}

		OutFailureReason = FString::Printf(
			TEXT("post-cancel final activation precheck failed (unexpected drift). Candidate=%s FailureTags=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return false;
	}

	return true;
}

bool UActionAbilitySystemComponent::DoOwnedTagSnapshotsMatch(
	const FGameplayTagContainer& ExpectedOwnedTags,
	const FGameplayTagContainer& ActualOwnedTags) const
{
	return ExpectedOwnedTags.HasAllExact(ActualOwnedTags)
		&& ActualOwnedTags.HasAllExact(ExpectedOwnedTags);
}

void UActionAbilitySystemComponent::BuildAbilityDebugNamesForHandles(
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
	TArray<FString>& OutAbilityNames) const
{
	OutAbilityNames.Reset();
	OutAbilityNames.Reserve(AbilityHandles.Num());

	for (const FGameplayAbilitySpecHandle AbilityHandle : AbilityHandles)
	{
		if (!AbilityHandle.IsValid())
		{
			continue;
		}

		const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityHandle);
		const UActionGameplayAbilityBase* Ability = AbilitySpec ? ResolveActionAbilityFromSpec(*AbilitySpec) : nullptr;
		OutAbilityNames.Add(Ability ? Ability->GetAbilityDebugName() : TEXT("UnknownAbility"));
	}
}

FString UActionAbilitySystemComponent::DescribeRelationshipActivationFailureStage(
	const EActionAbilityRelationshipFailureStage FailureStage) const
{
	switch (FailureStage)
	{
	case EActionAbilityRelationshipFailureStage::PostCancelFinalPrecheck:
		return TEXT("PostCancelFinalPrecheck");

	case EActionAbilityRelationshipFailureStage::FinalTryActivate:
		return TEXT("FinalTryActivate");

	default:
		break;
	}

	return TEXT("None");
}

FString UActionAbilitySystemComponent::DescribeRelationshipActivationFailureKind(
	const EActionAbilityRelationshipFailureKind FailureKind) const
{
	switch (FailureKind)
	{
	case EActionAbilityRelationshipFailureKind::CandidateSpecMissingDrift:
		return TEXT("CandidateSpecMissingDrift");

	case EActionAbilityRelationshipFailureKind::CandidateAbilityObjectMissingDrift:
		return TEXT("CandidateAbilityObjectMissingDrift");

	case EActionAbilityRelationshipFailureKind::ActorInfoInvalid:
		return TEXT("ActorInfoInvalid");

	case EActionAbilityRelationshipFailureKind::AvatarOrNetRoleDrift:
		return TEXT("AvatarOrNetRoleDrift");

	case EActionAbilityRelationshipFailureKind::HeroAvatarTypeDrift:
		return TEXT("HeroAvatarTypeDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedBlueprintCanActivateOverrideDrift:
		return TEXT("UnsupportedBlueprintCanActivateOverrideDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedOnGivenActivationPolicyDrift:
		return TEXT("UnsupportedOnGivenActivationPolicyDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedNetExecutionPolicyDrift:
		return TEXT("UnsupportedNetExecutionPolicyDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedReplicationPolicyDrift:
		return TEXT("UnsupportedReplicationPolicyDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedInstancingPolicyDrift:
		return TEXT("UnsupportedInstancingPolicyDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedRetriggerInstancedAbilityDrift:
		return TEXT("UnsupportedRetriggerInstancedAbilityDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedSourceOrTargetTagRequirementsDrift:
		return TEXT("UnsupportedSourceOrTargetTagRequirementsDrift");

	case EActionAbilityRelationshipFailureKind::UnsupportedHeroCombatCategoryAuditDrift:
		return TEXT("UnsupportedHeroCombatCategoryAuditDrift");

	case EActionAbilityRelationshipFailureKind::ResidualOwnerTagGateDrift:
		return TEXT("ResidualOwnerTagGateDrift");

	case EActionAbilityRelationshipFailureKind::OwnedTagSnapshotDrift:
		return TEXT("OwnedTagSnapshotDrift");

	case EActionAbilityRelationshipFailureKind::ResourceDrift:
		return TEXT("ResourceDrift");

	case EActionAbilityRelationshipFailureKind::CombatReactDrift:
		return TEXT("CombatReactDrift");

	case EActionAbilityRelationshipFailureKind::UserActivationInhibitedDrift:
		return TEXT("UserActivationInhibitedDrift");

	case EActionAbilityRelationshipFailureKind::AbilityTagsBlockedDrift:
		return TEXT("AbilityTagsBlockedDrift");

	case EActionAbilityRelationshipFailureKind::InputBlockedDrift:
		return TEXT("InputBlockedDrift");

	case EActionAbilityRelationshipFailureKind::UnknownFinalGate:
		return TEXT("UnknownFinalGate");

	case EActionAbilityRelationshipFailureKind::CandidateSpecMissingOnFinalTry:
		return TEXT("CandidateSpecMissingOnFinalTry");

	case EActionAbilityRelationshipFailureKind::FinalTryActorInfoInvalid:
		return TEXT("FinalTryActorInfoInvalid");

	case EActionAbilityRelationshipFailureKind::FinalTryAbilityObjectMissing:
		return TEXT("FinalTryAbilityObjectMissing");

	case EActionAbilityRelationshipFailureKind::FinalTryExecutionContextRejected:
		return TEXT("FinalTryExecutionContextRejected");

	case EActionAbilityRelationshipFailureKind::FinalTryInstancedPerActorAlreadyActive:
		return TEXT("FinalTryInstancedPerActorAlreadyActive");

	case EActionAbilityRelationshipFailureKind::FinalTryMissingPrimaryInstance:
		return TEXT("FinalTryMissingPrimaryInstance");

	case EActionAbilityRelationshipFailureKind::FinalTryActivateUnknown:
		return TEXT("FinalTryActivateUnknown");

	default:
		break;
	}

	return TEXT("None");
}

EActionAbilityRelationshipFailureKind UActionAbilitySystemComponent::ClassifyPostCancelDriftKind(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
	const FGameplayTagContainer& OwnedTagsAfterCancel,
	const FGameplayTagContainer& FinalActivationFailureTags,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	// 这条 helper 只解释“post-cancel 才暴露”的 residual drift。
	// 它不会去解释 relationship resolve 之前的普通拒绝，也不会吞掉 final TryActivate 的专有尾门。
	if (!CandidateAbility)
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel candidate ability object no longer resolves. CandidateSpecClass=%s Handle=%s"),
			*GetNameSafe(CandidateSpec.Ability ? CandidateSpec.Ability->GetClass() : nullptr),
			*CandidateSpec.Handle.ToString());
		return EActionAbilityRelationshipFailureKind::CandidateAbilityObjectMissingDrift;
	}

	// 先解释“候选对象 / 宿主 / 能力形态”这些 cancel 后不该漂移、但当前已经漂移的稳定事实。
	if (!CandidateSpec.Handle.IsValid() || !FindAbilitySpecFromHandle(CandidateSpec.Handle))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel candidate spec no longer resolves. Candidate=%s Handle=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*CandidateSpec.Handle.ToString());
		return EActionAbilityRelationshipFailureKind::CandidateSpecMissingDrift;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel actor info became invalid before final activation gate could be explained. Candidate=%s"),
			*CandidateAbility->GetAbilityDebugName());
		return EActionAbilityRelationshipFailureKind::ActorInfoInvalid;
	}

	const AActor* AvatarActorInstance = ActorInfo->AvatarActor.Get();
	if (!AvatarActorInstance || !CandidateAbility->ShouldActivateAbility(AvatarActorInstance->GetLocalRole()))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel avatar/net role drift detected. AvatarActorValid=%s LocalRole=%s"),
			*AbilitySystemComponentBoolToDebugText(AvatarActorInstance != nullptr),
			AvatarActorInstance ? *DescribeNetRoleForDebug(AvatarActorInstance->GetLocalRole()) : TEXT("None"));
		return EActionAbilityRelationshipFailureKind::AvatarOrNetRoleDrift;
	}

	if (Cast<const UActionHeroGameplayAbility>(CandidateAbility)
		&& !Cast<const AActionHeroCharacter>(AvatarActorInstance))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel hero avatar type drift detected. Candidate=%s AvatarActorClass=%s Expected=AActionHeroCharacter LocalRole=%s"),
			*CandidateAbility->GetAbilityDebugName(),
			*GetNameSafe(AvatarActorInstance->GetClass()),
			*DescribeNetRoleForDebug(AvatarActorInstance->GetLocalRole()));
		return EActionAbilityRelationshipFailureKind::HeroAvatarTypeDrift;
	}

	EActionAbilityRelationshipFailureKind UnsupportedShapeFailureKind =
		EActionAbilityRelationshipFailureKind::None;
	FString UnsupportedShapeFailureReason;
	if (TryClassifyUnsupportedHeroCombatRelationshipAbilityShape(
			CandidateAbility,
			UnsupportedShapeFailureKind,
			UnsupportedShapeFailureReason))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel Hero combat relationship whitelist drift detected. FailureKind=%s Details=%s"),
			*DescribeRelationshipActivationFailureKind(UnsupportedShapeFailureKind),
			UnsupportedShapeFailureReason.IsEmpty() ? TEXT("None") : *UnsupportedShapeFailureReason);
		return UnsupportedShapeFailureKind;
	}

	// ResidualOwnerTagGateDrift 和 OwnedTagSnapshotDrift 必须拆开看：
	// 前者表示候选 Ability 自己的 owner-side 最终标签门在真实 cancel 后失败；
	// 后者表示“预测取消后标签快照”和“真实取消后标签快照”本身已经不一致。
	FGameplayTagContainer ResidualOwnerTagFailureTags;
	FString ResidualOwnerTagFailureReason;
	if (!CandidateAbility->CheckPredictiveFinalActivationTagGateAgainstOwnedTags(
			OwnedTagsAfterCancel,
			ResidualOwnerTagFailureReason,
			&ResidualOwnerTagFailureTags))
	{
		OutFailureReason = FString::Printf(
			TEXT("%s FailureTags=%s"),
			ResidualOwnerTagFailureReason.IsEmpty() ? TEXT("post-cancel residual owner tag drift detected.") : *ResidualOwnerTagFailureReason,
			*DescribeGameplayTagsForDebug(ResidualOwnerTagFailureTags));
		return EActionAbilityRelationshipFailureKind::ResidualOwnerTagGateDrift;
	}

	if (!DoOwnedTagSnapshotsMatch(PredictedOwnedTagsAfterCancel, OwnedTagsAfterCancel))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel owned tags diverged from the predicted cancel snapshot. PredictedOwnedTagsAfterCancel=%s ActualOwnedTagsAfterCancel=%s"),
			*DescribeGameplayTagsForDebug(PredictedOwnedTagsAfterCancel),
			*DescribeGameplayTagsForDebug(OwnedTagsAfterCancel));
		return EActionAbilityRelationshipFailureKind::OwnedTagSnapshotDrift;
	}

	FGameplayTagContainer ResourceFailureTags;
	if (!CandidateAbility->CheckPreCancelResourcePreconditions(
			CandidateSpec.Handle,
			ActorInfo,
			&ResourceFailureTags))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel resource drift detected. FailureTags=%s"),
			*DescribeGameplayTagsForDebug(ResourceFailureTags));
		return EActionAbilityRelationshipFailureKind::ResourceDrift;
	}

	if (!CandidateAbility->IsActivationAllowedByCombatReact(CombatReactComponent))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel combat react drift detected. Activation=%s"),
			*CandidateAbility->DescribeCombatReactActivationDecision(CombatReactComponent));
		return EActionAbilityRelationshipFailureKind::CombatReactDrift;
	}

	if (GetUserAbilityActivationInhibited())
	{
		OutFailureReason = TEXT("post-cancel user ability activation became inhibited. GetUserAbilityActivationInhibited()=true");
		return EActionAbilityRelationshipFailureKind::UserActivationInhibitedDrift;
	}

	if (AreAbilityTagsBlocked(CandidateAbility->GetAbilityIdentityTags()))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel ability tags became globally blocked. AbilityTags=%s FinalFailureTags=%s"),
			*DescribeGameplayTagsForDebug(CandidateAbility->GetAbilityIdentityTags()),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return EActionAbilityRelationshipFailureKind::AbilityTagsBlockedDrift;
	}

	if (IsAbilityInputBlocked(CandidateSpec.InputID))
	{
		OutFailureReason = FString::Printf(
			TEXT("post-cancel ability input became blocked. InputID=%d"),
			CandidateSpec.InputID);
		return EActionAbilityRelationshipFailureKind::InputBlockedDrift;
	}

	// 只有所有已知 residual probe 都跑完后，才退回 UnknownFinalGate。
	// 因此它是保底桶，而不是正常使用者应当频繁看到的常规失败结论。
	OutFailureReason = FString::Printf(
		TEXT("post-cancel final activation gate still failed after all known reclassification probes were exhausted. Candidate=%s FinalFailureTags=%s KnownProbes=%s Prioritize investigating stale assets, bypassed grant validation, non-whitelisted dynamic grant paths or still-unprobed engine gates."),
		*CandidateAbility->GetAbilityDebugName(),
		*DescribeGameplayTagsForDebug(FinalActivationFailureTags),
		*DescribeRelationshipPostCancelKnownProbeSummary());
	return EActionAbilityRelationshipFailureKind::UnknownFinalGate;
}

FActionAbilityRelationshipFailureDiagnostic UActionAbilitySystemComponent::BuildRelationshipActivationFailureDiagnosticForPostCancelFinalPrecheck(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
	const FGameplayTagContainer& OwnedTagsBeforeCancel,
	const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
	const FGameplayTagContainer& PredictedReleasedTags,
	const FGameplayTagContainer& FinalActivationFailureTags,
	const FString& FinalFailureReason) const
{
	FActionAbilityRelationshipFailureDiagnostic Diagnostic;
	(void)CombatReactComponent;
	Diagnostic.bValid = true;
	Diagnostic.CandidateSpecHandle = CandidateSpec.Handle;
	Diagnostic.FailureStage = EActionAbilityRelationshipFailureStage::PostCancelFinalPrecheck;
	Diagnostic.CandidateAbilityClassName = GetNameSafe(
		CandidateSpec.Ability ? CandidateSpec.Ability->GetClass() : nullptr);
	Diagnostic.CandidateAbilityDebugName =
		CandidateAbility ? CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility");
	Diagnostic.CandidateCategory =
		CandidateAbility ? CandidateAbility->GetAbilityCategory() : EActionAbilityCategory::None;
	BuildAbilityDebugNamesForHandles(AbilityHandlesToCancel, Diagnostic.AbilitiesToCancel);
	Diagnostic.OwnedTagsBeforeCancel = OwnedTagsBeforeCancel;
	Diagnostic.PredictedOwnedTagsAfterCancel = PredictedOwnedTagsAfterCancel;
	Diagnostic.PredictedReleasedTags = PredictedReleasedTags;
	GetOwnedGameplayTags(Diagnostic.OwnedTagsAfterCancel);
	Diagnostic.FinalActivationFailureTags = FinalActivationFailureTags;
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	const UActionCombatReactComponent* CurrentCombatReactComponent =
		ResolveCombatReactComponentFromActorInfo(ActorInfo);
	Diagnostic.CombatReactActivationDecision = CandidateAbility
		? CandidateAbility->DescribeCombatReactActivationDecision(CurrentCombatReactComponent)
		: TEXT("None");

	FString ClassifiedFailureReason;
	Diagnostic.FailureKind = ClassifyPostCancelDriftKind(
		CandidateSpec,
		CandidateAbility,
		CurrentCombatReactComponent,
		PredictedOwnedTagsAfterCancel,
		Diagnostic.OwnedTagsAfterCancel,
		FinalActivationFailureTags,
		ClassifiedFailureReason);

	if (!FinalFailureReason.IsEmpty() && !ClassifiedFailureReason.IsEmpty())
	{
		Diagnostic.FailureReason = FString::Printf(
			TEXT("%s ClassifiedReason=%s"),
			*FinalFailureReason,
			*ClassifiedFailureReason);
	}
	else if (!ClassifiedFailureReason.IsEmpty())
	{
		Diagnostic.FailureReason = ClassifiedFailureReason;
	}
	else
	{
		Diagnostic.FailureReason = FinalFailureReason;
	}

	return Diagnostic;
}

EActionAbilityRelationshipFailureKind UActionAbilitySystemComponent::ClassifyFinalTryActivateFailureKind(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
	const FGameplayTagContainer& FinalActivationFailureTags,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	// final try 先读 InternalTryActivateAbilityFailureTags，再尽量复用 post-cancel 的共享重分类。
	// 只有两者都解释不了的尾门，才继续往 final-tail probe 和 FinalTryActivateUnknown 走。
	if (FinalActivationFailureTags.Num() > 0)
	{
		FGameplayTagContainer OwnedTagsAfterFinalTryFailure;
		GetOwnedGameplayTags(OwnedTagsAfterFinalTryFailure);

		FString ClassifiedGateFailureReason;
		const EActionAbilityRelationshipFailureKind ReclassifiedGateFailureKind =
			ClassifyPostCancelDriftKind(
				CandidateSpec,
				CandidateAbility,
				CombatReactComponent,
				PredictedOwnedTagsAfterCancel,
				OwnedTagsAfterFinalTryFailure,
				FinalActivationFailureTags,
				ClassifiedGateFailureReason);
		if (ReclassifiedGateFailureKind != EActionAbilityRelationshipFailureKind::UnknownFinalGate)
		{
			OutFailureReason = FString::Printf(
				TEXT("final TryActivateAbility failed after post-cancel final activation precheck passed. InternalTryActivateAbilityFailureTags=%s ClassifiedReason=%s"),
				*DescribeGameplayTagsForDebug(FinalActivationFailureTags),
				ClassifiedGateFailureReason.IsEmpty() ? TEXT("None") : *ClassifiedGateFailureReason);
			return ReclassifiedGateFailureKind;
		}
	}

	EActionAbilityRelationshipFailureKind UnsupportedShapeFailureKind =
		EActionAbilityRelationshipFailureKind::None;
	FString UnsupportedShapeFailureReason;
	const UGameplayAbility* AbilityToInspectForWhitelistDrift =
		CandidateAbility ? static_cast<const UGameplayAbility*>(CandidateAbility) : CandidateSpec.Ability.Get();
	if (TryClassifyUnsupportedHeroCombatRelationshipAbilityShape(
			AbilityToInspectForWhitelistDrift,
			UnsupportedShapeFailureKind,
			UnsupportedShapeFailureReason))
	{
		OutFailureReason = FString::Printf(
			TEXT("final TryActivateAbility failed because the candidate ability shape no longer matches the Hero combat relationship whitelist. FailureKind=%s InternalTryActivateAbilityFailureTags=%s Details=%s"),
			*DescribeRelationshipActivationFailureKind(UnsupportedShapeFailureKind),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags),
			UnsupportedShapeFailureReason.IsEmpty() ? TEXT("None") : *UnsupportedShapeFailureReason);
		return UnsupportedShapeFailureKind;
	}

	if (!CandidateSpec.Handle.IsValid() || !FindAbilitySpecFromHandle(CandidateSpec.Handle))
	{
		OutFailureReason = FString::Printf(
			TEXT("final TryActivateAbility failed because candidate spec no longer resolves. Handle=%s InternalTryActivateAbilityFailureTags=%s"),
			*CandidateSpec.Handle.ToString(),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return EActionAbilityRelationshipFailureKind::CandidateSpecMissingOnFinalTry;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	if (!ActorInfo || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		OutFailureReason = FString::Printf(
			TEXT("final TryActivateAbility failed because actor info became invalid after post-cancel final activation precheck passed. Handle=%s InternalTryActivateAbilityFailureTags=%s"),
			*CandidateSpec.Handle.ToString(),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return EActionAbilityRelationshipFailureKind::FinalTryActorInfoInvalid;
	}

	if (CandidateAbility)
	{
		FGameplayTagContainer OwnedTagsAfterFinalTryFailure;
		GetOwnedGameplayTags(OwnedTagsAfterFinalTryFailure);

		FString ClassifiedGateFailureReason;
		const EActionAbilityRelationshipFailureKind ReclassifiedGateFailureKind =
			ClassifyPostCancelDriftKind(
				CandidateSpec,
				CandidateAbility,
				CombatReactComponent,
				PredictedOwnedTagsAfterCancel,
				OwnedTagsAfterFinalTryFailure,
				FinalActivationFailureTags,
				ClassifiedGateFailureReason);
		if (ReclassifiedGateFailureKind != EActionAbilityRelationshipFailureKind::UnknownFinalGate)
		{
			OutFailureReason = FString::Printf(
				TEXT("final TryActivateAbility failed after post-cancel final activation precheck passed. InternalTryActivateAbilityFailureTags=%s ClassifiedReason=%s"),
				*DescribeGameplayTagsForDebug(FinalActivationFailureTags),
				ClassifiedGateFailureReason.IsEmpty() ? TEXT("None") : *ClassifiedGateFailureReason);
			return ReclassifiedGateFailureKind;
		}
	}

	// 走到这里说明共享 residual probe 也没能解释问题，
	// 接下来只补 final TryActivateAbility 自己才会暴露的尾部门事实。
	const FGameplayAbilitySpec* ResolvedSpec = FindAbilitySpecFromHandle(CandidateSpec.Handle);
	const UGameplayAbility* ResolvedAbility = ResolvedSpec ? ResolvedSpec->Ability : nullptr;
	const UGameplayAbility* AbilityToInspect =
		ResolvedAbility ? ResolvedAbility : (CandidateSpec.Ability ? CandidateSpec.Ability : nullptr);
	if (!AbilityToInspect)
	{
		OutFailureReason = FString::Printf(
			TEXT("final TryActivateAbility failed but no ability object could be resolved from the candidate spec. Handle=%s InternalTryActivateAbilityFailureTags=%s"),
			*CandidateSpec.Handle.ToString(),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return EActionAbilityRelationshipFailureKind::FinalTryAbilityObjectMissing;
	}

	const ENetRole LocalRole = ResolveActivationNetRoleFromActorInfo(ActorInfo);
	const bool bIsLocal = AbilityActorInfo.IsValid() && AbilityActorInfo->IsLocallyControlled();
	const EGameplayAbilityNetExecutionPolicy::Type NetExecutionPolicy =
		AbilityToInspect->GetNetExecutionPolicy();
	if (LocalRole == ROLE_SimulatedProxy
		|| (!bIsLocal
			&& (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly
				|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted))
		|| (LocalRole != ROLE_Authority
			&& (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly
				|| NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated)))
	{
		OutFailureReason = FString::Printf(
			TEXT("final TryActivateAbility failed because execution context was rejected. NetExecutionPolicy=%s LocalRole=%s IsLocallyControlled=%s InternalTryActivateAbilityFailureTags=%s"),
			*UEnum::GetValueAsString(NetExecutionPolicy),
			*DescribeNetRoleForDebug(LocalRole),
			*AbilitySystemComponentBoolToDebugText(bIsLocal),
			*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
		return EActionAbilityRelationshipFailureKind::FinalTryExecutionContextRejected;
	}

	if (ResolvedSpec
		&& AbilityToInspect->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		if (ResolvedSpec->IsActive())
		{
			OutFailureReason = FString::Printf(
				TEXT("final TryActivateAbility failed because the InstancedPerActor spec is already active. Candidate=%s RetriggerEnabled=%s InternalTryActivateAbilityFailureTags=%s"),
				CandidateAbility ? *CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility"),
				*AbilitySystemComponentBoolToDebugText(CandidateAbility && CandidateAbility->UsesRetriggerInstancedAbility()),
				*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
			return EActionAbilityRelationshipFailureKind::FinalTryInstancedPerActorAlreadyActive;
		}

		if (!ResolvedSpec->GetPrimaryInstance())
		{
			OutFailureReason = FString::Printf(
				TEXT("final TryActivateAbility failed because the InstancedPerActor spec has no primary instance. Candidate=%s InternalTryActivateAbilityFailureTags=%s"),
				CandidateAbility ? *CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility"),
				*DescribeGameplayTagsForDebug(FinalActivationFailureTags));
			return EActionAbilityRelationshipFailureKind::FinalTryMissingPrimaryInstance;
		}
	}

	OutFailureReason = FString::Printf(
		TEXT("final TryActivateAbility still failed after post-cancel final activation precheck passed and all known internal gate/tail probes were exhausted. Candidate=%s InternalTryActivateAbilityFailureTags=%s KnownProbes=%s"),
		CandidateAbility ? *CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility"),
		*DescribeGameplayTagsForDebug(FinalActivationFailureTags),
		*DescribeRelationshipFinalTryKnownProbeSummary());
	return EActionAbilityRelationshipFailureKind::FinalTryActivateUnknown;
}

FActionAbilityRelationshipFailureDiagnostic UActionAbilitySystemComponent::BuildRelationshipActivationFailureDiagnosticForFinalTryActivate(
	const FGameplayAbilitySpec& CandidateSpec,
	const UActionGameplayAbilityBase* CandidateAbility,
	const UActionCombatReactComponent* CombatReactComponent,
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandlesToCancel,
	const FGameplayTagContainer& OwnedTagsBeforeCancel,
	const FGameplayTagContainer& PredictedOwnedTagsAfterCancel,
	const FGameplayTagContainer& PredictedReleasedTags,
	const FGameplayTagContainer& FinalActivationFailureTags,
	const FString& FinalFailureReason) const
{
	FActionAbilityRelationshipFailureDiagnostic Diagnostic;
	(void)CombatReactComponent;
	Diagnostic.bValid = true;
	Diagnostic.CandidateSpecHandle = CandidateSpec.Handle;
	Diagnostic.FailureStage = EActionAbilityRelationshipFailureStage::FinalTryActivate;
	Diagnostic.FailureReason = FinalFailureReason;
	Diagnostic.FinalActivationFailureTags = FinalActivationFailureTags;

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	const UActionCombatReactComponent* CurrentCombatReactComponent =
		ResolveCombatReactComponentFromActorInfo(ActorInfo);
	Diagnostic.CandidateAbilityClassName = GetNameSafe(
		CandidateSpec.Ability ? CandidateSpec.Ability->GetClass() : nullptr);
	Diagnostic.CandidateAbilityDebugName =
		CandidateAbility ? CandidateAbility->GetAbilityDebugName() : TEXT("UnknownAbility");
	Diagnostic.CandidateCategory =
		CandidateAbility ? CandidateAbility->GetAbilityCategory() : EActionAbilityCategory::None;
	BuildAbilityDebugNamesForHandles(AbilityHandlesToCancel, Diagnostic.AbilitiesToCancel);
	Diagnostic.OwnedTagsBeforeCancel = OwnedTagsBeforeCancel;
	Diagnostic.PredictedOwnedTagsAfterCancel = PredictedOwnedTagsAfterCancel;
	Diagnostic.PredictedReleasedTags = PredictedReleasedTags;
	GetOwnedGameplayTags(Diagnostic.OwnedTagsAfterCancel);
	Diagnostic.CombatReactActivationDecision = CandidateAbility
		? CandidateAbility->DescribeCombatReactActivationDecision(CurrentCombatReactComponent)
		: TEXT("None");

	FString ClassifiedFailureReason;
	Diagnostic.FailureKind = ClassifyFinalTryActivateFailureKind(
		CandidateSpec,
		CandidateAbility,
		CurrentCombatReactComponent,
		PredictedOwnedTagsAfterCancel,
		FinalActivationFailureTags,
		ClassifiedFailureReason);
	if (!FinalFailureReason.IsEmpty() && !ClassifiedFailureReason.IsEmpty())
	{
		Diagnostic.FailureReason = FString::Printf(
			TEXT("%s ClassifiedReason=%s"),
			*FinalFailureReason,
			*ClassifiedFailureReason);
	}
	else if (!ClassifiedFailureReason.IsEmpty())
	{
		Diagnostic.FailureReason = ClassifiedFailureReason;
	}

	return Diagnostic;
}

void UActionAbilitySystemComponent::RecordRelationshipActivationFailureDiagnostic(
	FActionAbilityRelationshipFailureDiagnostic Diagnostic)
{
	if (!Diagnostic.bValid)
	{
		return;
	}

	++RelationshipActivationFailureSequenceCounter;
	Diagnostic.FailureSequence = RelationshipActivationFailureSequenceCounter;
	Diagnostic.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastRelationshipActivationFailureDiagnostic = Diagnostic;
	RecentRelationshipActivationFailureDiagnostics.Add(Diagnostic);

	const int32 OverflowCount =
		RecentRelationshipActivationFailureDiagnostics.Num() - MaxRelationshipActivationFailureHistoryEntries;
	if (OverflowCount > 0)
	{
		RecentRelationshipActivationFailureDiagnostics.RemoveAt(0, OverflowCount);
	}
}

void UActionAbilitySystemComponent::ClearLastRelationshipActivationFailureDiagnosticIfSupersededBySuccessfulActivation(
	const FGameplayAbilitySpecHandle CandidateSpecHandle)
{
	if (LastRelationshipActivationFailureDiagnostic.MatchesSpecHandle(CandidateSpecHandle))
	{
		LastRelationshipActivationFailureDiagnostic.Reset();
	}
}

FString UActionAbilitySystemComponent::BuildRelationshipActivationFailureDebugText(
	const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const
{
	return FString::Printf(
		TEXT("[GA Failure] relationship activation failure snapshot. FailureSequence=%llu WorldTimeSeconds=%.3f Candidate=%s Class=%s Category=%s FailureStage=%s FailureKind=%s AbilitiesToCancel=%s OwnedTagsBeforeCancel=%s PredictedOwnedTagsAfterCancel=%s PredictedReleasedTags=%s OwnedTagsAfterCancel=%s FinalActivationFailureTags=%s CombatReactActivation=%s FailureReason=%s"),
		Diagnostic.FailureSequence,
		Diagnostic.WorldTimeSeconds,
		Diagnostic.CandidateAbilityDebugName.IsEmpty() ? TEXT("UnknownAbility") : *Diagnostic.CandidateAbilityDebugName,
		Diagnostic.CandidateAbilityClassName.IsEmpty() ? TEXT("UnknownClass") : *Diagnostic.CandidateAbilityClassName,
		*ActionAbilityCategoryToString(Diagnostic.CandidateCategory),
		*DescribeRelationshipActivationFailureStage(Diagnostic.FailureStage),
		*DescribeRelationshipActivationFailureKind(Diagnostic.FailureKind),
		Diagnostic.AbilitiesToCancel.Num() > 0 ? *FString::Join(Diagnostic.AbilitiesToCancel, TEXT(", ")) : TEXT("None"),
		*DescribeGameplayTagsForDebug(Diagnostic.OwnedTagsBeforeCancel),
		*DescribeGameplayTagsForDebug(Diagnostic.PredictedOwnedTagsAfterCancel),
		*DescribeGameplayTagsForDebug(Diagnostic.PredictedReleasedTags),
		*DescribeGameplayTagsForDebug(Diagnostic.OwnedTagsAfterCancel),
		*DescribeGameplayTagsForDebug(Diagnostic.FinalActivationFailureTags),
		Diagnostic.CombatReactActivationDecision.IsEmpty() ? TEXT("None") : *Diagnostic.CombatReactActivationDecision,
		Diagnostic.FailureReason.IsEmpty() ? TEXT("None") : *Diagnostic.FailureReason);
}

FString UActionAbilitySystemComponent::BuildRelationshipActivationFailureAuditSummaryLine(
	const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const
{
	return FString::Printf(
		TEXT("[GA Audit] LastRelationshipActivationFailure Snapshot=LastFailureOnly FailureSequence=%llu FailureStage=%s FailureKind=%s FinalActivationFailureTags=%s FailureReason=%s"),
		Diagnostic.FailureSequence,
		*DescribeRelationshipActivationFailureStage(Diagnostic.FailureStage),
		*DescribeRelationshipActivationFailureKind(Diagnostic.FailureKind),
		*DescribeGameplayTagsForDebug(Diagnostic.FinalActivationFailureTags),
		Diagnostic.FailureReason.IsEmpty() ? TEXT("None") : *Diagnostic.FailureReason);
}

FString UActionAbilitySystemComponent::BuildRelationshipActivationFailureHistoryLine(
	const FActionAbilityRelationshipFailureDiagnostic& Diagnostic) const
{
	return FString::Printf(
		TEXT("[GA FailureHistory] FailureSequence=%llu WorldTimeSeconds=%.3f Candidate=%s Class=%s Category=%s FailureStage=%s FailureKind=%s FinalActivationFailureTags=%s AbilitiesToCancel=%s FailureReason=%s"),
		Diagnostic.FailureSequence,
		Diagnostic.WorldTimeSeconds,
		Diagnostic.CandidateAbilityDebugName.IsEmpty() ? TEXT("UnknownAbility") : *Diagnostic.CandidateAbilityDebugName,
		Diagnostic.CandidateAbilityClassName.IsEmpty() ? TEXT("UnknownClass") : *Diagnostic.CandidateAbilityClassName,
		*ActionAbilityCategoryToString(Diagnostic.CandidateCategory),
		*DescribeRelationshipActivationFailureStage(Diagnostic.FailureStage),
		*DescribeRelationshipActivationFailureKind(Diagnostic.FailureKind),
		*DescribeGameplayTagsForDebug(Diagnostic.FinalActivationFailureTags),
		Diagnostic.AbilitiesToCancel.Num() > 0 ? *FString::Join(Diagnostic.AbilitiesToCancel, TEXT(", ")) : TEXT("None"),
		Diagnostic.FailureReason.IsEmpty() ? TEXT("None") : *Diagnostic.FailureReason);
}

bool UActionAbilitySystemComponent::ResolveAbilityRelationshipBeforeActivation(
	const FGameplayAbilitySpec& CandidateSpec,
	TArray<FGameplayAbilitySpecHandle>& OutAbilityHandlesToCancel,
	FString& OutFailureReason) const
{
	OutAbilityHandlesToCancel.Reset();
	OutFailureReason.Reset();

	// 这一步只做“关系系统自己的前置裁决”：
	// 1. 候选 Ability 自身的受击规则 / 运行时依赖检查；
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

	// 候选 Ability 自己的共享硬门禁 / 业务前置先在这里稳定收口。
	// 真正面对当前活跃 Ability 的默认矩阵关系和 interrupt-window 例外窗口，则在下面逐个活跃 Ability 统一判断。
	const FString CandidateDebugName = CandidateAbility->GetAbilityDebugName();
	const EActionAbilityCategory CandidateCategory = CandidateAbility->GetAbilityCategory();
	const FActionAbilityCategoryRelationshipRule* CandidateRelationshipRule =
		FindCategoryRelationshipRule(CandidateCategory);
	if (!CandidateRelationshipRule)
	{
		OutFailureReason = FString::Printf(
			TEXT("%s has no valid relationship matrix rule. Category=%s"),
			*CandidateDebugName,
			*ActionAbilityCategoryToString(CandidateCategory));
		return false;
	}

	const int32 CandidatePriority = CandidateRelationshipRule->Priority;
	const AActionCharacterBase* OwnerCharacter = Cast<AActionCharacterBase>(GetAvatarActor());
	const UActionCombatReactComponent* CombatReactComponent =
		OwnerCharacter ? OwnerCharacter->GetActionCombatReactComponent() : nullptr;
	const AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActor());
	const UHeroCombatComponent* HeroCombatComponent =
		OwnerHeroCharacter ? OwnerHeroCharacter->GetHeroCombatComponent() : nullptr;
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

		const EActionAbilityCategory ActiveCategory = ActiveAbility->GetAbilityCategory();
		const FActionAbilityCategoryRelationshipRule* ActiveRelationshipRule =
			FindCategoryRelationshipRule(ActiveCategory);
		if (!ActiveRelationshipRule)
		{
			OutFailureReason = FString::Printf(
				TEXT("Active ability %s has no valid relationship matrix rule. Category=%s"),
				*ActiveAbility->GetAbilityDebugName(),
				*ActionAbilityCategoryToString(ActiveCategory));
			return false;
		}

		const int32 ActivePriority = ActiveRelationshipRule->Priority;
		const FString ActiveDebugName = ActiveAbility->GetAbilityDebugName();
		const bool bCandidateTargetsActiveAbility =
			CandidateRelationshipRule->CanCancelCategory(ActiveCategory);

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
			if (!bCandidateTargetsActiveAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s has higher priority than active ability %s, but the matrix default cancel targets do not include category %s. CandidateCategory=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*ActionAbilityCategoryToString(ActiveCategory),
					*ActionAbilityCategoryToString(CandidateCategory));
				return false;
			}

			const bool bCanInterruptThisActiveAbility =
				CandidateRelationshipRule->bCanInterruptLowerPriorityAbilities
				&& ActiveRelationshipRule->bCanBeInterruptedByHigherPriority
				&& bCandidateTargetsActiveAbility;

			if (!bCanInterruptThisActiveAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s has higher priority but the matrix still rejects interrupting %s. CandidateCategory=%s ActiveCategory=%s CandidateInterruptLower=%s ActiveBeInterruptedHigher=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*ActionAbilityCategoryToString(CandidateCategory),
					*ActionAbilityCategoryToString(ActiveCategory),
					*AbilitySystemComponentBoolToDebugText(CandidateRelationshipRule->bCanInterruptLowerPriorityAbilities),
					*AbilitySystemComponentBoolToDebugText(ActiveRelationshipRule->bCanBeInterruptedByHigherPriority));
				return false;
			}

			OutAbilityHandlesToCancel.AddUnique(ActiveSpec.Handle);
			continue;
		}

		if (CandidatePriority == ActivePriority)
		{
			if (!bCandidateTargetsActiveAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s shares the same priority with active ability %s, but the matrix default cancel targets do not include category %s. CandidateCategory=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*ActionAbilityCategoryToString(ActiveCategory),
					*ActionAbilityCategoryToString(CandidateCategory));
				return false;
			}

			const bool bCanInterruptSamePriorityAbility =
				CandidateRelationshipRule->bCanInterruptSamePriorityAbilities
				&& ActiveRelationshipRule->bCanBeInterruptedBySamePriority
				&& bCandidateTargetsActiveAbility;

			if (!bCanInterruptSamePriorityAbility)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s and %s share the same priority, but the matrix rejects same-priority interrupt. CandidateCategory=%s ActiveCategory=%s CandidateInterruptSame=%s ActiveBeInterruptedSame=%s"),
					*CandidateDebugName,
					*ActiveDebugName,
					*ActionAbilityCategoryToString(CandidateCategory),
					*ActionAbilityCategoryToString(ActiveCategory),
					*AbilitySystemComponentBoolToDebugText(CandidateRelationshipRule->bCanInterruptSamePriorityAbilities),
					*AbilitySystemComponentBoolToDebugText(ActiveRelationshipRule->bCanBeInterruptedBySamePriority));
				return false;
			}

			OutAbilityHandlesToCancel.AddUnique(ActiveSpec.Handle);
			continue;
		}

		const bool bInterruptWindowOwnedByActiveAbility =
			HeroCombatComponent
			&& HeroCombatComponent->DoesAbilityInterruptWindowBelongTo(ActiveSpec.Handle);
		const bool bInterruptWindowAllowsCandidate =
			HeroCombatComponent
			&& HeroCombatComponent->IsAbilityInterruptCategoryAllowedForOwner(
				ActiveSpec.Handle,
				CandidateCategory);
		if (bInterruptWindowOwnedByActiveAbility && bInterruptWindowAllowsCandidate)
		{
			OutAbilityHandlesToCancel.AddUnique(ActiveSpec.Handle);
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("%s has lower priority than active ability %s and no interrupt exception window owned by that active ability allows category %s. ActiveCategory=%s InterruptWindowOpen=%s OwnerMatches=%s WindowAllowsCandidate=%s"),
			*CandidateDebugName,
			*ActiveDebugName,
			*ActionAbilityCategoryToString(CandidateCategory),
			*ActionAbilityCategoryToString(ActiveCategory),
			*AbilitySystemComponentBoolToDebugText(
				HeroCombatComponent && HeroCombatComponent->IsAbilityInterruptWindowActive()),
			*AbilitySystemComponentBoolToDebugText(
				bInterruptWindowOwnedByActiveAbility),
			*AbilitySystemComponentBoolToDebugText(
				bInterruptWindowAllowsCandidate));
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

	const EActionAbilityCategory CandidateCategory = CandidateAbility->GetAbilityCategory();
	const FActionAbilityCategoryRelationshipRule* CandidateRule =
		FindCategoryRelationshipRule(CandidateCategory);

	return FString::Printf(
		TEXT("[GA Relationship] Activation blocked. Candidate=%s Category=%s Priority=%d Reason=%s Activation=%s Interrupt=%s"),
		*CandidateAbility->GetAbilityDebugName(),
		*ActionAbilityCategoryToString(CandidateCategory),
		CandidateRule ? CandidateRule->Priority : 0,
		*FailureReason,
		*CandidateAbility->DescribeCombatReactActivationDecision(CombatReactComponent),
		*CandidateAbility->DescribeCombatReactInterruptDecision(CombatReactComponent));
}

bool UActionAbilitySystemComponent::ValidateGrantedHeroCombatAbilityCategoryBeforeGrant(
	const UGameplayAbility* GrantedAbility,
	const FGameplayTag& AbilityInputTag,
	FString& OutFailureReason,
	FString& OutAuditState) const
{
	OutFailureReason.Reset();
	OutAuditState.Reset();

	const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
		BuildActionHeroCombatAbilityCategoryValidationResult(GrantedAbility);
	if (!ValidationResult.bIsHeroCombatRelationshipAbility)
	{
		return true;
	}

	OutAuditState = ValidationResult.AuditState;
	OutFailureReason = ValidationResult.FailureReason;

	(void)AbilityInputTag;
	return ValidationResult.bPassed;
}

FString UActionAbilitySystemComponent::BuildGrantAbilityCategoryValidationFailureText(
	const UGameplayAbility* GrantedAbility,
	const FGameplayTag& AbilityInputTag,
	const FString& FailureReason,
	const FString& AuditState) const
{
	const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
		BuildActionHeroCombatAbilityCategoryValidationResult(GrantedAbility);

	return FString::Printf(
		TEXT("[GA GrantValidation] Input=%s Class=%s Identity=%s MatchedTopLevelIdentity=%s AdditionalPlayerAbilityTags=%s Category=%s HasBlueprintCanActivateOverride=%s CategoryAudit=%s Reason=%s"),
		AbilityInputTag.IsValid() ? *AbilityInputTag.ToString() : TEXT("None"),
		*GetNameSafe(GrantedAbility ? GrantedAbility->GetClass() : nullptr),
		*DescribeGameplayTagsForDebug(ValidationResult.IdentityTags),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.MatchedTopLevelIdentityTags),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.AdditionalPlayerAbilityTags),
		*ActionAbilityCategoryToString(ValidationResult.AbilityCategory),
		ValidationResult.bHasBlueprintCanActivateOverride ? TEXT("Yes") : TEXT("No"),
		AuditState.IsEmpty() ? *ValidationResult.AuditState : *AuditState,
		FailureReason.IsEmpty() ? TEXT("Unknown") : *FailureReason);
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
	return FindBestActivatableAbilitySpecByDynamicTag(InInputTag);
}

FGameplayAbilitySpec* UActionAbilitySystemComponent::FindBestActivatableAbilitySpecByDynamicTag(
	const FGameplayTag& InDynamicTag)
{
	if (!InDynamicTag.IsValid())
	{
		return nullptr;
	}

	// 动态标签到授予 Spec 的正式解析口径固定为“最后授予优先”。
	// 输入链、tag 直激活链、调试链和外部补冷却链都应复用同一套选择策略。
	TArray<FGameplayAbilitySpec>& LocalAbilitySpecs = GetActivatableAbilities();
	for (int32 Index = LocalAbilitySpecs.Num() - 1; Index >= 0; --Index)
	{
		FGameplayAbilitySpec& Spec = LocalAbilitySpecs[Index];
		if (Spec.Ability && Spec.DynamicAbilityTags.HasTagExact(InDynamicTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}

const FGameplayAbilitySpec* UActionAbilitySystemComponent::FindBestActivatableAbilitySpecByDynamicTag(
	const FGameplayTag& InDynamicTag) const
{
	if (!InDynamicTag.IsValid())
	{
		return nullptr;
	}

	const TArray<FGameplayAbilitySpec>& LocalAbilitySpecs = GetActivatableAbilities();
	for (int32 Index = LocalAbilitySpecs.Num() - 1; Index >= 0; --Index)
	{
		const FGameplayAbilitySpec& Spec = LocalAbilitySpecs[Index];
		if (Spec.Ability && Spec.DynamicAbilityTags.HasTagExact(InDynamicTag))
		{
			return &Spec;
		}
	}

	return nullptr;
}

FGameplayAbilitySpec* UActionAbilitySystemComponent::GetActivatableAbilitySpecByTag(FGameplayTag AbilityTag)
{
	return FindBestActivatableAbilitySpecByDynamicTag(AbilityTag);
}

const FGameplayAbilitySpec* UActionAbilitySystemComponent::GetActivatableAbilitySpecByTag(const FGameplayTag AbilityTag) const
{
	return FindBestActivatableAbilitySpecByDynamicTag(AbilityTag);
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

void UActionAbilitySystemComponent::PrintHeroCombatAbilityCategoryAudit() const
{
	TArray<FString> AuditLines;
	GetHeroCombatAbilityCategoryAuditLines(AuditLines);

	if (AuditLines.Num() <= 0)
	{
		const FString EmptyAuditText = TEXT("[GA CategoryAudit] 当前 ASC 上未找到任何 Hero 战斗 GA 类别审计目标。");
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

void UActionAbilitySystemComponent::PrintHeroCombatAbilityRelationshipFailureHistory(
	const int32 MaxEntries) const
{
	TArray<FString> HistoryLines;
	GetHeroCombatAbilityRelationshipFailureHistoryLines(HistoryLines, MaxEntries);

	if (HistoryLines.Num() <= 0)
	{
		const FString EmptyHistoryText = TEXT("[GA FailureHistory] 当前 ASC 上还没有记录到任何关系主链激活失败历史。");
		Debug::Print(EmptyHistoryText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *EmptyHistoryText);
		return;
	}

	for (const FString& HistoryLine : HistoryLines)
	{
		Debug::Print(HistoryLine, FColor::Cyan, 6.0f);
		UE_LOG(ActionRPG, Log, TEXT("%s"), *HistoryLine);
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
		const FString CategoryAuditLine = BuildHeroCombatAbilityCategoryAuditLine(*AbilitySpec, ActionAbility);
		Debug::Print(CategoryAuditLine, FColor::Green, 6.0f);
		UE_LOG(ActionRPG, Log, TEXT("%s"), *CategoryAuditLine);

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

		if (LastRelationshipActivationFailureDiagnostic.MatchesSpecHandle(AbilitySpec->Handle))
		{
			const FString FailureText =
				BuildRelationshipActivationFailureDebugText(LastRelationshipActivationFailureDiagnostic);
			Debug::Print(FailureText, FColor::Yellow, 6.0f);
			UE_LOG(ActionRPG, Log, TEXT("%s"), *FailureText);
		}
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

		if (ActionGameplayTags::IsSpiritSkillInputTag(AbilityInputTag))
		{
			if (const UHeroCombatComponent* HeroCombatComponent = HeroCharacter->GetHeroCombatComponent())
			{
				const FString SpiritStateText = FString::Printf(
					TEXT("[GA Debug] 当前 Spirit 资格：%s"),
					*HeroCombatComponent->DescribeSpiritSkillComboRuntimeState(AbilityInputTag));
				Debug::Print(SpiritStateText, FColor::Green, 6.0f);
				UE_LOG(ActionRPG, Log, TEXT("%s"), *SpiritStateText);
			}
		}
	}

	return true;
}

bool UActionAbilitySystemComponent::PrintHeroCombatAbilityCategoryAuditByInputTag(
	const FGameplayTag AbilityInputTag) const
{
	if (!AbilityInputTag.IsValid())
	{
		const FString InvalidInputText = TEXT("[GA CategoryAudit] 输入标签无效，无法打印 Hero 战斗 GA 类别审计摘要。");
		Debug::Print(InvalidInputText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *InvalidInputText);
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = GetActivatableAbilitySpecByTag(AbilityInputTag);
	if (!AbilitySpec)
	{
		const FString MissingAbilityText = FString::Printf(
			TEXT("[GA CategoryAudit] 当前 ASC 未授予输入=%s 对应的战斗 GA。"),
			*AbilityInputTag.ToString());
		Debug::Print(MissingAbilityText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *MissingAbilityText);
		return false;
	}

	const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(*AbilitySpec);
	if (!ActionAbility)
	{
		const FString MissingActionAbilityText = FString::Printf(
			TEXT("[GA CategoryAudit] 输入=%s 命中的授予 Spec 未解析到 ActionGameplayAbility。Class=%s"),
			*AbilityInputTag.ToString(),
			*GetNameSafe(AbilitySpec->Ability ? AbilitySpec->Ability->GetClass() : nullptr));
		Debug::Print(MissingActionAbilityText, FColor::Yellow, 4.0f);
		UE_LOG(ActionRPG, Warning, TEXT("%s"), *MissingActionAbilityText);
		return false;
	}

	const FString AuditLine = BuildHeroCombatAbilityCategoryAuditLine(*AbilitySpec, ActionAbility);
	Debug::Print(AuditLine, FColor::Green, 6.0f);
	UE_LOG(ActionRPG, Log, TEXT("%s"), *AuditLine);
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
	TSet<FGameplayAbilitySpecHandle> AuditedAbilityHandles;

	for (const FGameplayTag& AbilityIdentityTag : GetHeroCombatAbilityAuditTags())
	{
		bool bFoundAnyGrantedAbility = false;

		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(AbilitySpec);
			if (!IsHeroCombatAbilityRelevantForAudit(AbilitySpec, ActionAbility)
				|| !ActionAbility->GetAbilityIdentityTags().HasTagExact(AbilityIdentityTag))
			{
				continue;
			}

			bFoundAnyGrantedAbility = true;
			AuditedAbilityHandles.Add(AbilitySpec.Handle);
			OutAuditLines.Add(BuildHeroCombatAbilityRelationshipAuditLine(AbilitySpec, ActionAbility, CombatReactComponent));
			if (LastRelationshipActivationFailureDiagnostic.MatchesSpecHandle(AbilitySpec.Handle))
			{
				OutAuditLines.Add(BuildRelationshipActivationFailureAuditSummaryLine(
					LastRelationshipActivationFailureDiagnostic));
			}
		}

		if (!bFoundAnyGrantedAbility)
		{
			OutAuditLines.Add(FString::Printf(
				TEXT("[GA Audit] Ability=%s Granted=否"),
				*AbilityIdentityTag.ToString()));
		}
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(AbilitySpec);
		if (!IsHeroCombatAbilityRelevantForAudit(AbilitySpec, ActionAbility)
			|| AuditedAbilityHandles.Contains(AbilitySpec.Handle))
		{
			continue;
		}

		OutAuditLines.Add(BuildHeroCombatAbilityRelationshipAuditLine(AbilitySpec, ActionAbility, CombatReactComponent));
		if (LastRelationshipActivationFailureDiagnostic.MatchesSpecHandle(AbilitySpec.Handle))
		{
			OutAuditLines.Add(BuildRelationshipActivationFailureAuditSummaryLine(
				LastRelationshipActivationFailureDiagnostic));
		}
	}
}

void UActionAbilitySystemComponent::GetHeroCombatAbilityRelationshipFailureHistoryLines(
	TArray<FString>& OutHistoryLines,
	const int32 MaxEntries) const
{
	OutHistoryLines.Reset();

	const int32 ClampedMaxEntries = FMath::Clamp(
		MaxEntries,
		1,
		MaxRelationshipActivationFailureHistoryEntries);
	const int32 AvailableEntries = RecentRelationshipActivationFailureDiagnostics.Num();
	const int32 EntriesToPrint = FMath::Min(ClampedMaxEntries, AvailableEntries);

	for (int32 IndexOffset = 0; IndexOffset < EntriesToPrint; ++IndexOffset)
	{
		const int32 HistoryIndex = AvailableEntries - 1 - IndexOffset;
		OutHistoryLines.Add(BuildRelationshipActivationFailureHistoryLine(
			RecentRelationshipActivationFailureDiagnostics[HistoryIndex]));
	}
}

void UActionAbilitySystemComponent::GetHeroCombatAbilityCategoryAuditLines(TArray<FString>& OutAuditLines) const
{
	OutAuditLines.Reset();

	TSet<FGameplayAbilitySpecHandle> AuditedAbilityHandles;

	for (const FGameplayTag& AbilityIdentityTag : GetHeroCombatAbilityAuditTags())
	{
		bool bFoundAnyGrantedAbility = false;

		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(AbilitySpec);
			if (!IsHeroCombatAbilityRelevantForAudit(AbilitySpec, ActionAbility)
				|| !ActionAbility->GetAbilityIdentityTags().HasTagExact(AbilityIdentityTag))
			{
				continue;
			}

			bFoundAnyGrantedAbility = true;
			AuditedAbilityHandles.Add(AbilitySpec.Handle);
			OutAuditLines.Add(BuildHeroCombatAbilityCategoryAuditLine(AbilitySpec, ActionAbility));
		}

		if (!bFoundAnyGrantedAbility)
		{
			OutAuditLines.Add(FString::Printf(
				TEXT("[GA CategoryAudit] Identity=%s Granted=否"),
				*AbilityIdentityTag.ToString()));
		}
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UActionGameplayAbilityBase* ActionAbility = ResolveActionAbilityFromSpec(AbilitySpec);
		if (!IsHeroCombatAbilityRelevantForAudit(AbilitySpec, ActionAbility)
			|| AuditedAbilityHandles.Contains(AbilitySpec.Handle))
		{
			continue;
		}

		OutAuditLines.Add(BuildHeroCombatAbilityCategoryAuditLine(AbilitySpec, ActionAbility));
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
	const AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(GetAvatarActor());
	const UHeroCombatComponent* HeroCombatComponent =
		HeroCharacter ? HeroCharacter->GetHeroCombatComponent() : nullptr;
	const bool bInterruptWindowOpen =
		HeroCombatComponent && HeroCombatComponent->IsAbilityInterruptWindowActive();
	const bool bInterruptWindowOwnedByThisAbility =
		HeroCombatComponent && HeroCombatComponent->DoesAbilityInterruptWindowBelongTo(AbilitySpec.Handle);
	const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
		BuildActionHeroCombatAbilityCategoryValidationResult(Ability);
	const EActionAbilityCategory AbilityCategory = ValidationResult.AbilityCategory;
	const FActionAbilityCategoryRelationshipRule* RelationshipRule =
		FindCategoryRelationshipRule(AbilityCategory);
	const bool bInterruptWindowAllowsThisCategory =
		HeroCombatComponent && HeroCombatComponent->IsAbilityInterruptCategoryAllowedForOwner(
			AbilitySpec.Handle,
			AbilityCategory);

	return FString::Printf(
		TEXT("[GA Audit] Input=%s Class=%s Ability=%s Category=%s MatrixPriority=%d MatrixCancel=%s Active=%s InterruptWindowOpen=%s InterruptWindowOwner=%s InterruptWindowAllowsSelf=%s MatchedTopLevelIdentity=%s AdditionalPlayerAbilityTags=%s MatrixRuleValid=%s CategoryAudit=%s Activation=%s Interrupt=%s"),
		*DescribeGameplayTagsForDebug(AbilitySpec.DynamicAbilityTags),
		*GetNameSafe(AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr),
		*DescribeGameplayTagsForDebug(Ability->GetAbilityIdentityTags()),
		*ActionAbilityCategoryToString(AbilityCategory),
		RelationshipRule ? RelationshipRule->Priority : 0,
		RelationshipRule
			? *DescribeAbilityCategoriesForDebug(RelationshipRule->DefaultCancelableCategories)
			: TEXT("MissingMatrixRule"),
		*AbilitySystemComponentBoolToDebugText(AbilitySpec.IsActive()),
		*AbilitySystemComponentBoolToDebugText(bInterruptWindowOpen),
		*AbilitySystemComponentBoolToDebugText(bInterruptWindowOwnedByThisAbility),
		*AbilitySystemComponentBoolToDebugText(bInterruptWindowAllowsThisCategory),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.MatchedTopLevelIdentityTags),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.AdditionalPlayerAbilityTags),
		*AbilitySystemComponentBoolToDebugText(ValidationResult.bMatrixRuleValid),
		*ValidationResult.AuditState,
		*Ability->DescribeCombatReactActivationDecision(CombatReactComponent),
		*Ability->DescribeCombatReactInterruptDecision(CombatReactComponent));
}

FString UActionAbilitySystemComponent::BuildHeroCombatAbilityCategoryAuditLine(
	const FGameplayAbilitySpec& AbilitySpec,
	const UActionGameplayAbilityBase* Ability) const
{
	if (!Ability)
	{
		return TEXT("[GA CategoryAudit] Ability=Unknown Granted=否");
	}

	const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
		BuildActionHeroCombatAbilityCategoryValidationResult(Ability);

	return FString::Printf(
		TEXT("[GA CategoryAudit] Input=%s Class=%s Identity=%s MatchedTopLevelIdentity=%s AdditionalPlayerAbilityTags=%s Category=%s ExpectedCategory=%s MatrixRuleValid=%s CategoryAudit=%s Active=%s"),
		*DescribeGameplayTagsForDebug(AbilitySpec.DynamicAbilityTags),
		*GetNameSafe(AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr),
		*DescribeGameplayTagsForDebug(ValidationResult.IdentityTags),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.MatchedTopLevelIdentityTags),
		*DescribeGameplayTagsForDebug(ValidationResult.CategoryAuditResult.AdditionalPlayerAbilityTags),
		*ActionAbilityCategoryToString(ValidationResult.AbilityCategory),
		*ActionAbilityCategoryToString(
			ValidationResult.CategoryAuditResult.ExpectedCategoryFromTopLevelIdentity),
		*AbilitySystemComponentBoolToDebugText(ValidationResult.bMatrixRuleValid),
		*ValidationResult.AuditState,
		*AbilitySystemComponentBoolToDebugText(AbilitySpec.IsActive()));
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
		// 已激活实例继续接收 GAS 标准输入按下事件，供保持输入、蓄力和内部状态机消费。
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
	}
}

void UActionAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		// 已激活实例继续接收 GAS 标准输入释放事件，供状态型 Ability 或等待释放任务做正式收尾。
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
	}
}
