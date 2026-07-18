// 文件说明：声明 Hero 主动战斗 GA 的类别/矩阵共享校验 helper。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionAbilityTypes.h"

class UGameplayAbility;

/**
 * Hero 主动战斗关系型 GA 的类别校验结果。
 * 这份结构只服务授予期硬校验、editor-only 审计和资产校验报告，
 * 不替代 GetAbilityCategory() 或关系矩阵本身的正式权威。
 */
struct FActionHeroCombatAbilityCategoryValidationResult
{
	bool bIsHeroCombatRelationshipAbility = false;
	bool bHasBlueprintCanActivateOverride = false;
	bool bUsesOnGivenActivationPolicy = false;
	bool bMatrixRuleValid = false;
	bool bSupportedNetExecutionPolicy = true;
	bool bSupportedReplicationPolicy = true;
	bool bSupportedInstancingPolicy = true;
	bool bUsesRetriggerInstancedAbility = false;
	bool bHasSourceOrTargetTagRequirements = false;
	bool bPassed = false;
	FGameplayTagContainer IdentityTags;
	FActionAbilityCategoryAuditResult CategoryAuditResult;
	EActionAbilityCategory AbilityCategory = EActionAbilityCategory::None;
	FString AuditState = TEXT("NotHeroCombatRelationshipAbility");
	FString FailureReason;
};

ACTIONRPG_API FString DescribeActionAbilityValidationGameplayTags(
	const FGameplayTagContainer& InTags);

ACTIONRPG_API FString DescribeActionAbilityValidationGameplayTags(
	const TArray<FGameplayTag>& InTags);

ACTIONRPG_API FActionHeroCombatAbilityCategoryValidationResult
BuildActionHeroCombatAbilityCategoryValidationResult(const UGameplayAbility* InAbility);

ACTIONRPG_API FActionHeroCombatAbilityCategoryValidationResult
BuildActionHeroCombatAbilityCategoryValidationResult(
	const TSubclassOf<UGameplayAbility>& InAbilityClass);
