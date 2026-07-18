#pragma once

#include "CoreMinimal.h"
#include "ActionGameplayTags.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ActionAbilityTypes.generated.h"

/**
 * Ability 激活语义公共类型。
 * 说明：
 * 1. 这里只描述“这类 Ability 通常通过什么入口被激活”；
 * 2. 它主要服务静态资产配置、授予链解释和关系系统读法统一；
 * 3. 它不是运行时激活状态机，也不回答某个 Ability 当前是否已经激活。
 */
UENUM(BlueprintType)
enum class EActionAbilityActivationPolicy : uint8
{
	OnInput,               /* 只通过正式输入标签触发。这是激活语义分类，不是运行时激活状态。 */
	OnTriggered,           /* 只通过事件或外部显式触发。这是激活语义分类，不是运行时激活状态。 */
	BothInputAndTriggered, /* 同时支持输入和事件触发。这是激活语义分类，不是运行时激活状态。 */
	OnGiven                /* 授予后立即尝试激活。这是激活语义分类，不等于一定激活成功。 */
};

/**
 * 主动 GA 关系系统里的正式能力类别。
 * 它只回答“这条 Ability 在统一矩阵里属于哪一类”，
 * 不等于具体输入标签，也不等于某次激活局部时序。
 */
UENUM(BlueprintType)
enum class EActionAbilityCategory : uint8
{
	None UMETA(Hidden),
	Attack,
	SpiritSkill,
	CombatModeOrDefense,
	Dodge,
	WeaponSwitch,
	Execution,
	ProjectileSwitch
};

/**
 * 单个能力类别在统一关系矩阵里的默认静态规则。
 * 它只服务主动 GA 关系裁决，不承担 CombatReact、输入缓冲或动画窗口 runtime。
 */
USTRUCT(BlueprintType)
struct FActionAbilityCategoryRelationshipRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	int32 Priority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanInterruptLowerPriorityAbilities = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanInterruptSamePriorityAbilities = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanBeInterruptedByHigherPriority = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	bool bCanBeInterruptedBySamePriority = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityRelationship")
	TArray<EActionAbilityCategory> DefaultCancelableCategories;

	bool CanCancelCategory(const EActionAbilityCategory InCategory) const
	{
		return InCategory != EActionAbilityCategory::None
			&& DefaultCancelableCategories.Contains(InCategory);
	}
};

/**
 * 主动 GA 关系系统的统一类别矩阵。
 * v1 先由 C++ 固定表提供默认值，不在这里引入新的配置资产。
 */
USTRUCT(BlueprintType)
struct FActionAbilityRelationshipMatrix
{
	GENERATED_BODY()

public:
	const FActionAbilityCategoryRelationshipRule* FindRule(const EActionAbilityCategory InCategory) const
	{
		return Rules.Find(InCategory);
	}

	void SetRule(
		const EActionAbilityCategory InCategory,
		const FActionAbilityCategoryRelationshipRule& InRule)
	{
		Rules.FindOrAdd(InCategory) = InRule;
	}

	static FActionAbilityRelationshipMatrix BuildDefaultHeroCombatMatrix()
	{
		// Hero 主动战斗 GA 的长期默认关系只从这张类别矩阵读取。
		// 它只定义“候选类别默认如何对待当前活跃类别”：
		// 1. 谁的默认优先级更高；
		// 2. 高低优先级之间默认能否互断；
		// 3. 候选类别默认允许取消哪些活跃类别。
		// 它不负责动画帧时机、输入层白名单或 AbilityInterruptWindow 例外开窗。
		// DefaultCancelableCategories 的读法始终是“候选类别默认可取消哪些活跃类别”，不是双向关系声明。
		FActionAbilityRelationshipMatrix Matrix;

		FActionAbilityCategoryRelationshipRule Rule;

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 8;
		Rule.bCanInterruptLowerPriorityAbilities = false;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		// ProjectileSwitch 继续维持低优先级便捷动作语义：
		// 默认不主动接管其它主动 GA，也默认需要给更高优先级动作让路。
		Matrix.SetRule(EActionAbilityCategory::ProjectileSwitch, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 18;
		Rule.bCanInterruptLowerPriorityAbilities = false;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		Matrix.SetRule(EActionAbilityCategory::Attack, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 20;
		Rule.bCanInterruptLowerPriorityAbilities = true;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		Rule.DefaultCancelableCategories = { EActionAbilityCategory::Attack };
		// SpiritSkill 的正式语义是“默认高于 Attack，但只默认接管 Attack”。
		// 它不顺手放宽到其它低优先级类别，避免 Spirit 默认接管范围继续膨胀。
		Matrix.SetRule(EActionAbilityCategory::SpiritSkill, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 25;
		Rule.bCanInterruptLowerPriorityAbilities = true;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		Rule.DefaultCancelableCategories = { EActionAbilityCategory::Attack };
		// CombatModeOrDefense 默认只从 Attack 手里接管。
		// Idle -> Combo 的普通姿态切入与 Combo -> Defense 的正式防御共用这条类别规则，
		// 但它们都不依赖输入层白名单去定义长期关系。
		Matrix.SetRule(EActionAbilityCategory::CombatModeOrDefense, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 26;
		Rule.bCanInterruptLowerPriorityAbilities = true;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		Rule.DefaultCancelableCategories =
		{
			EActionAbilityCategory::Attack,
			EActionAbilityCategory::CombatModeOrDefense
		};
		// WeaponSwitch 现在是主动接管型类别：
		// 它默认可以从 Attack / CombatModeOrDefense 抢入，
		// 但普通切武的后续表现仍由切武链自己处理，不在矩阵里表达。
		Matrix.SetRule(EActionAbilityCategory::WeaponSwitch, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 30;
		Rule.bCanInterruptLowerPriorityAbilities = true;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = true;
		Rule.bCanBeInterruptedBySamePriority = false;
		Rule.DefaultCancelableCategories =
		{
			EActionAbilityCategory::Attack,
			EActionAbilityCategory::CombatModeOrDefense
		};
		// Dodge 维持高优先级主动接管语义：
		// 默认直接压过 Attack / CombatModeOrDefense，
		// 不要求对方先开低优先级 interrupt-window 才能进入 ASC 关系裁决。
		Matrix.SetRule(EActionAbilityCategory::Dodge, Rule);

		Rule = FActionAbilityCategoryRelationshipRule();
		Rule.Priority = 40;
		Rule.bCanInterruptLowerPriorityAbilities = true;
		Rule.bCanInterruptSamePriorityAbilities = false;
		Rule.bCanBeInterruptedByHigherPriority = false;
		Rule.bCanBeInterruptedBySamePriority = false;
		Rule.DefaultCancelableCategories =
		{
			EActionAbilityCategory::Attack,
			EActionAbilityCategory::CombatModeOrDefense,
			EActionAbilityCategory::Dodge,
			EActionAbilityCategory::SpiritSkill,
			EActionAbilityCategory::WeaponSwitch,
			EActionAbilityCategory::ProjectileSwitch
		};
		// Execution 是最高优先级强接管类别：
		// 默认接管其它主动 GA，且默认不接受反向打断。
		// 这里表达的是长期默认关系，不依赖共享缓冲或普通 interrupt-window 解释自己的权威性。
		Matrix.SetRule(EActionAbilityCategory::Execution, Rule);

		return Matrix;
	}

private:
	TMap<EActionAbilityCategory, FActionAbilityCategoryRelationshipRule> Rules;
};

inline bool IsValidActionAbilityCategory(const EActionAbilityCategory InCategory)
{
	return InCategory != EActionAbilityCategory::None;
}

inline FString ActionAbilityCategoryToString(const EActionAbilityCategory InCategory)
{
	if (const UEnum* CategoryEnum = StaticEnum<EActionAbilityCategory>())
	{
		return CategoryEnum->GetNameStringByValue(static_cast<int64>(InCategory));
	}

	return TEXT("None");
}

inline FGameplayTag GetActionAbilityIdentityTagForCategory(const EActionAbilityCategory InCategory)
{
	switch (InCategory)
	{
	case EActionAbilityCategory::Attack:
		return ActionGameplayTags::Player_Ability_Attack;

	case EActionAbilityCategory::SpiritSkill:
		return ActionGameplayTags::Player_Ability_SpiritSkill;

	case EActionAbilityCategory::CombatModeOrDefense:
		return ActionGameplayTags::Player_Ability_CombatModeOrDefense;

	case EActionAbilityCategory::Dodge:
		return ActionGameplayTags::Player_Ability_Dodge;

	case EActionAbilityCategory::WeaponSwitch:
		return ActionGameplayTags::Player_Ability_WeaponSwitch;

	case EActionAbilityCategory::Execution:
		return ActionGameplayTags::Player_Ability_Execution;

	case EActionAbilityCategory::ProjectileSwitch:
		return ActionGameplayTags::Player_Ability_ProjectileSwitch;

	default:
		return FGameplayTag();
	}
}

inline const TArray<FGameplayTag>& GetActionAbilityTopLevelIdentityTags()
{
	static const TArray<FGameplayTag> IdentityTags =
	{
		ActionGameplayTags::Player_Ability_ProjectileSwitch,
		ActionGameplayTags::Player_Ability_Attack,
		ActionGameplayTags::Player_Ability_SpiritSkill,
		ActionGameplayTags::Player_Ability_CombatModeOrDefense,
		ActionGameplayTags::Player_Ability_WeaponSwitch,
		ActionGameplayTags::Player_Ability_Dodge,
		ActionGameplayTags::Player_Ability_Execution
	};

	return IdentityTags;
}

inline EActionAbilityCategory ResolveActionAbilityCategoryFromTopLevelIdentityTag(const FGameplayTag& InIdentityTag)
{
	if (InIdentityTag == ActionGameplayTags::Player_Ability_Attack)
	{
		return EActionAbilityCategory::Attack;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_SpiritSkill)
	{
		return EActionAbilityCategory::SpiritSkill;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_CombatModeOrDefense)
	{
		return EActionAbilityCategory::CombatModeOrDefense;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_Dodge)
	{
		return EActionAbilityCategory::Dodge;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_WeaponSwitch)
	{
		return EActionAbilityCategory::WeaponSwitch;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_Execution)
	{
		return EActionAbilityCategory::Execution;
	}

	if (InIdentityTag == ActionGameplayTags::Player_Ability_ProjectileSwitch)
	{
		return EActionAbilityCategory::ProjectileSwitch;
	}

	return EActionAbilityCategory::None;
}

inline EActionAbilityCategory ResolveActionAbilityCategoryFromAbilityIdentityTags(
	const FGameplayTagContainer& InAbilityIdentityTags)
{
	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_Attack))
	{
		return EActionAbilityCategory::Attack;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_SpiritSkill))
	{
		return EActionAbilityCategory::SpiritSkill;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_CombatModeOrDefense))
	{
		return EActionAbilityCategory::CombatModeOrDefense;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_Dodge))
	{
		return EActionAbilityCategory::Dodge;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_WeaponSwitch))
	{
		return EActionAbilityCategory::WeaponSwitch;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_Execution))
	{
		return EActionAbilityCategory::Execution;
	}

	if (InAbilityIdentityTags.HasTagExact(ActionGameplayTags::Player_Ability_ProjectileSwitch))
	{
		return EActionAbilityCategory::ProjectileSwitch;
	}

	return EActionAbilityCategory::None;
}

inline EActionAbilityCategory ResolveActionAbilityCategoryFromInputTag(const FGameplayTag& InInputTag)
{
	if (!InInputTag.IsValid())
	{
		return EActionAbilityCategory::None;
	}

	if (InInputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Attack))
	{
		return EActionAbilityCategory::Attack;
	}

	if (ActionGameplayTags::IsSpiritSkillInputTag(InInputTag))
	{
		return EActionAbilityCategory::SpiritSkill;
	}

	if (InInputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense))
	{
		return EActionAbilityCategory::CombatModeOrDefense;
	}

	if (InInputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Dodge))
	{
		return EActionAbilityCategory::Dodge;
	}

	if (InInputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSwitch
		|| InInputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Unarmed
		|| InInputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Melee
		|| InInputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Ranged
		|| InInputTag == ActionGameplayTags::InputTag_GameplayAbility_WeaponSlot_Hybrid)
	{
		return EActionAbilityCategory::WeaponSwitch;
	}

	if (InInputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Execution))
	{
		return EActionAbilityCategory::Execution;
	}

	if (InInputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch))
	{
		return EActionAbilityCategory::ProjectileSwitch;
	}

	return EActionAbilityCategory::None;
}

/**
 * Ability 类别审计结果。
 * 它只服务调试解释，帮助指出身份标签缺失、多标签冲突或只挂了 Player.Ability 子标签等问题，
 * 不替代正式的 GetAbilityCategory() 权威结果。
 */
struct FActionAbilityCategoryAuditResult
{
	EActionAbilityCategory ResolvedCategory = EActionAbilityCategory::None;
	EActionAbilityCategory ExpectedCategoryFromTopLevelIdentity = EActionAbilityCategory::None;
	TArray<FGameplayTag> MatchedTopLevelIdentityTags;
	TArray<FGameplayTag> AdditionalPlayerAbilityTags;

	bool HasMatchedTopLevelIdentityTag() const
	{
		return MatchedTopLevelIdentityTags.Num() > 0;
	}

	bool HasMultipleTopLevelIdentityTags() const
	{
		return MatchedTopLevelIdentityTags.Num() > 1;
	}

	bool HasAdditionalPlayerAbilityTags() const
	{
		return AdditionalPlayerAbilityTags.Num() > 0;
	}

	bool IsMissingTopLevelIdentityTag() const
	{
		return MatchedTopLevelIdentityTags.Num() <= 0;
	}

	bool HasResolvedCategoryMismatch() const
	{
		return IsValidActionAbilityCategory(ExpectedCategoryFromTopLevelIdentity)
			&& ResolvedCategory != ExpectedCategoryFromTopLevelIdentity;
	}
};

inline FActionAbilityCategoryAuditResult AuditActionAbilityCategoryFromIdentityTags(
	const FGameplayTagContainer& InAbilityIdentityTags)
{
	FActionAbilityCategoryAuditResult AuditResult;
	AuditResult.ResolvedCategory = ResolveActionAbilityCategoryFromAbilityIdentityTags(InAbilityIdentityTags);

	for (const FGameplayTag& IdentityTag : GetActionAbilityTopLevelIdentityTags())
	{
		if (InAbilityIdentityTags.HasTagExact(IdentityTag))
		{
			AuditResult.MatchedTopLevelIdentityTags.Add(IdentityTag);
		}
	}

	if (AuditResult.MatchedTopLevelIdentityTags.Num() == 1)
	{
		AuditResult.ExpectedCategoryFromTopLevelIdentity =
			ResolveActionAbilityCategoryFromTopLevelIdentityTag(
				AuditResult.MatchedTopLevelIdentityTags[0]);
	}

	const FGameplayTag PlayerAbilityRootTag =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Player.Ability")), false);
	if (!PlayerAbilityRootTag.IsValid())
	{
		return AuditResult;
	}

	for (const FGameplayTag& AbilityIdentityTag : InAbilityIdentityTags)
	{
		if (!AbilityIdentityTag.MatchesTag(PlayerAbilityRootTag))
		{
			continue;
		}

		if (AuditResult.MatchedTopLevelIdentityTags.Contains(AbilityIdentityTag))
		{
			continue;
		}

		bool bIsChildOfMatchedTopLevelIdentityTag = false;
		for (const FGameplayTag& MatchedIdentityTag : AuditResult.MatchedTopLevelIdentityTags)
		{
			if (AbilityIdentityTag.MatchesTag(MatchedIdentityTag))
			{
				bIsChildOfMatchedTopLevelIdentityTag = true;
				break;
			}
		}

		if (!bIsChildOfMatchedTopLevelIdentityTag)
		{
			AuditResult.AdditionalPlayerAbilityTags.AddUnique(AbilityIdentityTag);
		}
	}

	return AuditResult;
}

inline void AppendResolvedAbilityCategoriesFromInputTags(
	const FGameplayTagContainer& InAllowedInputTags,
	TArray<EActionAbilityCategory>& OutAllowedCategories)
{
	OutAllowedCategories.Reset();

	for (const FGameplayTag& InputTag : InAllowedInputTags)
	{
		const EActionAbilityCategory ResolvedCategory = ResolveActionAbilityCategoryFromInputTag(InputTag);
		if (IsValidActionAbilityCategory(ResolvedCategory))
		{
			OutAllowedCategories.AddUnique(ResolvedCategory);
		}
	}
}

/**
 * Ability 静态授予模板。
 * 作用：
 * 1. 作为数据资产侧对 ASC 授予能力的最小配置单元；
 * 2. 统一描述“输入标签 + Ability 类”这一对静态授予关系；
 * 3. 装备槽、角色默认能力和其他可扩展能力集都可以复用这份结构；
 * 4. 它不是已授予快照、已激活快照，也不是正式输入运行态。
 */
USTRUCT(BlueprintType)
struct FActionAbilitySet
{
	GENERATED_BODY()

public:
	/** 这条静态授予模板对应的正式输入标签。它只描述授予后应该挂在哪个输入语义下，不等于当前输入正在按住或已被消费。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ToolTip = "这条 Ability 在授予链里绑定到哪个正式输入语义。它是静态授予模板字段，不等于当前输入运行态；留空时这条配置不会进入正式能力集。"))
	FGameplayTag InputTag;

	/** 这条模板最终要授予到 ASC 的 Ability 类。它只描述能力类型，不替代输入标签，也不是已授予实例快照。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ToolTip = "真正授予到 ASC 的 GameplayAbility 类。它只描述静态能力类型，不替代输入标签，也不是已授予实例快照；留空时这条配置无效。"))
	TSubclassOf<UGameplayAbility> AbilityToGrant;

	/** 判断这条静态授予模板是否完整，避免把空 Ability 或空输入标签写入授予链。它只做模板最小校验，不替代 ASC 正式授予校验、关系裁决或激活资格判断。 */
	bool IsValid() const
	{
		return InputTag.IsValid() && AbilityToGrant;
	}
};
