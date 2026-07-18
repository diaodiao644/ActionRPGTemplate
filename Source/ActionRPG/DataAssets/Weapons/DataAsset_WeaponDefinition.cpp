#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilityCategoryValidation.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_SpiritSkill.h"
#include "AnimNotify/AnimNotify_CombatWeaponPresentationSwitch.h"
#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"
#include "DataAssets/Effects/DataAsset_ActionHitEffectDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponDefinitionData, Log, All);

namespace ActionWeaponDefinitionDefaults
{
	// 这一组 helper 统一把类别、请求标签和 subtype 语义翻译成推荐静态配置与校验文案。
	// 它们只服务 WeaponDefinition 资产整理与校验，不持有任何运行时正式状态。
	static FString GetRequestSemanticName(const FGameplayTag& InRequestTag)
	{
		if (InRequestTag == ActionGameplayTags::Attack_Request_Default)
		{
			return TEXT("默认攻击请求");
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Held)
		{
			return TEXT("长按重击请求");
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter)
		{
			return TEXT("闪避反击请求");
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Sprint)
		{
			return TEXT("冲刺攻击请求");
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Airborne)
		{
			return TEXT("空中攻击请求");
		}

		return InRequestTag.ToString();
	}

	static FString GetBranchSemanticName(const FGameplayTag& InBranchTag)
	{
		if (InBranchTag == ActionGameplayTags::Attack_Branch_Light)
		{
			return TEXT("轻攻击分支");
		}

		if (InBranchTag == ActionGameplayTags::Attack_Branch_Heavy)
		{
			return TEXT("重攻击分支");
		}

		if (InBranchTag == ActionGameplayTags::Attack_Branch_DodgeCounter)
		{
			return TEXT("闪避反击分支");
		}

		if (InBranchTag == ActionGameplayTags::Attack_Branch_Sprint)
		{
			return TEXT("冲刺攻击分支");
		}

		if (InBranchTag == ActionGameplayTags::Attack_Branch_Airborne)
		{
			return TEXT("空中攻击分支");
		}

		return InBranchTag.ToString();
	}

	static FString GetWeaponCategoryName(const EHeroWeaponCategory InWeaponCategory)
	{
		if (const UEnum* WeaponCategoryEnum = StaticEnum<EHeroWeaponCategory>())
		{
			return WeaponCategoryEnum->GetNameStringByValue(static_cast<int64>(InWeaponCategory));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InWeaponCategory));
	}

	static FString GetWeaponPropertyTypeName(const EActionWeaponPropertyType InWeaponPropertyType)
	{
		if (const UEnum* WeaponPropertyTypeEnum = StaticEnum<EActionWeaponPropertyType>())
		{
			return WeaponPropertyTypeEnum->GetNameStringByValue(static_cast<int64>(InWeaponPropertyType));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InWeaponPropertyType));
	}

	static FString GetSpiritEntryKindName(const EActionSpiritAbilityEntryKind InEntryKind)
	{
		switch (InEntryKind)
		{
		case EActionSpiritAbilityEntryKind::GrantedAbilityOnly:
			return TEXT("GrantedAbilityOnly");
		case EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance:
			return TEXT("SpiritSkillSelfEnhance");
		case EActionSpiritAbilityEntryKind::SpiritSkillOffensive:
			return TEXT("SpiritSkillOffensive");
		default:
			break;
		}

		return TEXT("UnknownSpiritEntryKind");
	}

	static FString GetDamageTypeName(const EActionDamageType InDamageType)
	{
		if (const UEnum* DamageTypeEnum = StaticEnum<EActionDamageType>())
		{
			return DamageTypeEnum->GetNameStringByValue(static_cast<int64>(InDamageType));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InDamageType));
	}

	static void ValidateHeroCombatAbilityCategory(
		const TSubclassOf<UGameplayAbility>& InAbilityClass,
		const FString& InContextText,
		TArray<FString>& OutLines)
	{
		if (!InAbilityClass)
		{
			return;
		}

		const FActionHeroCombatAbilityCategoryValidationResult ValidationResult =
			BuildActionHeroCombatAbilityCategoryValidationResult(InAbilityClass);
		if (!ValidationResult.bIsHeroCombatRelationshipAbility || ValidationResult.bPassed)
		{
			return;
		}

		OutLines.Add(FString::Printf(
			TEXT("[错误] %s 引用的 Hero 主动战斗 GA 类别配置非法。Ability=%s Identity=%s MatchedTopLevelIdentity=%s AdditionalPlayerAbilityTags=%s Category=%s HasBlueprintCanActivateOverride=%s CategoryAudit=%s Reason=%s"),
			*InContextText,
			*GetNameSafe(InAbilityClass),
			*DescribeActionAbilityValidationGameplayTags(ValidationResult.IdentityTags),
			*DescribeActionAbilityValidationGameplayTags(
				ValidationResult.CategoryAuditResult.MatchedTopLevelIdentityTags),
			*DescribeActionAbilityValidationGameplayTags(
				ValidationResult.CategoryAuditResult.AdditionalPlayerAbilityTags),
			*ActionAbilityCategoryToString(ValidationResult.AbilityCategory),
			ValidationResult.bHasBlueprintCanActivateOverride ? TEXT("Yes") : TEXT("No"),
			*ValidationResult.AuditState,
			ValidationResult.FailureReason.IsEmpty() ? TEXT("Unknown") : *ValidationResult.FailureReason));
	}

	static EActionDamageType ResolveExpectedDamageTypeByWeaponPropertyType(const EActionWeaponPropertyType InWeaponPropertyType)
	{
		switch (InWeaponPropertyType)
		{
		case EActionWeaponPropertyType::Elemental:
			return EActionDamageType::Elemental;

		case EActionWeaponPropertyType::Mundane:
		case EActionWeaponPropertyType::Spirit:
		default:
			return EActionDamageType::Physical;
		}
	}

	static FGameplayTag ResolveExpectedWeaponRootTag(const EHeroWeaponCategory InWeaponCategory)
	{
		switch (InWeaponCategory)
		{
		case EHeroWeaponCategory::Unarmed:
			return ActionGameplayTags::Player_Weapon_Unarmed;

		case EHeroWeaponCategory::PureMelee:
			return ActionGameplayTags::Player_Weapon_Melee;

		case EHeroWeaponCategory::PureRanged:
			return ActionGameplayTags::Player_Weapon_Ranged;

		case EHeroWeaponCategory::MeleeRangedHybrid:
			return ActionGameplayTags::Player_Weapon_Hybrid;

		default:
			break;
		}

		return FGameplayTag();
	}

	static FString GetExpectedWeaponRootTagName(const EHeroWeaponCategory InWeaponCategory)
	{
		const FGameplayTag ExpectedRootTag = ResolveExpectedWeaponRootTag(InWeaponCategory);

		return ExpectedRootTag.IsValid() ? ExpectedRootTag.ToString() : TEXT("Unknown");
	}

	static FGameplayTag ResolveRecommendedWeaponSubtypeTag(const EHeroWeaponCategory InWeaponCategory)
	{
		switch (InWeaponCategory)
		{
		case EHeroWeaponCategory::Unarmed:
			return ActionGameplayTags::Player_Weapon_Unarmed;

		case EHeroWeaponCategory::PureMelee:
			return ActionGameplayTags::Player_Weapon_Melee_Sword;

		case EHeroWeaponCategory::PureRanged:
			return ActionGameplayTags::Player_Weapon_Ranged_Staff;

		case EHeroWeaponCategory::MeleeRangedHybrid:
			return ActionGameplayTags::Player_Weapon_Hybrid_Sword;

		default:
			break;
		}

		return FGameplayTag();
	}

	static bool IsAllowedWeaponSubtypeTag(
		const EHeroWeaponCategory InWeaponCategory,
		const FGameplayTag& InWeaponSubtypeTag)
	{
		if (!InWeaponSubtypeTag.IsValid())
		{
			return false;
		}

		switch (InWeaponCategory)
		{
		case EHeroWeaponCategory::Unarmed:
			return InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Unarmed;

		case EHeroWeaponCategory::PureMelee:
			return InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Sword
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Blade
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Spear
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Scythe;

		case EHeroWeaponCategory::PureRanged:
			return InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Staff
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Bow
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Gun;

		case EHeroWeaponCategory::MeleeRangedHybrid:
			return InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Sword
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Blade
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Spear
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Staff
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Bow
				|| InWeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Gun;

		default:
			break;
		}

		return false;
	}

	static FGameplayTag ResolveRecommendedWeaponTag(
		const EHeroWeaponCategory InWeaponCategory,
		const FGameplayTag& InWeaponSubtypeTag)
	{
		const FGameplayTag ResolvedSubtypeTag = IsAllowedWeaponSubtypeTag(InWeaponCategory, InWeaponSubtypeTag)
			? InWeaponSubtypeTag
			: ResolveRecommendedWeaponSubtypeTag(InWeaponCategory);

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Unarmed)
		{
			return ActionGameplayTags::Player_Weapon_Unarmed_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Sword)
		{
			return ActionGameplayTags::Player_Weapon_Melee_Sword_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Blade)
		{
			return ActionGameplayTags::Player_Weapon_Melee_Blade_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Spear)
		{
			return ActionGameplayTags::Player_Weapon_Melee_Spear_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Melee_Scythe)
		{
			return ActionGameplayTags::Player_Weapon_Melee_Scythe_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Staff)
		{
			return ActionGameplayTags::Player_Weapon_Ranged_Staff_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Bow)
		{
			return ActionGameplayTags::Player_Weapon_Ranged_Bow_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Gun)
		{
			return ActionGameplayTags::Player_Weapon_Ranged_Gun_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Sword)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Sword_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Blade)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Blade_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Spear)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Spear_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Staff)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Staff_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Bow)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Bow_Default;
		}

		if (ResolvedSubtypeTag == ActionGameplayTags::Player_Weapon_Hybrid_Gun)
		{
			return ActionGameplayTags::Player_Weapon_Hybrid_Gun_Default;
		}

		return FGameplayTag();
	}

	static TSoftClassPtr<AHeroWeaponBase> ResolveRecommendedWeaponActorClass(const EHeroWeaponCategory InWeaponCategory)
	{
		return InWeaponCategory == EHeroWeaponCategory::Unarmed
			? TSoftClassPtr<AHeroWeaponBase>()
			: TSoftClassPtr<AHeroWeaponBase>(FSoftClassPath(TEXT("/Script/ActionRPG.HeroWeaponBase")));
	}

	static FActionAttackEntryConfig MakeDefaultAttackEntryConfig(const FGameplayTag& InRequestTag)
	{
		FActionAttackEntryConfig AttackEntryConfig;
		AttackEntryConfig.RequestTag = InRequestTag;
		if (InRequestTag == ActionGameplayTags::Attack_Request_Default)
		{
			AttackEntryConfig.BranchTag = ActionGameplayTags::Attack_Branch_Light;
			AttackEntryConfig.TriggerInputEvent = EActionInputEvent::Released;
			AttackEntryConfig.bUseComboIndex = true;
			AttackEntryConfig.bAdvanceComboIndexOnPlay = true;
			return AttackEntryConfig;
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Held)
		{
			AttackEntryConfig.BranchTag = ActionGameplayTags::Attack_Branch_Heavy;
			AttackEntryConfig.TriggerInputEvent = EActionInputEvent::Held;
			AttackEntryConfig.bResetComboIndexOnPlay = true;
			return AttackEntryConfig;
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter)
		{
			AttackEntryConfig.BranchTag = ActionGameplayTags::Attack_Branch_DodgeCounter;
			AttackEntryConfig.TriggerInputEvent = EActionInputEvent::Pressed;
			AttackEntryConfig.bResetComboIndexOnPlay = true;
			return AttackEntryConfig;
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Sprint)
		{
			AttackEntryConfig.BranchTag = ActionGameplayTags::Attack_Branch_Sprint;
			AttackEntryConfig.TriggerInputEvent = EActionInputEvent::Pressed;
			AttackEntryConfig.bAdvanceComboIndexOnPlay = true;
			AttackEntryConfig.bResetComboIndexOnPlay = true;
			return AttackEntryConfig;
		}

		if (InRequestTag == ActionGameplayTags::Attack_Request_Airborne)
		{
			AttackEntryConfig.BranchTag = ActionGameplayTags::Attack_Branch_Airborne;
			AttackEntryConfig.TriggerInputEvent = EActionInputEvent::Released;
			AttackEntryConfig.bResetComboIndexOnPlay = true;
		}

		return AttackEntryConfig;
	}

	static void RebuildAttackEntryConfigs(
		TArray<FActionAttackEntryConfig>& InOutAttackEntryConfigs,
		const bool bPreserveExistingOverrides,
		const EHeroWeaponCategory InWeaponCategory)
	{
		(void)InWeaponCategory;

		TMap<FGameplayTag, FActionAttackEntryConfig> ExistingAttackEntryConfigMap;
		for (const FActionAttackEntryConfig& ExistingConfig : InOutAttackEntryConfigs)
		{
			if (ExistingConfig.RequestTag.IsValid())
			{
				ExistingAttackEntryConfigMap.Add(ExistingConfig.RequestTag, ExistingConfig);
			}
		}

		const FGameplayTag OrderedRequestTags[] =
		{
			ActionGameplayTags::Attack_Request_Default,
			ActionGameplayTags::Attack_Request_Held,
			ActionGameplayTags::Attack_Request_DodgeCounter,
			ActionGameplayTags::Attack_Request_Sprint,
			ActionGameplayTags::Attack_Request_Airborne
		};

		TArray<FActionAttackEntryConfig> RebuiltConfigs;
		RebuiltConfigs.Reserve(UE_ARRAY_COUNT(OrderedRequestTags));

		for (const FGameplayTag& RequestTag : OrderedRequestTags)
		{
			FActionAttackEntryConfig Config = MakeDefaultAttackEntryConfig(RequestTag);
			if (bPreserveExistingOverrides)
			{
				if (const FActionAttackEntryConfig* ExistingConfig = ExistingAttackEntryConfigMap.Find(RequestTag))
				{
					Config = *ExistingConfig;
					Config.RequestTag = RequestTag;
				}
			}

			RebuiltConfigs.Add(Config);
		}

		InOutAttackEntryConfigs = MoveTemp(RebuiltConfigs);
	}

	static void ClearAnimationAssetReferences(FActionWeaponAnimationConfig& InOutAnimationConfig)
	{
		InOutAnimationConfig.LinkedAnimLayer.Reset();

		for (FActionAttackEntryConfig& AttackEntryConfig : InOutAnimationConfig.AttackEntryConfigs)
		{
			for (FActionAttackClipConfig& AttackClip : AttackEntryConfig.AttackClips)
			{
				AttackClip.AttackMontage.Reset();
			}
		}

		InOutAnimationConfig.EnterCombatModeMontage.Reset();
		InOutAnimationConfig.ExitCombatModeMontage.Reset();
		InOutAnimationConfig.StandingDodgeMontage.Reset();
		InOutAnimationConfig.MovingDodgeMontage.Reset();
		InOutAnimationConfig.DefenseMontage.Reset();
		InOutAnimationConfig.BlockedHitMontage.Reset();
		InOutAnimationConfig.SpecialWeaponSwitchClip = FActionWeaponSwitchClipConfig();
	}

	static void AppendValidationLine(TArray<FString>& OutLines, const FString& InLine)
	{
		OutLines.Add(InLine);
	}

	static void ValidateAttackEntryConfigs(
		const FActionWeaponAnimationConfig& InAnimationConfig,
		TArray<FString>& OutLines)
	{
		TSet<FGameplayTag> ExistingRequestTags;
		TSet<FGameplayTag> ExistingBranchTags;

		for (const FActionAttackEntryConfig& EntryConfig : InAnimationConfig.AttackEntryConfigs)
		{
			if (!EntryConfig.RequestTag.IsValid())
			{
				AppendValidationLine(OutLines, TEXT("[错误] AttackEntryConfigs 中存在 RequestTag 无效的攻击条目。"));
				continue;
			}

			if (ExistingRequestTags.Contains(EntryConfig.RequestTag))
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[错误] AttackEntryConfigs 中存在重复请求标签：%s"),
						*EntryConfig.RequestTag.ToString()));
			}
			ExistingRequestTags.Add(EntryConfig.RequestTag);

			if (!EntryConfig.BranchTag.IsValid())
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[错误] 攻击条目 %s 未配置有效的 BranchTag。"),
						*GetRequestSemanticName(EntryConfig.RequestTag)));
				continue;
			}

			if (ExistingBranchTags.Contains(EntryConfig.BranchTag))
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[提示] AttackEntryConfigs 中存在重复攻击分支语义：%s。当前批次仍按一对一请求映射为正式基线。"),
						*EntryConfig.BranchTag.ToString()));
			}
			ExistingBranchTags.Add(EntryConfig.BranchTag);

			if (EntryConfig.AttackClips.Num() <= 0)
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[提示] 攻击条目 %s（%s）还没有配置任何攻击段落。"),
						*GetRequestSemanticName(EntryConfig.RequestTag),
						*EntryConfig.RequestTag.ToString()));
			}

			int32 EmptyMontageCount = 0;
			for (const FActionAttackClipConfig& AttackClip : EntryConfig.AttackClips)
			{
				if (AttackClip.AttackMontage.IsNull())
				{
					++EmptyMontageCount;
				}
			}

			if (EmptyMontageCount > 0)
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[提示] 攻击条目 %s 中存在 %d 个空攻击段落蒙太奇。"),
						*GetRequestSemanticName(EntryConfig.RequestTag),
						EmptyMontageCount));
			}
		}
	}


	static void ValidateHitWindowTemplateConfigs(
		const FActionWeaponAnimationConfig& InAnimationConfig,
		TArray<FString>& OutLines)
	{
		TSet<FName> ExistingTemplateNames;

		for (int32 TemplateIndex = 0; TemplateIndex < InAnimationConfig.HitWindowTemplateConfigs.Num(); ++TemplateIndex)
		{
			const FActionHitWindowTemplateConfig& TemplateConfig =
				InAnimationConfig.HitWindowTemplateConfigs[TemplateIndex];

			FString FailureReason;
			if (!TemplateConfig.IsValidTemplate(FailureReason))
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[错误] 命中窗口模板[%d] 配置非法：%s"),
						TemplateIndex,
						*FailureReason));
				continue;
			}

			if (ExistingTemplateNames.Contains(TemplateConfig.TemplateName))
			{
				AppendValidationLine(
					OutLines,
					FString::Printf(
						TEXT("[错误] HitWindowTemplateConfigs 中存在重复模板名：%s"),
						*TemplateConfig.TemplateName.ToString()));
				continue;
			}

			ExistingTemplateNames.Add(TemplateConfig.TemplateName);
		}
	}

	static void ValidateCommonAnimationResources(
		const FActionWeaponAnimationConfig& InAnimationConfig,
		const EHeroWeaponCategory InWeaponCategory,
		TArray<FString>& OutLines)
	{
		auto HasCombatWeaponPresentationSwitchNotify = [](const UAnimMontage* InMontage) -> bool
		{
			if (!InMontage)
			{
				return false;
			}

			for (const FAnimNotifyEvent& NotifyEvent : InMontage->Notifies)
			{
				if (NotifyEvent.Notify != nullptr
					&& NotifyEvent.Notify->IsA(UAnimNotify_CombatWeaponPresentationSwitch::StaticClass()))
				{
					return true;
				}
			}

			return false;
		};

		if (InAnimationConfig.LinkedAnimLayer.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] LinkedAnimLayer 尚未配置。"));
		}

		if (InAnimationConfig.EnterCombatModeMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] EnterCombatModeMontage 尚未配置。"));
		}
		else if (!HasCombatWeaponPresentationSwitchNotify(InAnimationConfig.EnterCombatModeMontage.Get()))
		{
			AppendValidationLine(OutLines, TEXT("[提示] EnterCombatModeMontage 缺少 CombatWeaponPresentationSwitch Notify。"));
		}

		if (InAnimationConfig.ExitCombatModeMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] ExitCombatModeMontage 尚未配置。"));
		}
		else if (!HasCombatWeaponPresentationSwitchNotify(InAnimationConfig.ExitCombatModeMontage.Get()))
		{
			AppendValidationLine(OutLines, TEXT("[提示] ExitCombatModeMontage 缺少 CombatWeaponPresentationSwitch Notify。"));
		}

		if (InAnimationConfig.StandingDodgeMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] StandingDodgeMontage 尚未配置。"));
		}

		if (InAnimationConfig.MovingDodgeMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] MovingDodgeMontage 尚未配置。"));
		}

		if (InAnimationConfig.DefenseMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] DefenseMontage 尚未配置。"));
		}

		if (InAnimationConfig.BlockedHitMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] BlockedHitMontage 尚未配置。普通格挡成功后将缺少独立格挡受击表现。"));
		}

		if (!InAnimationConfig.SpecialWeaponSwitchClip.IsValidConfig()
			&& InWeaponCategory != EHeroWeaponCategory::Unarmed)
		{
			AppendValidationLine(OutLines, TEXT("[提示] SpecialWeaponSwitchClip 尚未配置有效蒙太奇。"));
		}
	}

	static void ValidateExecutionConfig(
		const FActionExecutionConfig& InExecutionConfig,
		TArray<FString>& OutLines)
	{
		if (InExecutionConfig.ExecutionMontage.IsNull())
		{
			AppendValidationLine(OutLines, TEXT("[提示] 处决配置中的 ExecutionMontage 尚未配置。"));
		}

		if (InExecutionConfig.VictimTurnDurationSeconds < 0.f)
		{
			AppendValidationLine(OutLines, TEXT("[错误] 处决配置中的 VictimTurnDurationSeconds 不应小于 0。"));
		}

		if (InExecutionConfig.ExecutionStartDistance < 0.f)
		{
			AppendValidationLine(OutLines, TEXT("[错误] 处决配置中的 ExecutionStartDistance 不应小于 0。"));
		}
	}

	static bool TryBuildHitEffectEntryValidationFailure(
		const FActionHitEffectEntry& InEffectEntry,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		if (!InEffectEntry.EffectDefinition)
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 未配置 EffectDefinition。"),
				*InContextName);
			return true;
		}

		if (!InEffectEntry.EffectDefinition->IsValidDefinition())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 引用的效果资产 %s 未通过合法性校验。"),
				*InContextName,
				*GetNameSafe(InEffectEntry.EffectDefinition));
			return true;
		}

		if (InEffectEntry.EffectDefinition->GetEffectKind() != InEffectEntry.EffectKind)
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 的 EffectKind 与效果资产小类不匹配。EntryKind=%d AssetKind=%d"),
				*InContextName,
				static_cast<int32>(InEffectEntry.EffectKind),
				static_cast<int32>(InEffectEntry.EffectDefinition->GetEffectKind()));
			return true;
		}

		switch (InEffectEntry.EffectKind)
		{
		case EActionHitEffectKind::Dot:
			if (!Cast<UDataAsset_ActionHitDotEffectDefinition>(InEffectEntry.EffectDefinition))
			{
				OutFailureReason = FString::Printf(
					TEXT("%s 选择了 Dot 小类，但引用的不是 UDataAsset_ActionHitDotEffectDefinition。"),
					*InContextName);
				return true;
			}
			break;

		case EActionHitEffectKind::Buff:
			if (!Cast<UDataAsset_ActionHitBuffEffectDefinition>(InEffectEntry.EffectDefinition))
			{
				OutFailureReason = FString::Printf(
					TEXT("%s 选择了 Buff 小类，但引用的不是 UDataAsset_ActionHitBuffEffectDefinition。"),
					*InContextName);
				return true;
			}
			break;

		case EActionHitEffectKind::Debuff:
			if (!Cast<UDataAsset_ActionHitDebuffEffectDefinition>(InEffectEntry.EffectDefinition))
			{
				OutFailureReason = FString::Printf(
					TEXT("%s 选择了 Debuff 小类，但引用的不是 UDataAsset_ActionHitDebuffEffectDefinition。"),
					*InContextName);
				return true;
			}
			break;

		default:
			break;
		}

		return false;
	}

	static bool TryBuildHitConfigEffectValidationFailure(
		const FActionWeaponHitConfig& InHitConfig,
		const bool bAllowsAdditionalHitEffects,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		if (!bAllowsAdditionalHitEffects && InHitConfig.AdditionalEffects.Num() > 0)
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 配置了 AdditionalEffects，但当前武器类型不允许额外命中效果层。"),
				*InContextName);
			return true;
		}

		auto TryBuildInvalidEntryFailure =
			[&OutFailureReason, &InContextName](
				const TArray<FActionHitEffectEntry>& InEffectEntries,
				const TCHAR* InLayerName) -> bool
			{
				for (int32 Index = 0; Index < InEffectEntries.Num(); ++Index)
				{
					FString EntryFailureReason;
					if (TryBuildHitEffectEntryValidationFailure(
						InEffectEntries[Index],
						FString::Printf(TEXT("%s.%s[%d]"), *InContextName, InLayerName, Index),
						EntryFailureReason))
					{
						OutFailureReason = EntryFailureReason;
						return true;
					}
				}

				return false;
			};

		return TryBuildInvalidEntryFailure(InHitConfig.DefaultEffects, TEXT("DefaultEffects"))
			|| TryBuildInvalidEntryFailure(InHitConfig.AdditionalEffects, TEXT("AdditionalEffects"));
	}

	static bool TryBuildExecutionHitConfigValidationFailure(
		const FActionExecutionHitConfig& InHitConfig,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		for (int32 Index = 0; Index < InHitConfig.DefaultEffects.Num(); ++Index)
		{
			FString EntryFailureReason;
			if (TryBuildHitEffectEntryValidationFailure(
				InHitConfig.DefaultEffects[Index],
				FString::Printf(TEXT("%s.DefaultEffects[%d]"), *InContextName, Index),
				EntryFailureReason))
			{
				OutFailureReason = EntryFailureReason;
				return true;
			}
		}

		return false;
	}

	static void ValidateHitConfigEffects(
		const FActionWeaponHitConfig& InHitConfig,
		const bool bAllowsAdditionalHitEffects,
		const FString& InContextName,
		TArray<FString>& OutLines)
	{
		FString ValidationFailure;
		if (TryBuildHitConfigEffectValidationFailure(
			InHitConfig,
			bAllowsAdditionalHitEffects,
			InContextName,
			ValidationFailure))
		{
			AppendValidationLine(OutLines, FString::Printf(TEXT("[错误] %s"), *ValidationFailure));
		}
	}

	static void ValidateExecutionHitConfig(
		const FActionExecutionHitConfig& InHitConfig,
		const FString& InContextName,
		TArray<FString>& OutLines)
	{
		FString ValidationFailure;
		if (TryBuildExecutionHitConfigValidationFailure(InHitConfig, InContextName, ValidationFailure))
		{
			AppendValidationLine(OutLines, FString::Printf(TEXT("[错误] %s"), *ValidationFailure));
		}
	}

	static bool TryBuildProjectileHitEffectValidationFailure(
		const FActionProjectileConfig& InProjectileConfig,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		auto TryBuildInvalidEntryFailure =
			[&OutFailureReason, &InContextName](
				const TArray<FActionHitEffectEntry>& InEffectEntries,
				const TCHAR* InLayerName) -> bool
			{
				for (int32 Index = 0; Index < InEffectEntries.Num(); ++Index)
				{
					FString EntryFailureReason;
					if (TryBuildHitEffectEntryValidationFailure(
						InEffectEntries[Index],
						FString::Printf(TEXT("%s.%s[%d]"), *InContextName, InLayerName, Index),
						EntryFailureReason))
					{
						OutFailureReason = EntryFailureReason;
						return true;
					}
				}

				return false;
			};

		return TryBuildInvalidEntryFailure(InProjectileConfig.DefaultEffects, TEXT("DefaultEffects"))
			|| TryBuildInvalidEntryFailure(InProjectileConfig.AdditionalEffects, TEXT("AdditionalEffects"));
	}

	static bool TryBuildProjectileConfigValidationFailure(
		const FActionProjectileConfig& InProjectileConfig,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		if (!InProjectileConfig.HasAnyConfiguredValue())
		{
			return false;
		}

		if (!InProjectileConfig.IsValidConfig())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 已填写发射物相关字段，但 ProjectileClass 为空。"),
				*InContextName);
			return true;
		}

		if (InProjectileConfig.DamageType != EActionDamageType::Elemental
			&& InProjectileConfig.DamageElementTypeTag.IsValid())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 不是元素伤害，但仍配置了 DamageElementTypeTag。"),
				*InContextName);
			return true;
		}

		if (InProjectileConfig.DamageType == EActionDamageType::Elemental
			&& !InProjectileConfig.DamageElementTypeTag.IsValid())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 是元素伤害，但没有配置 DamageElementTypeTag。"),
				*InContextName);
			return true;
		}

		return TryBuildProjectileHitEffectValidationFailure(
			InProjectileConfig,
			InContextName,
			OutFailureReason);
	}

	static bool TryBuildSwitchableProjectileEntryValidationFailure(
		const FActionSwitchableProjectileConfigEntry& InEntry,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		if (!InEntry.ProjectileConfigTag.IsValid())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s 的 ProjectileConfigTag 无效。"),
				*InContextName);
			return true;
		}

		return TryBuildProjectileConfigValidationFailure(
			InEntry.ProjectileConfig,
			InContextName,
			OutFailureReason);
	}

	static void ValidateProjectileConfig(
		const FActionProjectileConfig& InProjectileConfig,
		const FString& InContextName,
		TArray<FString>& OutLines)
	{
		FString ValidationFailure;
		if (TryBuildProjectileConfigValidationFailure(
			InProjectileConfig,
			InContextName,
			ValidationFailure))
		{
			AppendValidationLine(OutLines, FString::Printf(TEXT("[错误] %s"), *ValidationFailure));
		}
	}

	static bool TryBuildSpiritAbilityEntryValidationFailure(
		const TArray<FActionSpiritAbilityEntryConfig>& InEntryConfigs,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();

		TSet<FGameplayTag> ExistingInputTags;
		int32 SpiritSkillEntryCount = 0;
		for (int32 EntryIndex = 0; EntryIndex < InEntryConfigs.Num(); ++EntryIndex)
		{
			const FActionSpiritAbilityEntryConfig& EntryConfig = InEntryConfigs[EntryIndex];
			if (!EntryConfig.HasAnyConfiguredData())
			{
				OutFailureReason = FString::Printf(
					TEXT("SpiritAbilityEntryConfigs 第 %d 项为空条目，请删除或补完整。"),
					EntryIndex);
				return true;
			}

			FString EntryFailureReason;
			if (!EntryConfig.IsValidConfig(EntryFailureReason))
			{
				OutFailureReason = FString::Printf(
					TEXT("SpiritAbilityEntryConfigs 第 %d 项无效。EntryKind=%s，%s"),
					EntryIndex,
					*GetSpiritEntryKindName(EntryConfig.EntryKind),
					*EntryFailureReason);
				return true;
			}

			if (ExistingInputTags.Contains(EntryConfig.InputTag))
			{
				OutFailureReason = FString::Printf(
					TEXT("SpiritAbilityEntryConfigs 中存在重复输入标签。InputTag=%s"),
					*EntryConfig.InputTag.ToString());
				return true;
			}
			ExistingInputTags.Add(EntryConfig.InputTag);

			if (EntryConfig.IsSpiritSkillEntry())
			{
				++SpiritSkillEntryCount;
				if (!EntryConfig.AbilityToGrant
					|| !EntryConfig.AbilityToGrant->IsChildOf(UHeroGA_SpiritSkill::StaticClass()))
				{
					OutFailureReason = FString::Printf(
						TEXT("SpiritAbilityEntryConfigs 第 %d 项无效。EntryKind=%s，但 AbilityToGrant 不是 UHeroGA_SpiritSkill。"),
						EntryIndex,
						*GetSpiritEntryKindName(EntryConfig.EntryKind));
					return true;
				}
			}
			else if (EntryConfig.AbilityToGrant
				&& EntryConfig.AbilityToGrant->IsChildOf(UHeroGA_SpiritSkill::StaticClass()))
			{
				OutFailureReason = FString::Printf(
					TEXT("SpiritAbilityEntryConfigs 第 %d 项无效。EntryKind=%s，却使用了 UHeroGA_SpiritSkill。"),
					EntryIndex,
					*GetSpiritEntryKindName(EntryConfig.EntryKind));
				return true;
			}
		}

		if (SpiritSkillEntryCount > 4)
		{
			OutFailureReason = FString::Printf(
				TEXT("SpiritAbilityEntryConfigs 中最多只允许 4 条 Spirit 主动技能条目。当前=%d"),
				SpiritSkillEntryCount);
			return true;
		}

		return false;
	}

	static bool TryBuildSpiritAbilityEntryDetailValidationFailure(
		const FActionSpiritAbilityEntryConfig& InEntryConfig,
		const bool bInAllowsAdditionalHitEffects,
		const FActionProjectileConfig& InDefaultProjectileConfig,
		const FString& InContextName,
		FString& OutFailureReason)
	{
		OutFailureReason.Reset();

		if (InEntryConfig.IsOffensiveSpiritSkill())
		{
			for (int32 ClipIndex = 0; ClipIndex < InEntryConfig.SpiritSkillConfig.SkillClips.Num(); ++ClipIndex)
			{
				const FActionSpiritSkillClipConfig& SkillClip = InEntryConfig.SpiritSkillConfig.SkillClips[ClipIndex];
				const FString ClipContextName = FString::Printf(TEXT("%s.SkillClips[%d]"), *InContextName, ClipIndex);
				if (TryBuildHitConfigEffectValidationFailure(
					SkillClip.HitConfig,
					bInAllowsAdditionalHitEffects,
					FString::Printf(TEXT("%s.HitConfig"), *ClipContextName),
					OutFailureReason))
				{
					return true;
				}

				if (!SkillClip.bShouldSpawnProjectile)
				{
					continue;
				}

				if (SkillClip.ProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig)
				{
					if (TryBuildProjectileConfigValidationFailure(
						InDefaultProjectileConfig,
						FString::Printf(TEXT("%s 依赖的武器默认发射物配置"), *ClipContextName),
						OutFailureReason))
					{
						return true;
					}
					continue;
				}

				if (TryBuildProjectileConfigValidationFailure(
					SkillClip.ProjectileSpawnConfig.ProjectileConfigOverride,
					FString::Printf(TEXT("%s 的发射物配置"), *ClipContextName),
					OutFailureReason))
				{
					return true;
				}
			}
		}
		else if (InEntryConfig.IsSelfEnhanceSpiritSkill())
		{
			for (int32 ClipIndex = 0; ClipIndex < InEntryConfig.SpiritSkillConfig.SkillClips.Num(); ++ClipIndex)
			{
				const FActionSpiritSkillClipConfig& SkillClip = InEntryConfig.SpiritSkillConfig.SkillClips[ClipIndex];
				if (SkillClip.HasAnyOffensiveConfiguredData())
				{
					OutFailureReason = FString::Printf(
						TEXT("%s 的 SelfEnhance 表现段 SkillClips[%d] 不允许配置攻击字段：HitConfig / Projectile。"),
						*InContextName,
						ClipIndex);
					return true;
				}
			}
		}

		return false;
	}
}

UDataAsset_WeaponDefinition::UDataAsset_WeaponDefinition()
{
	// 新建武器资产时直接生成一套完整的攻击链模板，
	// 这样后续只需要往既定分支里填动画，而不需要手动搭结构。
	ResetAllAttackConfigsToDefault();
}

void UDataAsset_WeaponDefinition::PostLoad()
{
	Super::PostLoad();
	ActionWeaponDefinitionDefaults::RebuildAttackEntryConfigs(
		AnimationConfig.AttackEntryConfigs,
		true,
		WeaponCategory);
	SyncSpiritAbilityEditorExposureFlags();
}

#if WITH_EDITOR
void UDataAsset_WeaponDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SyncSpiritAbilityEditorExposureFlags();
}

bool UDataAsset_WeaponDefinition::CanEditChange(const FProperty* InProperty) const
{
	const bool bParentAllowsEdit = Super::CanEditChange(InProperty);
	if (!bParentAllowsEdit || InProperty == nullptr)
	{
		return bParentAllowsEdit;
	}

	const FName PropertyName = InProperty->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, WeaponActorClass))
	{
		return CanEditWeaponActorClass();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, DamageElementTypeTag))
	{
		return CanEditDamageElementTypeTag();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, SpiritAbilityEntryConfigs))
	{
		return CanEditSpiritAbilityFields();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, DefaultProjectileConfig))
	{
		return CanEditDefaultProjectileConfig();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, bSupportsProjectileSwitching))
	{
		return CanEditProjectileSwitching();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataAsset_WeaponDefinition, SwitchableProjectileConfigs))
	{
		return CanEditSwitchableProjectileConfigs();
	}

	return true;
}

bool UDataAsset_WeaponDefinition::CanEditWeaponActorClass() const
{
	return WeaponCategory != EHeroWeaponCategory::Unarmed;
}

bool UDataAsset_WeaponDefinition::CanEditDamageElementTypeTag() const
{
	return WeaponPropertyType == EActionWeaponPropertyType::Elemental;
}

bool UDataAsset_WeaponDefinition::CanEditSpiritAbilityFields() const
{
	return WeaponPropertyType == EActionWeaponPropertyType::Spirit;
}

bool UDataAsset_WeaponDefinition::CanEditDefaultProjectileConfig() const
{
	return WeaponCategory == EHeroWeaponCategory::PureRanged
		|| WeaponCategory == EHeroWeaponCategory::MeleeRangedHybrid;
}

bool UDataAsset_WeaponDefinition::CanEditProjectileSwitching() const
{
	return WeaponCategory == EHeroWeaponCategory::PureRanged
		&& WeaponSubtypeTag == ActionGameplayTags::Player_Weapon_Ranged_Staff;
}

bool UDataAsset_WeaponDefinition::CanEditSwitchableProjectileConfigs() const
{
	return CanEditProjectileSwitching() && bSupportsProjectileSwitching;
}
#endif

void UDataAsset_WeaponDefinition::SyncSpiritAbilityEditorExposureFlags()
{
	for (FActionSpiritAbilityEntryConfig& EntryConfig : SpiritAbilityEntryConfigs)
	{
		EntryConfig.SyncEditorExposureFlags();
	}
}

bool UDataAsset_WeaponDefinition::HasAnySpiritAbilityEntryConfigs() const
{
	return SpiritAbilityEntryConfigs.ContainsByPredicate([](const FActionSpiritAbilityEntryConfig& EntryConfig)
	{
		return EntryConfig.HasAnyConfiguredData();
	});
}

bool UDataAsset_WeaponDefinition::HasAnySpiritSkillAbilityEntryConfigs() const
{
	return SpiritAbilityEntryConfigs.ContainsByPredicate([](const FActionSpiritAbilityEntryConfig& EntryConfig)
	{
		return EntryConfig.IsSpiritSkillEntry() && EntryConfig.HasAnyConfiguredData();
	});
}

bool UDataAsset_WeaponDefinition::TryResolveSpiritAbilityEntryConfigByInputTag(
	const FGameplayTag& InInputTag,
	FActionSpiritAbilityEntryConfig& OutEntryConfig) const
{
	OutEntryConfig = FActionSpiritAbilityEntryConfig();
	if (!InInputTag.IsValid())
	{
		return false;
	}

	// Spirit 条目查询只在当前武器资产内部做静态匹配。
	// 这里返回的是一份配置快照，不等于 Ability 已授予，也不表达技能当前是否处于运行中。
	for (const FActionSpiritAbilityEntryConfig& EntryConfig : SpiritAbilityEntryConfigs)
	{
		if (EntryConfig.InputTag == InInputTag)
		{
			OutEntryConfig = EntryConfig;
			return true;
		}
	}

	return false;
}

bool UDataAsset_WeaponDefinition::IsValidDefinition() const
{
	// 对外公开的有效性查询统一复用详细失败描述。
	// 这样编辑器和运行时防御性检查不会各自维护一套分叉语义。
	return DescribeValidationFailure().IsEmpty();
}

FString UDataAsset_WeaponDefinition::DescribeValidationFailure() const
{
	// 这条入口的职责是给资产作者返回“哪一项静态配置不成立”的单条收口原因。
	// 它不是战斗运行时错误状态机，也不承担资产自动修复。
	if (!WeaponTag.IsValid())
	{
		return TEXT("WeaponTag 无效。");
	}

	if (!WeaponSubtypeTag.IsValid())
	{
		return TEXT("WeaponSubtypeTag 无效。");
	}

	if (!HasMatchingCategoryRootTag())
	{
		return FString::Printf(
			TEXT("WeaponTag 与 WeaponCategory 根标签不匹配。WeaponCategory=%s WeaponTag=%s ExpectedRoot=%s"),
			*ActionWeaponDefinitionDefaults::GetWeaponCategoryName(WeaponCategory),
			*WeaponTag.ToString(),
			*ActionWeaponDefinitionDefaults::GetExpectedWeaponRootTagName(WeaponCategory));
	}

	if (!HasMatchingWeaponSubtypeTag())
	{
		return FString::Printf(
			TEXT("WeaponSubtypeTag 与 WeaponCategory 不匹配或不在允许集合内。WeaponCategory=%s WeaponSubtypeTag=%s"),
			*ActionWeaponDefinitionDefaults::GetWeaponCategoryName(WeaponCategory),
			*WeaponSubtypeTag.ToString());
	}

	if (!HasMatchingWeaponTagHierarchy())
	{
		return FString::Printf(
			TEXT("WeaponTag 与 WeaponSubtypeTag 层级不匹配。WeaponTag=%s WeaponSubtypeTag=%s"),
			*WeaponTag.ToString(),
			*WeaponSubtypeTag.ToString());
	}

	const EActionDamageType ExpectedDamageType =
		ActionWeaponDefinitionDefaults::ResolveExpectedDamageTypeByWeaponPropertyType(WeaponPropertyType);
	if (DamageType != ExpectedDamageType)
	{
		return FString::Printf(
			TEXT("WeaponPropertyType 与 DamageType 不匹配。WeaponPropertyType=%s DamageType=%s ExpectedDamageType=%s"),
			*ActionWeaponDefinitionDefaults::GetWeaponPropertyTypeName(WeaponPropertyType),
			*ActionWeaponDefinitionDefaults::GetDamageTypeName(DamageType),
			*ActionWeaponDefinitionDefaults::GetDamageTypeName(ExpectedDamageType));
	}

	if (WeaponPropertyType == EActionWeaponPropertyType::Elemental && !DamageElementTypeTag.IsValid())
	{
		return TEXT("元素武器必须配置 DamageElementTypeTag。");
	}

	if (WeaponPropertyType != EActionWeaponPropertyType::Elemental && DamageElementTypeTag.IsValid())
	{
		return TEXT("非元素武器不应配置 DamageElementTypeTag。");
	}

	if (WeaponPropertyType != EActionWeaponPropertyType::Spirit
		&& SpiritAbilityEntryConfigs.Num() > 0)
	{
		return TEXT("只有灵武器才允许配置 Spirit Ability 条目。");
	}

	if (WeaponPropertyType == EActionWeaponPropertyType::Spirit)
	{
		// Spirit 条目校验分两层：
		// 先校验条目集合的整体结构，再校验每条条目的细节与命中/发射物桥接配置。
		FString SpiritSkillValidationFailure;
		if (ActionWeaponDefinitionDefaults::TryBuildSpiritAbilityEntryValidationFailure(
				SpiritAbilityEntryConfigs,
				SpiritSkillValidationFailure))
		{
			return SpiritSkillValidationFailure;
		}

		for (int32 EntryIndex = 0; EntryIndex < SpiritAbilityEntryConfigs.Num(); ++EntryIndex)
		{
			if (ActionWeaponDefinitionDefaults::TryBuildSpiritAbilityEntryDetailValidationFailure(
				SpiritAbilityEntryConfigs[EntryIndex],
				AllowsAdditionalHitEffects(),
				DefaultProjectileConfig,
				FString::Printf(TEXT("SpiritAbilityEntryConfigs[%d]"), EntryIndex),
				SpiritSkillValidationFailure))
			{
				return SpiritSkillValidationFailure;
			}
		}
	}

	if (WeaponCategory != EHeroWeaponCategory::PureRanged && bSupportsProjectileSwitching)
	{
		return TEXT("只有 Player.Weapon.Ranged.Staff 小类别的纯远程武器才允许开启 bSupportsProjectileSwitching。");
	}

	if (WeaponCategory != EHeroWeaponCategory::PureRanged && SwitchableProjectileConfigs.Num() > 0)
	{
		return TEXT("只有 Player.Weapon.Ranged.Staff 小类别的纯远程武器才允许配置 SwitchableProjectileConfigs。");
	}

	if (bSupportsProjectileSwitching)
	{
		if (WeaponSubtypeTag != ActionGameplayTags::Player_Weapon_Ranged_Staff)
		{
			return FString::Printf(
				TEXT("只有法杖类远程武器才允许开启发射物切换。当前 WeaponSubtypeTag=%s"),
				*WeaponSubtypeTag.ToString());
		}

		if (SwitchableProjectileConfigs.Num() <= 0)
		{
			return TEXT("已开启 bSupportsProjectileSwitching，但没有配置 SwitchableProjectileConfigs。");
		}
	}
	else if (SwitchableProjectileConfigs.Num() > 0)
	{
		return TEXT("未开启 bSupportsProjectileSwitching 时，不应配置 SwitchableProjectileConfigs。");
	}

	{
		FString HitEffectValidationFailure;
		for (const FActionAttackEntryConfig& AttackEntryConfig : AnimationConfig.AttackEntryConfigs)
		{
			for (int32 ClipIndex = 0; ClipIndex < AttackEntryConfig.AttackClips.Num(); ++ClipIndex)
			{
				const FActionAttackClipConfig& AttackClip = AttackEntryConfig.AttackClips[ClipIndex];
				if (ActionWeaponDefinitionDefaults::TryBuildHitConfigEffectValidationFailure(
					AttackClip.HitConfig,
					AllowsAdditionalHitEffects(),
					FString::Printf(TEXT("攻击条目 %s 的攻击段落[%d] HitConfig"), *AttackEntryConfig.RequestTag.ToString(), ClipIndex),
					HitEffectValidationFailure))
				{
					return HitEffectValidationFailure;
				}
			}
		}

		if (ActionWeaponDefinitionDefaults::TryBuildExecutionHitConfigValidationFailure(
			ExecutionConfig.HitConfig,
			TEXT("处决专用 HitConfig"),
			HitEffectValidationFailure))
		{
			return HitEffectValidationFailure;
		}
	}

	{
		FString ProjectileValidationFailure;
		// 发射物校验同样只验证静态桥接关系：
		// 默认发射物配置、攻击段条目覆写、以及可切换配置标签能否组成完整配置入口。
		if (ActionWeaponDefinitionDefaults::TryBuildProjectileConfigValidationFailure(
			DefaultProjectileConfig,
			TEXT("武器默认发射物配置"),
			ProjectileValidationFailure))
		{
			return ProjectileValidationFailure;
		}

		for (const FActionAttackEntryConfig& AttackEntryConfig : AnimationConfig.AttackEntryConfigs)
		{
			for (int32 ClipIndex = 0; ClipIndex < AttackEntryConfig.AttackClips.Num(); ++ClipIndex)
			{
				const FActionAttackClipConfig& AttackClip = AttackEntryConfig.AttackClips[ClipIndex];
				if (!AttackClip.bShouldSpawnProjectile)
				{
					continue;
				}

				if (AttackClip.ProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig)
				{
					if (!DefaultProjectileConfig.IsValidConfig())
					{
						return FString::Printf(
							TEXT("攻击条目 %s 的攻击段落[%d] 已启用发射物生成，但武器默认发射物配置不可用。"),
							*AttackEntryConfig.RequestTag.ToString(),
							ClipIndex);
					}

					continue;
				}

				if (ActionWeaponDefinitionDefaults::TryBuildProjectileConfigValidationFailure(
					AttackClip.ProjectileSpawnConfig.ProjectileConfigOverride,
					FString::Printf(TEXT("攻击条目 %s 的攻击段落[%d] 发射物配置"), *AttackEntryConfig.RequestTag.ToString(), ClipIndex),
					ProjectileValidationFailure))
				{
					return ProjectileValidationFailure;
				}
			}
		}

		{
			TSet<FGameplayTag> ExistingProjectileConfigTags;
			for (int32 EntryIndex = 0; EntryIndex < SwitchableProjectileConfigs.Num(); ++EntryIndex)
			{
				const FActionSwitchableProjectileConfigEntry& Entry = SwitchableProjectileConfigs[EntryIndex];
				if (ActionWeaponDefinitionDefaults::TryBuildSwitchableProjectileEntryValidationFailure(
					Entry,
					FString::Printf(TEXT("可切换发射物配置[%d]"), EntryIndex),
					ProjectileValidationFailure))
				{
					return ProjectileValidationFailure;
				}

				if (ExistingProjectileConfigTags.Contains(Entry.ProjectileConfigTag))
				{
					return FString::Printf(
						TEXT("SwitchableProjectileConfigs 中存在重复的 ProjectileConfigTag。Tag=%s"),
						*Entry.ProjectileConfigTag.ToString());
				}

				ExistingProjectileConfigTags.Add(Entry.ProjectileConfigTag);
			}
		}
	}

	{
		FString AttackConfigValidationFailure;
		if (!AnimationConfig.ValidateRequiredAttackConfig(AttackConfigValidationFailure))
		{
			return AttackConfigValidationFailure;
		}
	}

	{
		TSet<FName> ExistingHitWindowTemplateNames;
		for (const FActionHitWindowTemplateConfig& TemplateConfig : AnimationConfig.HitWindowTemplateConfigs)
		{
			FString HitWindowTemplateValidationFailure;
			if (!TemplateConfig.IsValidTemplate(HitWindowTemplateValidationFailure))
			{
				return HitWindowTemplateValidationFailure;
			}

			if (ExistingHitWindowTemplateNames.Contains(TemplateConfig.TemplateName))
			{
				return FString::Printf(
					TEXT("HitWindowTemplateConfigs 中存在重复模板名。TemplateName=%s"),
					*TemplateConfig.TemplateName.ToString());
			}

			ExistingHitWindowTemplateNames.Add(TemplateConfig.TemplateName);
		}
	}

	if (WeaponCategory == EHeroWeaponCategory::Unarmed)
	{
		return FString();
	}

	if (WeaponActorClass.IsNull())
	{
		return FString::Printf(
			TEXT("非空手武器未配置 WeaponActorClass。WeaponCategory=%s WeaponTag=%s"),
			*ActionWeaponDefinitionDefaults::GetWeaponCategoryName(WeaponCategory),
			*WeaponTag.ToString());
	}

	return FString();
}

bool UDataAsset_WeaponDefinition::MatchesWeaponCategory(const EHeroWeaponCategory InWeaponCategory) const
{
	return WeaponCategory == InWeaponCategory;
}

bool UDataAsset_WeaponDefinition::HasMatchingCategoryRootTag() const
{
	return WeaponTag.MatchesTag(ActionWeaponDefinitionDefaults::ResolveExpectedWeaponRootTag(WeaponCategory));
}

bool UDataAsset_WeaponDefinition::HasMatchingWeaponSubtypeTag() const
{
	return ActionWeaponDefinitionDefaults::IsAllowedWeaponSubtypeTag(WeaponCategory, WeaponSubtypeTag);
}

bool UDataAsset_WeaponDefinition::HasMatchingWeaponTagHierarchy() const
{
	if (!WeaponTag.IsValid() || !WeaponSubtypeTag.IsValid())
	{
		return false;
	}

	const FGameplayTag ResolvedSubtypeTag = WeaponTag.RequestDirectParent();
	return ResolvedSubtypeTag.IsValid() && ResolvedSubtypeTag == WeaponSubtypeTag;
}

bool UDataAsset_WeaponDefinition::ResolveProjectileConfigForSpawn(
	const FActionProjectileSpawnConfig& InSpawnConfig,
	const FGameplayTag& InSelectedProjectileConfigTag,
	FActionProjectileConfig& OutProjectileConfig,
	EActionResolvedProjectileConfigSource* OutResolvedConfigSource) const
{
	OutProjectileConfig = FActionProjectileConfig();
	if (OutResolvedConfigSource)
	{
		*OutResolvedConfigSource = EActionResolvedProjectileConfigSource::None;
	}

	if (InSpawnConfig.bUseWeaponDefaultProjectileConfig)
	{
		// 先看当前请求是否带了可切换配置标签。
		// 只有当默认配置路径被启用时，选中的可切换发射物才有机会覆盖武器默认配置。
		if (InSelectedProjectileConfigTag.IsValid()
			&& ResolveSwitchableProjectileConfigByTag(InSelectedProjectileConfigTag, OutProjectileConfig))
		{
			if (OutResolvedConfigSource)
			{
				*OutResolvedConfigSource = EActionResolvedProjectileConfigSource::SwitchableProjectileConfig;
			}
			return true;
		}

		if (!DefaultProjectileConfig.IsValidConfig())
		{
			return false;
		}

		// 没有切到可切换配置时，才回落到武器默认发射物模板。
		OutProjectileConfig = DefaultProjectileConfig;
		if (OutResolvedConfigSource)
		{
			*OutResolvedConfigSource = EActionResolvedProjectileConfigSource::DefaultProjectileConfig;
		}
		return true;
	}

	if (!InSpawnConfig.ProjectileConfigOverride.IsValidConfig())
	{
		return false;
	}

	// 条目级覆写路径优先级最高。
	// 这里只是把最终静态模板解析出来，真正生成发射物 Actor 仍由外层攻击链执行。
	OutProjectileConfig = InSpawnConfig.ProjectileConfigOverride;
	if (OutResolvedConfigSource)
	{
		*OutResolvedConfigSource = EActionResolvedProjectileConfigSource::BranchProjectileOverride;
	}
	return true;
}

bool UDataAsset_WeaponDefinition::ResolveSwitchableProjectileConfigByTag(
	const FGameplayTag& InProjectileConfigTag,
	FActionProjectileConfig& OutProjectileConfig) const
{
	OutProjectileConfig = FActionProjectileConfig();

	// 可切换发射物查询的前提是：武器显式支持这套静态入口，且调用侧给了有效的配置标签。
	if (!bSupportsProjectileSwitching || !InProjectileConfigTag.IsValid())
	{
		return false;
	}

	for (const FActionSwitchableProjectileConfigEntry& Entry : SwitchableProjectileConfigs)
	{
		if (Entry.ProjectileConfigTag == InProjectileConfigTag && Entry.ProjectileConfig.IsValidConfig())
		{
			OutProjectileConfig = Entry.ProjectileConfig;
			return true;
		}
	}

	return false;
}

TSubclassOf<UActionHeroLinkedAnimLayer> UDataAsset_WeaponDefinition::GetLinkedAnimLayerClass() const
{
	return AnimationConfig.LinkedAnimLayer.Get();
}

bool UDataAsset_WeaponDefinition::TryResolveHitWindowConfigByName(
	const FName InTemplateName,
	const FName InRuntimeWindowName,
	FActionHitWindowRuntimeConfig& OutRuntimeConfig) const
{
	return AnimationConfig.TryResolveHitWindowConfigByName(
		InTemplateName,
		InRuntimeWindowName,
		OutRuntimeConfig);
}

FGameplayTag UDataAsset_WeaponDefinition::ResolveAttackBranchTag(const FGameplayTag& InRequestTag) const
{
	return AnimationConfig.ResolveAttackBranchTag(InRequestTag);
}

EActionInputEvent UDataAsset_WeaponDefinition::ResolveAttackTriggerInputEvent(const FGameplayTag& InRequestTag) const
{
	return AnimationConfig.ResolveAttackTriggerInputEvent(InRequestTag);
}

bool UDataAsset_WeaponDefinition::TryResolveAttackExecutionConfigByRequestTag(
	const FGameplayTag& InRequestTag,
	const int32 InComboIndex,
	FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
{
	return AnimationConfig.TryResolveAttackExecutionConfigByRequestTag(
		InRequestTag,
		InComboIndex,
		OutResolvedConfig);
}

bool UDataAsset_WeaponDefinition::TryResolveAttackHitConfigByRequestTag(
	const FGameplayTag& InRequestTag,
	FActionWeaponHitConfig& OutHitConfig) const
{
	return AnimationConfig.TryResolveAttackHitConfigByRequestTag(InRequestTag, OutHitConfig);
}

bool UDataAsset_WeaponDefinition::TryResolveAttackExecutionConfig(
	const FGameplayTag& InBranchTag,
	const int32 InComboIndex,
	FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
{
	return AnimationConfig.TryResolveAttackExecutionConfig(InBranchTag, InComboIndex, OutResolvedConfig);
}

int32 UDataAsset_WeaponDefinition::GetAttackMontageCountForBranch(const FGameplayTag& InBranchTag) const
{
	return AnimationConfig.GetAttackMontageCountForBranch(InBranchTag);
}

UAnimMontage* UDataAsset_WeaponDefinition::GetCombatModeTransitionAnimMontage(const EHeroCombatMode InCombatMode) const
{
	return AnimationConfig.GetCombatModeTransitionMontage(InCombatMode);
}

int32 UDataAsset_WeaponDefinition::GetCombatModeTransitionReactGuardThreshold(const EHeroCombatMode InCombatMode) const
{
	return AnimationConfig.GetCombatModeTransitionReactGuardThreshold(InCombatMode);
}

UAnimMontage* UDataAsset_WeaponDefinition::GetDodgeAnimMontage(const bool bHasMoveInput) const
{
	return AnimationConfig.GetDodgeMontage(bHasMoveInput);
}

int32 UDataAsset_WeaponDefinition::GetDodgeReactGuardThreshold(const bool bHasMoveInput) const
{
	return AnimationConfig.GetDodgeReactGuardThreshold(bHasMoveInput);
}

UAnimMontage* UDataAsset_WeaponDefinition::GetDefenseAnimMontage() const
{
	return AnimationConfig.GetDefenseMontage();
}

int32 UDataAsset_WeaponDefinition::GetDefenseReactGuardThreshold() const
{
	return AnimationConfig.GetDefenseReactGuardThreshold();
}

UAnimMontage* UDataAsset_WeaponDefinition::GetBlockedHitAnimMontage() const
{
	return AnimationConfig.GetBlockedHitMontage();
}

UAnimMontage* UDataAsset_WeaponDefinition::GetExecutionMontage() const
{
	return ExecutionConfig.GetExecutionMontage();
}

UAnimMontage* UDataAsset_WeaponDefinition::GetSpecialWeaponSwitchMontage() const
{
	return AnimationConfig.GetSpecialWeaponSwitchMontage();
}

int32 UDataAsset_WeaponDefinition::GetSpecialWeaponSwitchReactGuardThreshold() const
{
	return AnimationConfig.GetSpecialWeaponSwitchReactGuardThreshold();
}

const FActionWeaponHitConfig& UDataAsset_WeaponDefinition::GetSpecialWeaponSwitchHitConfig() const
{
	return AnimationConfig.GetSpecialWeaponSwitchHitConfig();
}

FString UDataAsset_WeaponDefinition::ResolveDebugName() const
{
	if (WeaponTag.IsValid())
	{
		return WeaponTag.ToString();
	}

	const FString AssetName = GetName();
	return AssetName.IsEmpty() ? TEXT("未命名武器") : AssetName;
}

void UDataAsset_WeaponDefinition::NormalizeAttackConfigArrays()
{
	// 规范化入口偏“保守收口”：
	// 只把攻击条目数组整理回当前正式五请求结构，不主动重置成推荐预设。
	ActionWeaponDefinitionDefaults::RebuildAttackEntryConfigs(
		AnimationConfig.AttackEntryConfigs,
		true,
		WeaponCategory);
}

void UDataAsset_WeaponDefinition::ResetAttackEntryConfigsToDefault()
{
	ActionWeaponDefinitionDefaults::RebuildAttackEntryConfigs(
		AnimationConfig.AttackEntryConfigs,
		false,
		WeaponCategory);
}

void UDataAsset_WeaponDefinition::ResetAllAttackConfigsToDefault()
{
	// 新建资产或需要整体重搭攻击链时，统一重建正式 AttackEntryConfigs。
	ResetAttackEntryConfigsToDefault();
}

void UDataAsset_WeaponDefinition::ApplyRecommendedUnarmedPreset()
{
	ApplyRecommendedPreset(
		EHeroWeaponCategory::Unarmed,
		ActionGameplayTags::Player_Weapon_Unarmed,
		ActionGameplayTags::Player_Weapon_Unarmed_Default);
}

void UDataAsset_WeaponDefinition::ApplyRecommendedMeleePreset()
{
	ApplyRecommendedPreset(
		EHeroWeaponCategory::PureMelee,
		ActionGameplayTags::Player_Weapon_Melee_Sword,
		ActionGameplayTags::Player_Weapon_Melee_Sword_Default);
}

void UDataAsset_WeaponDefinition::ApplyRecommendedRangedPreset()
{
	ApplyRecommendedPreset(
		EHeroWeaponCategory::PureRanged,
		ActionGameplayTags::Player_Weapon_Ranged_Staff,
		ActionGameplayTags::Player_Weapon_Ranged_Staff_Default);
}

void UDataAsset_WeaponDefinition::ApplyRecommendedHybridPreset()
{
	ApplyRecommendedPreset(
		EHeroWeaponCategory::MeleeRangedHybrid,
		ActionGameplayTags::Player_Weapon_Hybrid_Sword,
		ActionGameplayTags::Player_Weapon_Hybrid_Sword_Default);
}

void UDataAsset_WeaponDefinition::ApplyRecommendedPresetByCurrentCategory()
{
	const FGameplayTag RecommendedSubtypeTag =
		ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponSubtypeTag(WeaponCategory);
	ApplyRecommendedPreset(
		WeaponCategory,
		RecommendedSubtypeTag,
		ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponTag(WeaponCategory, RecommendedSubtypeTag));
}

void UDataAsset_WeaponDefinition::RepairDefinitionShellForCurrentCategory()
{
	RepairDefinitionShell(WeaponCategory, false);
}

void UDataAsset_WeaponDefinition::RepairUnarmedDefinitionShell()
{
	RepairDefinitionShell(EHeroWeaponCategory::Unarmed, true);
}

void UDataAsset_WeaponDefinition::RepairMeleeDefinitionShell()
{
	RepairDefinitionShell(EHeroWeaponCategory::PureMelee, true);
}

void UDataAsset_WeaponDefinition::RepairRangedDefinitionShell()
{
	RepairDefinitionShell(EHeroWeaponCategory::PureRanged, true);
}

void UDataAsset_WeaponDefinition::RepairHybridDefinitionShell()
{
	RepairDefinitionShell(EHeroWeaponCategory::MeleeRangedHybrid, true);
}

FString UDataAsset_WeaponDefinition::BuildEditorValidationReport() const
{
	TArray<FString> ValidationLines;

	// 编辑器报告是“尽量多报”的资产整理入口。
	// 和 DescribeValidationFailure 的单条失败描述不同，这里会把同一份资产的多个缺口一次性汇总出来。
	if (!WeaponTag.IsValid())
	{
		ValidationLines.Add(TEXT("[错误] WeaponTag 未配置。"));
	}
	if (!WeaponSubtypeTag.IsValid())
	{
		ValidationLines.Add(TEXT("[错误] WeaponSubtypeTag 未配置。"));
	}
	else if (!HasMatchingCategoryRootTag())
	{
		ValidationLines.Add(FString::Printf(
			TEXT("[错误] WeaponTag 与 WeaponCategory 根标签不匹配。WeaponTag=%s ExpectedRoot=%s"),
			*WeaponTag.ToString(),
			*ActionWeaponDefinitionDefaults::GetExpectedWeaponRootTagName(WeaponCategory)));
	}
	else if (!HasMatchingWeaponSubtypeTag())
	{
		ValidationLines.Add(FString::Printf(
			TEXT("[错误] WeaponSubtypeTag 与 WeaponCategory 不匹配。WeaponSubtypeTag=%s WeaponCategory=%s"),
			*WeaponSubtypeTag.ToString(),
			*ActionWeaponDefinitionDefaults::GetWeaponCategoryName(WeaponCategory)));
	}
	else if (!HasMatchingWeaponTagHierarchy())
	{
		ValidationLines.Add(FString::Printf(
			TEXT("[错误] WeaponTag 不符合 Category.Subtype.Variant 层级，或与 WeaponSubtypeTag 不一致。WeaponTag=%s WeaponSubtypeTag=%s"),
			*WeaponTag.ToString(),
			*WeaponSubtypeTag.ToString()));
	}

	{
		const EActionDamageType ExpectedDamageType =
			ActionWeaponDefinitionDefaults::ResolveExpectedDamageTypeByWeaponPropertyType(WeaponPropertyType);
		if (DamageType != ExpectedDamageType)
		{
			ValidationLines.Add(FString::Printf(
				TEXT("[错误] WeaponPropertyType 与 DamageType 不匹配。WeaponPropertyType=%s DamageType=%s Expected=%s"),
				*ActionWeaponDefinitionDefaults::GetWeaponPropertyTypeName(WeaponPropertyType),
				*ActionWeaponDefinitionDefaults::GetDamageTypeName(DamageType),
				*ActionWeaponDefinitionDefaults::GetDamageTypeName(ExpectedDamageType)));
		}

		if (WeaponPropertyType == EActionWeaponPropertyType::Elemental)
		{
			if (!DamageElementTypeTag.IsValid())
			{
				ValidationLines.Add(TEXT("[错误] 元素武器必须配置 DamageElementTypeTag。"));
			}
		}
		else if (DamageElementTypeTag.IsValid())
		{
			ValidationLines.Add(TEXT("[错误] 非元素武器不应配置 DamageElementTypeTag。"));
		}

		if (WeaponPropertyType == EActionWeaponPropertyType::Spirit)
		{
			// Spirit 校验在编辑器报告里继续展开为多条提示/错误，
			// 方便资产作者同时看见“没有配置条目”和“条目细节不完整”这两层问题。
			if (!HasAnySpiritAbilityEntryConfigs())
			{
				ValidationLines.Add(TEXT("[提示] 灵武器当前还没有配置 SpiritAbilityEntryConfigs。"));
			}

			FString SpiritSkillValidationFailure;
			if (ActionWeaponDefinitionDefaults::TryBuildSpiritAbilityEntryValidationFailure(
				SpiritAbilityEntryConfigs,
				SpiritSkillValidationFailure))
			{
				ValidationLines.Add(FString::Printf(TEXT("[错误] %s"), *SpiritSkillValidationFailure));
			}

			for (int32 EntryIndex = 0; EntryIndex < SpiritAbilityEntryConfigs.Num(); ++EntryIndex)
			{
				const FActionSpiritAbilityEntryConfig& SpiritAbilityEntry = SpiritAbilityEntryConfigs[EntryIndex];
				const FString EntryContext = FString::Printf(TEXT("SpiritAbilityEntryConfigs[%d]"), EntryIndex);
				const FString EntryKindName =
					ActionWeaponDefinitionDefaults::GetSpiritEntryKindName(SpiritAbilityEntry.EntryKind);

				FString EntryFailureReason;
				if (!SpiritAbilityEntry.IsValidConfig(EntryFailureReason))
				{
					ValidationLines.Add(FString::Printf(
						TEXT("[错误] %s（EntryKind=%s）%s"),
						*EntryContext,
						*EntryKindName,
						*EntryFailureReason));
				}

				ActionWeaponDefinitionDefaults::ValidateHeroCombatAbilityCategory(
					SpiritAbilityEntry.AbilityToGrant,
					FString::Printf(TEXT("%s（EntryKind=%s）.AbilityToGrant"), *EntryContext, *EntryKindName),
					ValidationLines);

				if (SpiritAbilityEntry.IsOffensiveSpiritSkill())
				{
					for (int32 ClipIndex = 0; ClipIndex < SpiritAbilityEntry.SpiritSkillConfig.SkillClips.Num(); ++ClipIndex)
					{
						const FActionSpiritSkillClipConfig& SkillClip =
							SpiritAbilityEntry.SpiritSkillConfig.SkillClips[ClipIndex];
						const FString ClipContext = FString::Printf(TEXT("%s.SkillClips[%d]"), *EntryContext, ClipIndex);
						ActionWeaponDefinitionDefaults::ValidateHitConfigEffects(
							SkillClip.HitConfig,
							AllowsAdditionalHitEffects(),
							FString::Printf(TEXT("%s.HitConfig"), *ClipContext),
							ValidationLines);

						if (!SkillClip.bShouldSpawnProjectile)
						{
							continue;
						}

						if (SkillClip.ProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig)
						{
							ActionWeaponDefinitionDefaults::ValidateProjectileConfig(
								DefaultProjectileConfig,
								FString::Printf(TEXT("%s 依赖的武器默认发射物配置"), *ClipContext),
								ValidationLines);
						}
						else
						{
							ActionWeaponDefinitionDefaults::ValidateProjectileConfig(
								SkillClip.ProjectileSpawnConfig.ProjectileConfigOverride,
								FString::Printf(TEXT("%s 的发射物配置"), *ClipContext),
								ValidationLines);
						}
					}
				}
				else if (SpiritAbilityEntry.IsSelfEnhanceSpiritSkill())
				{
					for (int32 ClipIndex = 0; ClipIndex < SpiritAbilityEntry.SpiritSkillConfig.SkillClips.Num(); ++ClipIndex)
					{
						const FActionSpiritSkillClipConfig& SkillClip =
							SpiritAbilityEntry.SpiritSkillConfig.SkillClips[ClipIndex];
						if (SkillClip.HasAnyOffensiveConfiguredData())
						{
							ValidationLines.Add(FString::Printf(
								TEXT("[错误] %s（EntryKind=%s）的 SkillClips[%d] 不允许配置攻击字段：HitConfig / Projectile。"),
								*EntryContext,
								*EntryKindName,
								ClipIndex));
						}
					}
				}
			}

			if (!HasAnySpiritSkillAbilityEntryConfigs())
			{
				ValidationLines.Add(TEXT("[提示] 灵武器当前还没有配置任何正式 Spirit 主动技能条目。"));
			}
		}
		else
		{
			if (SpiritAbilityEntryConfigs.Num() > 0)
			{
				ValidationLines.Add(TEXT("[错误] 只有灵武器才允许配置 Spirit Ability 条目。"));
			}
		}

		if (WeaponCategory != EHeroWeaponCategory::PureRanged)
		{
			if (bSupportsProjectileSwitching)
			{
				ValidationLines.Add(TEXT("[错误] 只有 Player.Weapon.Ranged.Staff 小类别的纯远程武器才允许开启 bSupportsProjectileSwitching。"));
			}

			if (SwitchableProjectileConfigs.Num() > 0)
			{
				ValidationLines.Add(TEXT("[错误] 只有 Player.Weapon.Ranged.Staff 小类别的纯远程武器才允许配置 SwitchableProjectileConfigs。"));
			}
		}
		else
		{
			if (bSupportsProjectileSwitching && WeaponSubtypeTag != ActionGameplayTags::Player_Weapon_Ranged_Staff)
			{
				ValidationLines.Add(TEXT("[错误] 只有 Player.Weapon.Ranged.Staff 小类别才允许开启 bSupportsProjectileSwitching。"));
			}

			if (!bSupportsProjectileSwitching && SwitchableProjectileConfigs.Num() > 0)
			{
				ValidationLines.Add(TEXT("[错误] 未开启 bSupportsProjectileSwitching 时，不应配置 SwitchableProjectileConfigs。"));
			}

			if (bSupportsProjectileSwitching && SwitchableProjectileConfigs.Num() <= 0)
			{
				ValidationLines.Add(TEXT("[错误] 已开启 bSupportsProjectileSwitching，但没有配置 SwitchableProjectileConfigs。"));
			}
		}
	}

	FString AttackConfigValidationFailure;
	if (!AnimationConfig.ValidateRequiredAttackConfig(AttackConfigValidationFailure))
	{
		ValidationLines.Add(FString::Printf(TEXT("[错误] 攻击配置不完整：%s"), *AttackConfigValidationFailure));
	}
	ActionWeaponDefinitionDefaults::ValidateAttackEntryConfigs(AnimationConfig, ValidationLines);
	ActionWeaponDefinitionDefaults::ValidateHitWindowTemplateConfigs(AnimationConfig, ValidationLines);
	ActionWeaponDefinitionDefaults::ValidateExecutionConfig(ExecutionConfig, ValidationLines);
	ActionWeaponDefinitionDefaults::ValidateExecutionHitConfig(
		ExecutionConfig.HitConfig,
		TEXT("处决专用 HitConfig"),
		ValidationLines);
	ActionWeaponDefinitionDefaults::ValidateHitConfigEffects(
		AnimationConfig.SpecialWeaponSwitchClip.HitConfig,
		AllowsAdditionalHitEffects(),
		TEXT("SpecialWeaponSwitchClip.HitConfig"),
		ValidationLines);
	ActionWeaponDefinitionDefaults::ValidateProjectileConfig(
		DefaultProjectileConfig,
		TEXT("武器默认发射物配置"),
		ValidationLines);
	if (DefaultProjectileConfig.HasAnyConfiguredValue() && !DefaultProjectileConfig.HasAnyDamageConfig())
	{
		ValidationLines.Add(TEXT("[提示] 武器默认发射物配置尚未填写自己的等级驱动直伤，当前会回落到基础命中配置。"));
	}
	{
		TSet<FGameplayTag> ExistingProjectileConfigTags;
		for (int32 EntryIndex = 0; EntryIndex < SwitchableProjectileConfigs.Num(); ++EntryIndex)
		{
			const FActionSwitchableProjectileConfigEntry& Entry = SwitchableProjectileConfigs[EntryIndex];
			FString ProjectileValidationFailure;
			if (ActionWeaponDefinitionDefaults::TryBuildSwitchableProjectileEntryValidationFailure(
				Entry,
				FString::Printf(TEXT("可切换发射物配置[%d]"), EntryIndex),
				ProjectileValidationFailure))
			{
				ValidationLines.Add(FString::Printf(TEXT("[错误] %s"), *ProjectileValidationFailure));
				continue;
			}

			if (ExistingProjectileConfigTags.Contains(Entry.ProjectileConfigTag))
			{
				ValidationLines.Add(FString::Printf(
					TEXT("[错误] SwitchableProjectileConfigs 中存在重复的 ProjectileConfigTag。Tag=%s"),
					*Entry.ProjectileConfigTag.ToString()));
				continue;
			}

			ExistingProjectileConfigTags.Add(Entry.ProjectileConfigTag);
		}
	}
	for (const FActionAttackEntryConfig& AttackEntryConfig : AnimationConfig.AttackEntryConfigs)
	{
		// 逐攻击条目回读时，统一补齐命中附加效果与发射物配置的静态桥接校验。
		// 这样攻击段里混出的缺口能直接落到具体 RequestTag 和 ClipIndex。
		for (int32 ClipIndex = 0; ClipIndex < AttackEntryConfig.AttackClips.Num(); ++ClipIndex)
		{
			const FActionAttackClipConfig& AttackClip = AttackEntryConfig.AttackClips[ClipIndex];
			const FString ClipContextText = FString::Printf(
				TEXT("攻击条目 %s 的攻击段落[%d]"),
				*AttackEntryConfig.RequestTag.ToString(),
				ClipIndex);

			ActionWeaponDefinitionDefaults::ValidateHitConfigEffects(
				AttackClip.HitConfig,
				AllowsAdditionalHitEffects(),
				FString::Printf(TEXT("%s HitConfig"), *ClipContextText),
				ValidationLines);

			if (AttackClip.bShouldSpawnProjectile)
			{
				if (AttackClip.ProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig)
				{
					ActionWeaponDefinitionDefaults::ValidateProjectileConfig(
						DefaultProjectileConfig,
						FString::Printf(TEXT("%s 依赖的武器默认发射物配置"), *ClipContextText),
						ValidationLines);
				}
				else
				{
					ActionWeaponDefinitionDefaults::ValidateProjectileConfig(
						AttackClip.ProjectileSpawnConfig.ProjectileConfigOverride,
						FString::Printf(TEXT("%s 发射物配置"), *ClipContextText),
						ValidationLines);
					if (AttackClip.ProjectileSpawnConfig.ProjectileConfigOverride.HasAnyConfiguredValue()
						&& !AttackClip.ProjectileSpawnConfig.ProjectileConfigOverride.HasAnyDamageConfig())
					{
						ValidationLines.Add(FString::Printf(
							TEXT("[提示] %s 的发射物配置尚未填写自己的等级驱动直伤，当前会回落到基础命中配置。"),
							*ClipContextText));
					}
				}
			}
		}
	}

	if (WeaponCategory == EHeroWeaponCategory::Unarmed)
	{
		if (!WeaponActorClass.IsNull())
		{
			ValidationLines.Add(TEXT("[提示] 空手武器通常不需要 WeaponActorClass，当前存在额外配置。"));
		}
	}
	else if (WeaponActorClass.IsNull())
	{
		ValidationLines.Add(TEXT("[错误] 非空手武器未配置 WeaponActorClass。"));
	}
	ActionWeaponDefinitionDefaults::ValidateCommonAnimationResources(AnimationConfig, WeaponCategory, ValidationLines);

	if (ValidationLines.Num() <= 0)
	{
		return TEXT("WeaponDefinition 校验通过：武器定义的类别、攻击链壳层和基础资源引用符合当前框架要求。");
	}

	return FString::Join(ValidationLines, TEXT("\r\n"));
}

void UDataAsset_WeaponDefinition::ValidateAndLogWeaponDefinition() const
{
	const FString ValidationReport = BuildEditorValidationReport();

	// 日志出口只负责把“校验通过 / 校验失败”的完整报告打出来。
	// 它不在这里再做二次判断或额外修补。
	if (ValidationReport.StartsWith(TEXT("WeaponDefinition 校验通过")))
	{
		UE_LOG(LogWeaponDefinitionData, Log, TEXT("%s"), *ValidationReport);
	}
	else
	{
		UE_LOG(LogWeaponDefinitionData, Warning, TEXT("%s"), *ValidationReport);
	}
}

void UDataAsset_WeaponDefinition::ApplyRecommendedPreset(
	const EHeroWeaponCategory InWeaponCategory,
	const FGameplayTag& InWeaponSubtypeTag,
	const FGameplayTag& InWeaponTag)
{
	// 推荐预设属于“整体重置型”操作：
	// 会统一类别、Tag、默认武器类，并清空已有动画引用后重新搭一套攻击链壳层。
	WeaponCategory = InWeaponCategory;
	WeaponSubtypeTag = InWeaponSubtypeTag;
	WeaponTag = InWeaponTag;
	WeaponActorClass = ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponActorClass(InWeaponCategory);

	ActionWeaponDefinitionDefaults::ClearAnimationAssetReferences(AnimationConfig);
	ExecutionConfig.ExecutionMontage.Reset();
	ResetAllAttackConfigsToDefault();
}

void UDataAsset_WeaponDefinition::RepairDefinitionShell(
	const EHeroWeaponCategory InWeaponCategory,
	const bool bOverrideWeaponCategory)
{
	if (bOverrideWeaponCategory)
	{
		// 显式修某一类壳层时，先把资产类别切到目标类别，再做后续保守修复。
		WeaponCategory = InWeaponCategory;
	}

	// 这一步只修当前类别下最低限度的结构壳层，不主动清空已配好的动画或命中资源。
	// 和 ApplyRecommendedPreset 的“整体重置型”路径不同，它的目标是保守补齐当前正式结构。
	NormalizeAttackConfigArrays();

	if (!WeaponTag.IsValid())
	{
		WeaponTag = ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponTag(WeaponCategory, WeaponSubtypeTag);
	}

	if (!WeaponSubtypeTag.IsValid()
		|| !ActionWeaponDefinitionDefaults::IsAllowedWeaponSubtypeTag(WeaponCategory, WeaponSubtypeTag))
	{
		WeaponSubtypeTag = ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponSubtypeTag(WeaponCategory);
	}

	if (!WeaponTag.IsValid() || !HasMatchingWeaponTagHierarchy() || !HasMatchingCategoryRootTag())
	{
		WeaponTag = ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponTag(WeaponCategory, WeaponSubtypeTag);
	}

	if (WeaponCategory == EHeroWeaponCategory::Unarmed)
	{
		WeaponActorClass.Reset();
	}
	else if (WeaponActorClass.IsNull())
	{
		WeaponActorClass = ActionWeaponDefinitionDefaults::ResolveRecommendedWeaponActorClass(WeaponCategory);
	}
}
