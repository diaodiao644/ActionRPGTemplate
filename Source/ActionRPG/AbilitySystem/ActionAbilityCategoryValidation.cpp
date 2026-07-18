// 文件说明：实现 Hero 主动战斗 GA 的类别/矩阵共享校验 helper。

#include "AbilitySystem/ActionAbilityCategoryValidation.h"

#include "AbilitySystem/Abilities/ActionGameplayAbilityBase.h"
#include "AbilitySystem/Abilities/Hero/ActionHeroGameplayAbility.h"
#include "Abilities/GameplayAbility.h"

namespace
{
	const FActionAbilityCategoryRelationshipRule* FindHeroCombatAbilityRelationshipRule(
		const EActionAbilityCategory InAbilityCategory)
	{
		static const FActionAbilityRelationshipMatrix Matrix =
			FActionAbilityRelationshipMatrix::BuildDefaultHeroCombatMatrix();
		return Matrix.FindRule(InAbilityCategory);
	}

	FString DescribeActionHeroCombatAbilityCategoryAuditState(
		const FActionAbilityCategoryAuditResult& AuditResult,
		const bool bHasBlueprintCanActivateOverride,
		const bool bUsesOnGivenActivationPolicy,
		const bool bSupportedNetExecutionPolicy,
		const bool bSupportedReplicationPolicy,
		const bool bSupportedInstancingPolicy,
		const bool bUsesRetriggerInstancedAbility,
		const bool bHasSourceOrTargetTagRequirements,
		const bool bHasValidMatrixRule)
	{
		if (bHasBlueprintCanActivateOverride)
		{
			return TEXT("UnsupportedBlueprintCanActivateOverride");
		}

		if (bUsesOnGivenActivationPolicy)
		{
			return TEXT("UnsupportedOnGivenActivationPolicy");
		}

		if (!bSupportedNetExecutionPolicy)
		{
			return TEXT("UnsupportedNetExecutionPolicy");
		}

		if (!bSupportedReplicationPolicy)
		{
			return TEXT("UnsupportedReplicationPolicy");
		}

		if (!bSupportedInstancingPolicy)
		{
			return TEXT("UnsupportedInstancingPolicy");
		}

		if (bUsesRetriggerInstancedAbility)
		{
			return TEXT("UnsupportedRetriggerInstancedAbility");
		}

		if (bHasSourceOrTargetTagRequirements)
		{
			return TEXT("UnsupportedSourceOrTargetTagRequirements");
		}

		if (AuditResult.HasMultipleTopLevelIdentityTags())
		{
			return TEXT("MultipleTopLevelIdentityTags");
		}

		if (AuditResult.IsMissingTopLevelIdentityTag())
		{
			return AuditResult.HasAdditionalPlayerAbilityTags()
				? TEXT("MissingTopLevelIdentityTagOnlyHasDetailTags")
				: TEXT("MissingTopLevelIdentityTag");
		}

		if (AuditResult.HasAdditionalPlayerAbilityTags())
		{
			return TEXT("UnexpectedAdditionalPlayerAbilityTags");
		}

		if (!IsValidActionAbilityCategory(AuditResult.ResolvedCategory))
		{
			return TEXT("ResolvedCategoryNone");
		}

		if (AuditResult.HasResolvedCategoryMismatch())
		{
			return TEXT("ResolvedCategoryMismatch");
		}

		if (!bHasValidMatrixRule)
		{
			return TEXT("MissingMatrixRule");
		}

		return TEXT("OK");
	}

	bool IsHeroCombatRelationshipAbilityRelevantForCategoryValidation(
		const UActionGameplayAbilityBase* InActionAbility)
	{
		return InActionAbility != nullptr
			&& InActionAbility->UsesAbilityRelationshipSystem()
			&& Cast<UActionHeroGameplayAbility>(InActionAbility) != nullptr;
	}

	FString BuildActionHeroCombatAbilityCategoryValidationFailureReason(
		const UActionGameplayAbilityBase* ActionAbility,
		const FActionAbilityCategoryAuditResult& AuditResult,
		const bool bHasBlueprintCanActivateOverride,
		const bool bUsesOnGivenActivationPolicy,
		const bool bSupportedNetExecutionPolicy,
		const bool bSupportedReplicationPolicy,
		const bool bSupportedInstancingPolicy,
		const bool bUsesRetriggerInstancedAbility,
		const bool bHasSourceOrTargetTagRequirements,
		const EActionAbilityCategory AbilityCategory,
		const bool bHasValidMatrixRule)
	{
		if (bHasBlueprintCanActivateOverride)
		{
			return TEXT("grant rejected because Hero combat relationship abilities must not override Blueprint CanActivateAbility; that path is not part of the formal C++ relationship and drift-diagnostic authority chain.");
		}

		if (bUsesOnGivenActivationPolicy)
		{
			return TEXT("grant rejected because Hero combat relationship abilities must not use ActivationPolicy=OnGiven; that path would bypass TryActivateAbilitySpecWithRelationship().");
		}

		if (!bSupportedNetExecutionPolicy)
		{
			return FString::Printf(
				TEXT("grant rejected because Hero combat relationship abilities must use NetExecutionPolicy=LocalPredicted. Actual=%s"),
				*UEnum::GetValueAsString(ActionAbility ? ActionAbility->GetNetExecutionPolicy() : EGameplayAbilityNetExecutionPolicy::LocalPredicted));
		}

		if (!bSupportedReplicationPolicy)
		{
			return FString::Printf(
				TEXT("grant rejected because Hero combat relationship abilities must use ReplicationPolicy=ReplicateNo. Actual=%s"),
				*UEnum::GetValueAsString(ActionAbility ? ActionAbility->GetReplicationPolicy() : EGameplayAbilityReplicationPolicy::ReplicateNo));
		}

		if (!bSupportedInstancingPolicy)
		{
			return FString::Printf(
				TEXT("grant rejected because Hero combat relationship abilities must use InstancingPolicy=InstancedPerActor. Actual=%s"),
				*UEnum::GetValueAsString(ActionAbility ? ActionAbility->GetInstancingPolicy() : EGameplayAbilityInstancingPolicy::InstancedPerActor));
		}

		if (bUsesRetriggerInstancedAbility)
		{
			return TEXT("grant rejected because Hero combat relationship abilities must not enable bRetriggerInstancedAbility; that shape would bypass the current single-owner relationship activation authority chain.");
		}

		if (bHasSourceOrTargetTagRequirements)
		{
			return FString::Printf(
				TEXT("grant rejected because Hero combat relationship abilities must not use Source/Target activation tag requirements. Actual=%s"),
				ActionAbility ? *ActionAbility->DescribeSourceAndTargetActivationTagRequirements() : TEXT("None"));
		}

		if (AuditResult.HasMultipleTopLevelIdentityTags())
		{
			return FString::Printf(
				TEXT("grant rejected because multiple top-level Player.Ability identity tags are configured: %s"),
				*DescribeActionAbilityValidationGameplayTags(AuditResult.MatchedTopLevelIdentityTags));
		}

		if (AuditResult.IsMissingTopLevelIdentityTag())
		{
			return AuditResult.HasAdditionalPlayerAbilityTags()
				? FString::Printf(
					TEXT("grant rejected because no top-level Player.Ability identity tag is configured. Only detail tags were found: %s"),
					*DescribeActionAbilityValidationGameplayTags(AuditResult.AdditionalPlayerAbilityTags))
				: TEXT("grant rejected because no top-level Player.Ability identity tag is configured.");
		}

		if (AuditResult.HasAdditionalPlayerAbilityTags())
		{
			return FString::Printf(
				TEXT("grant rejected because unexpected Player.Ability detail tags are configured alongside the top-level identity tag: %s"),
				*DescribeActionAbilityValidationGameplayTags(AuditResult.AdditionalPlayerAbilityTags));
		}

		if (!IsValidActionAbilityCategory(AbilityCategory))
		{
			return TEXT("grant rejected because ability category resolved to None.");
		}

		if (AuditResult.HasResolvedCategoryMismatch())
		{
			return FString::Printf(
				TEXT("grant rejected because resolved category %s does not match the expected category %s from the top-level identity tag."),
				*ActionAbilityCategoryToString(AbilityCategory),
				*ActionAbilityCategoryToString(AuditResult.ExpectedCategoryFromTopLevelIdentity));
		}

		if (!bHasValidMatrixRule)
		{
			return FString::Printf(
				TEXT("grant rejected because no relationship matrix rule exists for category %s."),
				*ActionAbilityCategoryToString(AbilityCategory));
		}

		return FString();
	}
}

FString DescribeActionAbilityValidationGameplayTags(const FGameplayTagContainer& InTags)
{
	TArray<FString> TagNames;
	TagNames.Reserve(InTags.Num());

	for (const FGameplayTag& Tag : InTags)
	{
		TagNames.Add(Tag.ToString());
	}

	return TagNames.Num() > 0 ? FString::Join(TagNames, TEXT(", ")) : TEXT("None");
}

FString DescribeActionAbilityValidationGameplayTags(const TArray<FGameplayTag>& InTags)
{
	TArray<FString> TagNames;
	TagNames.Reserve(InTags.Num());

	for (const FGameplayTag& Tag : InTags)
	{
		TagNames.Add(Tag.ToString());
	}

	return TagNames.Num() > 0 ? FString::Join(TagNames, TEXT(", ")) : TEXT("None");
}

FActionHeroCombatAbilityCategoryValidationResult
BuildActionHeroCombatAbilityCategoryValidationResult(const UGameplayAbility* InAbility)
{
	FActionHeroCombatAbilityCategoryValidationResult ValidationResult;

	const UActionGameplayAbilityBase* ActionAbility = Cast<UActionGameplayAbilityBase>(InAbility);
	if (!IsHeroCombatRelationshipAbilityRelevantForCategoryValidation(ActionAbility))
	{
		return ValidationResult;
	}

	ValidationResult.bIsHeroCombatRelationshipAbility = true;
	ValidationResult.IdentityTags = ActionAbility->GetAbilityIdentityTags();
	ValidationResult.bHasBlueprintCanActivateOverride =
		ActionAbility->HasBlueprintCanActivateOverride();
	ValidationResult.bUsesOnGivenActivationPolicy =
		ActionAbility->UsesOnGivenActivationPolicy();
	ValidationResult.bSupportedNetExecutionPolicy =
		ActionAbility->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ValidationResult.bSupportedReplicationPolicy =
		ActionAbility->GetReplicationPolicy() == EGameplayAbilityReplicationPolicy::ReplicateNo;
	ValidationResult.bSupportedInstancingPolicy =
		ActionAbility->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ValidationResult.bUsesRetriggerInstancedAbility =
		ActionAbility->UsesRetriggerInstancedAbility();
	ValidationResult.bHasSourceOrTargetTagRequirements =
		ActionAbility->HasSourceOrTargetActivationTagRequirements();
	ValidationResult.CategoryAuditResult =
		AuditActionAbilityCategoryFromIdentityTags(ValidationResult.IdentityTags);
	ValidationResult.AbilityCategory = ActionAbility->GetAbilityCategory();
	ValidationResult.bMatrixRuleValid =
		FindHeroCombatAbilityRelationshipRule(ValidationResult.AbilityCategory) != nullptr;
	ValidationResult.AuditState = DescribeActionHeroCombatAbilityCategoryAuditState(
		ValidationResult.CategoryAuditResult,
		ValidationResult.bHasBlueprintCanActivateOverride,
		ValidationResult.bUsesOnGivenActivationPolicy,
		ValidationResult.bSupportedNetExecutionPolicy,
		ValidationResult.bSupportedReplicationPolicy,
		ValidationResult.bSupportedInstancingPolicy,
		ValidationResult.bUsesRetriggerInstancedAbility,
		ValidationResult.bHasSourceOrTargetTagRequirements,
		ValidationResult.bMatrixRuleValid);
	ValidationResult.FailureReason = BuildActionHeroCombatAbilityCategoryValidationFailureReason(
		ActionAbility,
		ValidationResult.CategoryAuditResult,
		ValidationResult.bHasBlueprintCanActivateOverride,
		ValidationResult.bUsesOnGivenActivationPolicy,
		ValidationResult.bSupportedNetExecutionPolicy,
		ValidationResult.bSupportedReplicationPolicy,
		ValidationResult.bSupportedInstancingPolicy,
		ValidationResult.bUsesRetriggerInstancedAbility,
		ValidationResult.bHasSourceOrTargetTagRequirements,
		ValidationResult.AbilityCategory,
		ValidationResult.bMatrixRuleValid);
	ValidationResult.bPassed = ValidationResult.AuditState == TEXT("OK");
	return ValidationResult;
}

FActionHeroCombatAbilityCategoryValidationResult
BuildActionHeroCombatAbilityCategoryValidationResult(
	const TSubclassOf<UGameplayAbility>& InAbilityClass)
{
	return BuildActionHeroCombatAbilityCategoryValidationResult(
		InAbilityClass ? InAbilityClass->GetDefaultObject<UGameplayAbility>() : nullptr);
}
