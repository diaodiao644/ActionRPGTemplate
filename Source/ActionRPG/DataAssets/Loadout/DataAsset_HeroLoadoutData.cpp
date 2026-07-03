// 文件说明：实现英雄负载配置资产的规范化固定武器槽构建逻辑。

#include "DataAssets/Loadout/DataAsset_HeroLoadoutData.h"

#include "AbilitySystem/Abilities/Hero/HeroGA_AttackHybrid.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_AttackMelee.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_AttackRanged.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_AttackUnarmed.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_CombatModeOrDefense.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Dodge.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_ProjectileSwitch.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_SpiritSkill.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroLoadoutData, Log, All);

namespace HeroLoadoutDataEditor
{
	static FHeroWeaponLoadoutDefinition* FindLoadoutDefinitionBySlot(
		TArray<FHeroWeaponLoadoutDefinition>& InOutWeaponLoadoutDefinitions,
		const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		for (FHeroWeaponLoadoutDefinition& WeaponLoadoutDefinition : InOutWeaponLoadoutDefinitions)
		{
			if (WeaponLoadoutDefinition.LoadoutSlot == InLoadoutSlot)
			{
				return &WeaponLoadoutDefinition;
			}
		}

		return nullptr;
	}

	static FString GetLoadoutSlotName(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		if (const UEnum* LoadoutSlotEnum = StaticEnum<EHeroWeaponLoadoutSlot>())
		{
			return LoadoutSlotEnum->GetNameStringByValue(static_cast<int64>(InLoadoutSlot));
		}

		return FString::Printf(TEXT("%d"), static_cast<int32>(InLoadoutSlot));
	}

	static void AppendRecommendedAttackAbilityTemplate(
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		FHeroLoadoutAttackAbilityConfig& InOutAttackAbilities)
	{
		// 这里把“固定武器槽 -> 五条主攻击 GA 模板”的关系集中收口。
		// 这样角色负载资产只需要点一次模板填充，就能拿到与当前四槽框架一致的攻击能力组合。
		switch (InLoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			InOutAttackAbilities.LightAttackAbility = UHeroGA_AttackUnarmedLight::StaticClass();
			InOutAttackAbilities.HeavyAttackAbility = UHeroGA_AttackUnarmedHeavy::StaticClass();
			InOutAttackAbilities.DodgeCounterAttackAbility = UHeroGA_AttackUnarmedDodgeCounter::StaticClass();
			InOutAttackAbilities.SprintAttackAbility = UHeroGA_AttackUnarmedSprint::StaticClass();
			InOutAttackAbilities.AirborneAttackAbility = UHeroGA_AttackUnarmedAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			InOutAttackAbilities.LightAttackAbility = UHeroGA_AttackMeleeLight::StaticClass();
			InOutAttackAbilities.HeavyAttackAbility = UHeroGA_AttackMeleeHeavy::StaticClass();
			InOutAttackAbilities.DodgeCounterAttackAbility = UHeroGA_AttackMeleeDodgeCounter::StaticClass();
			InOutAttackAbilities.SprintAttackAbility = UHeroGA_AttackMeleeSprint::StaticClass();
			InOutAttackAbilities.AirborneAttackAbility = UHeroGA_AttackMeleeAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::RangedWeapon:
			InOutAttackAbilities.LightAttackAbility = UHeroGA_AttackRangedLight::StaticClass();
			InOutAttackAbilities.HeavyAttackAbility = UHeroGA_AttackRangedHeavy::StaticClass();
			InOutAttackAbilities.DodgeCounterAttackAbility = UHeroGA_AttackRangedDodgeCounter::StaticClass();
			InOutAttackAbilities.SprintAttackAbility = UHeroGA_AttackRangedSprint::StaticClass();
			InOutAttackAbilities.AirborneAttackAbility = UHeroGA_AttackRangedAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::HybridWeapon:
			InOutAttackAbilities.LightAttackAbility = UHeroGA_AttackHybridLight::StaticClass();
			InOutAttackAbilities.HeavyAttackAbility = UHeroGA_AttackHybridHeavy::StaticClass();
			InOutAttackAbilities.DodgeCounterAttackAbility = UHeroGA_AttackHybridDodgeCounter::StaticClass();
			InOutAttackAbilities.SprintAttackAbility = UHeroGA_AttackHybridSprint::StaticClass();
			InOutAttackAbilities.AirborneAttackAbility = UHeroGA_AttackHybridAirborne::StaticClass();
			break;

		default:
			break;
		}
	}

	static void AppendRecommendedAdditionalAbilities(TArray<FActionAbilitySet>& OutAdditionalGrantedAbilities)
	{
		// 这些额外能力属于“槽位内通用战斗能力”，
		// 它们跟随当前槽位一起授予，但并不属于五条主攻击能力的一部分。
		OutAdditionalGrantedAbilities.Reset();

		FActionAbilitySet DodgeAbilitySet;
		DodgeAbilitySet.InputTag = ActionGameplayTags::InputTag_GameplayAbility_Dodge;
		DodgeAbilitySet.AbilityToGrant = UHeroGA_Dodge::StaticClass();
		OutAdditionalGrantedAbilities.Add(DodgeAbilitySet);

		FActionAbilitySet CombatModeOrDefenseAbilitySet;
		CombatModeOrDefenseAbilitySet.InputTag = ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense;
		CombatModeOrDefenseAbilitySet.AbilityToGrant = UHeroGA_CombatModeOrDefense::StaticClass();
		OutAdditionalGrantedAbilities.Add(CombatModeOrDefenseAbilitySet);
	}

	static void AppendRecommendedProjectileSwitchAbility(TArray<FActionAbilitySet>& OutAdditionalGrantedAbilities)
	{
		// 发射物切换目前是“法杖类远程武器的通用入口”，
		// 这里把它挂在远程槽模板里，只保留一份常驻能力，避免四槽常驻 GA 争抢同一输入标签。
		const bool bAlreadyExists = OutAdditionalGrantedAbilities.ContainsByPredicate([](const FActionAbilitySet& AbilitySet)
		{
			return AbilitySet.InputTag == ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch;
		});
		if (bAlreadyExists)
		{
			return;
		}

		FActionAbilitySet ProjectileSwitchAbilitySet;
		ProjectileSwitchAbilitySet.InputTag = ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch;
		ProjectileSwitchAbilitySet.AbilityToGrant = UHeroGA_ProjectileSwitch::StaticClass();
		OutAdditionalGrantedAbilities.Add(ProjectileSwitchAbilitySet);
	}

	static void AppendRecommendedReactiveAbilities(TArray<FActionAbilitySet>& OutReactiveAbilities)
	{
		// 目标侧处决硬锁现在由被处决蒙太奇内的 ExecutionRecoveryUnlock Notify 解除。
		// 当前默认没有需要自动补齐的事件驱动能力。
		OutReactiveAbilities.Reset();
	}

	static void ApplyRecommendedAbilityTemplateToLoadoutDefinition(
		FHeroWeaponLoadoutDefinition& InOutLoadoutDefinition)
	{
		// 这条入口只负责把“固定武器槽 -> 推荐能力模板”的静态关系落回当前槽位定义。
		// 它不直接授予运行时能力，也不在这里决定角色当前真正装备了哪把武器。
		switch (InOutLoadoutDefinition.LoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			// 空手槽当前使用“空手武器数据 + 通用攻击 GA”这条链路。
			// 虽然能力类暂时与其他槽一致，但单独保留这个分支，后续若空手要独立 GA，可直接在这里分叉。
			AppendRecommendedAttackAbilityTemplate(InOutLoadoutDefinition.LoadoutSlot, InOutLoadoutDefinition.AttackAbilities);
			AppendRecommendedAdditionalAbilities(InOutLoadoutDefinition.AdditionalGrantedAbilities);
			break;

		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			// 近战槽当前使用同一套攻击 GA，请求差异由武器数据和槽位输入标签路由决定。
			AppendRecommendedAttackAbilityTemplate(InOutLoadoutDefinition.LoadoutSlot, InOutLoadoutDefinition.AttackAbilities);
			AppendRecommendedAdditionalAbilities(InOutLoadoutDefinition.AdditionalGrantedAbilities);
			break;

		case EHeroWeaponLoadoutSlot::RangedWeapon:
			// 远程槽先复用同一套攻击 GA，后续若要拆远程专属 GA，也只需要改这里的模板输出。
			AppendRecommendedAttackAbilityTemplate(InOutLoadoutDefinition.LoadoutSlot, InOutLoadoutDefinition.AttackAbilities);
			AppendRecommendedAdditionalAbilities(InOutLoadoutDefinition.AdditionalGrantedAbilities);
			AppendRecommendedProjectileSwitchAbility(InOutLoadoutDefinition.AdditionalGrantedAbilities);
			break;

		case EHeroWeaponLoadoutSlot::HybridWeapon:
			// 混合槽同理，先保持统一框架，给未来近战/远程混合技留出独立模板入口。
			AppendRecommendedAttackAbilityTemplate(InOutLoadoutDefinition.LoadoutSlot, InOutLoadoutDefinition.AttackAbilities);
			AppendRecommendedAdditionalAbilities(InOutLoadoutDefinition.AdditionalGrantedAbilities);
			break;

		default:
			break;
		}
	}

	static void AppendLine(TArray<FString>& OutLines, const FString& InLine)
	{
		OutLines.Add(InLine);
	}

	static void ValidateAbilityArray(
		const TArray<FActionAbilitySet>& InAbilitySets,
		const FString& InContextText,
		TArray<FString>& OutLines)
	{
		TSet<FGameplayTag> SeenInputTags;
		for (int32 AbilityIndex = 0; AbilityIndex < InAbilitySets.Num(); ++AbilityIndex)
		{
			const FActionAbilitySet& AbilitySet = InAbilitySets[AbilityIndex];
			if (!AbilitySet.IsValid())
			{
				AppendLine(
					OutLines,
					FString::Printf(TEXT("[错误] %s 第 %d 项能力配置不完整。"), *InContextText, AbilityIndex));
				continue;
			}

			if (SeenInputTags.Contains(AbilitySet.InputTag))
			{
				AppendLine(
					OutLines,
					FString::Printf(TEXT("[错误] %s 存在重复输入标签：%s"), *InContextText, *AbilitySet.InputTag.ToString()));
				continue;
			}

			SeenInputTags.Add(AbilitySet.InputTag);
		}
	}

	static bool HasAbilityInputTag(
		const TArray<FActionAbilitySet>& InAbilitySets,
		const FGameplayTag& InInputTag)
	{
		return InAbilitySets.ContainsByPredicate([&InInputTag](const FActionAbilitySet& AbilitySet)
		{
			return AbilitySet.InputTag == InInputTag;
		});
	}

	static void ValidateAttackAbilities(
		const FHeroLoadoutAttackAbilityConfig& InAttackAbilities,
		const FString& InContextText,
		TArray<FString>& OutLines)
	{
		if (!InAttackAbilities.LightAttackAbility)
		{
			AppendLine(OutLines, FString::Printf(TEXT("[错误] %s 未配置轻攻击能力。"), *InContextText));
		}

		if (!InAttackAbilities.HeavyAttackAbility)
		{
			AppendLine(OutLines, FString::Printf(TEXT("[错误] %s 未配置重攻击能力。"), *InContextText));
		}

		if (!InAttackAbilities.DodgeCounterAttackAbility)
		{
			AppendLine(OutLines, FString::Printf(TEXT("[错误] %s 未配置闪避反击能力。"), *InContextText));
		}

		if (!InAttackAbilities.SprintAttackAbility)
		{
			AppendLine(OutLines, FString::Printf(TEXT("[错误] %s 未配置冲刺攻击能力。"), *InContextText));
		}

		if (!InAttackAbilities.AirborneAttackAbility)
		{
			AppendLine(OutLines, FString::Printf(TEXT("[错误] %s 未配置空中攻击能力。"), *InContextText));
		}
	}

	static void ValidateExpectedAbilityClass(
		const TSubclassOf<UGameplayAbility>& InAbilityClass,
		const UClass* InExpectedBaseClass,
		const FString& InAbilityName,
		const FString& InContextText,
		TArray<FString>& OutLines)
	{
		if (!InAbilityClass || !InExpectedBaseClass)
		{
			return;
		}

		if (!InAbilityClass->IsChildOf(InExpectedBaseClass))
		{
			AppendLine(
				OutLines,
				FString::Printf(
					TEXT("[错误] %s 的 %s 类型不正确。当前=%s，期望继承=%s"),
					*InContextText,
					*InAbilityName,
					*GetNameSafe(InAbilityClass),
					*InExpectedBaseClass->GetName()));
		}
	}

	static void ValidateRecommendedAttackAbilityTypes(
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FHeroLoadoutAttackAbilityConfig& InAttackAbilities,
		const FString& InContextText,
		TArray<FString>& OutLines)
	{
		UClass* ExpectedLightClass = nullptr;
		UClass* ExpectedHeavyClass = nullptr;
		UClass* ExpectedDodgeCounterClass = nullptr;
		UClass* ExpectedSprintClass = nullptr;
		UClass* ExpectedAirborneClass = nullptr;

		switch (InLoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			ExpectedLightClass = UHeroGA_AttackUnarmedLight::StaticClass();
			ExpectedHeavyClass = UHeroGA_AttackUnarmedHeavy::StaticClass();
			ExpectedDodgeCounterClass = UHeroGA_AttackUnarmedDodgeCounter::StaticClass();
			ExpectedSprintClass = UHeroGA_AttackUnarmedSprint::StaticClass();
			ExpectedAirborneClass = UHeroGA_AttackUnarmedAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			ExpectedLightClass = UHeroGA_AttackMeleeLight::StaticClass();
			ExpectedHeavyClass = UHeroGA_AttackMeleeHeavy::StaticClass();
			ExpectedDodgeCounterClass = UHeroGA_AttackMeleeDodgeCounter::StaticClass();
			ExpectedSprintClass = UHeroGA_AttackMeleeSprint::StaticClass();
			ExpectedAirborneClass = UHeroGA_AttackMeleeAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::RangedWeapon:
			ExpectedLightClass = UHeroGA_AttackRangedLight::StaticClass();
			ExpectedHeavyClass = UHeroGA_AttackRangedHeavy::StaticClass();
			ExpectedDodgeCounterClass = UHeroGA_AttackRangedDodgeCounter::StaticClass();
			ExpectedSprintClass = UHeroGA_AttackRangedSprint::StaticClass();
			ExpectedAirborneClass = UHeroGA_AttackRangedAirborne::StaticClass();
			break;

		case EHeroWeaponLoadoutSlot::HybridWeapon:
			ExpectedLightClass = UHeroGA_AttackHybridLight::StaticClass();
			ExpectedHeavyClass = UHeroGA_AttackHybridHeavy::StaticClass();
			ExpectedDodgeCounterClass = UHeroGA_AttackHybridDodgeCounter::StaticClass();
			ExpectedSprintClass = UHeroGA_AttackHybridSprint::StaticClass();
			ExpectedAirborneClass = UHeroGA_AttackHybridAirborne::StaticClass();
			break;

		default:
			break;
		}

		ValidateExpectedAbilityClass(
			InAttackAbilities.LightAttackAbility,
			ExpectedLightClass,
			TEXT("LightAttackAbility"),
			InContextText,
			OutLines);
		ValidateExpectedAbilityClass(
			InAttackAbilities.HeavyAttackAbility,
			ExpectedHeavyClass,
			TEXT("HeavyAttackAbility"),
			InContextText,
			OutLines);
		ValidateExpectedAbilityClass(
			InAttackAbilities.DodgeCounterAttackAbility,
			ExpectedDodgeCounterClass,
			TEXT("DodgeCounterAttackAbility"),
			InContextText,
			OutLines);
		ValidateExpectedAbilityClass(
			InAttackAbilities.SprintAttackAbility,
			ExpectedSprintClass,
			TEXT("SprintAttackAbility"),
			InContextText,
			OutLines);
		ValidateExpectedAbilityClass(
			InAttackAbilities.AirborneAttackAbility,
			ExpectedAirborneClass,
			TEXT("AirborneAttackAbility"),
			InContextText,
			OutLines);
	}

	static bool HasAnyProjectileEnabledAttackBranch(const UDataAsset_WeaponDefinition* InWeaponDefinition)
	{
		if (!InWeaponDefinition)
		{
			return false;
		}

		const FActionWeaponAnimationConfig& AnimationConfig = InWeaponDefinition->GetAnimationConfig();
		return AnimationConfig.AttackEntryConfigs.ContainsByPredicate([](const FActionAttackEntryConfig& EntryConfig)
		{
			return EntryConfig.AttackClips.ContainsByPredicate([](const FActionAttackClipConfig& AttackClip)
			{
				return AttackClip.bShouldSpawnProjectile;
			});
		});
	}

	static void ValidateProjectileAttackBranches(
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		const FString& InSlotContextText,
		TArray<FString>& OutLines)
	{
		if (!InWeaponDefinition)
		{
			return;
		}

		const FActionWeaponAnimationConfig& AnimationConfig = InWeaponDefinition->GetAnimationConfig();
		for (const FActionAttackEntryConfig& EntryConfig : AnimationConfig.AttackEntryConfigs)
		{
			for (int32 ClipIndex = 0; ClipIndex < EntryConfig.AttackClips.Num(); ++ClipIndex)
			{
				const FActionAttackClipConfig& AttackClip = EntryConfig.AttackClips[ClipIndex];
				if (!AttackClip.bShouldSpawnProjectile)
				{
					continue;
				}

				const FString BranchTagText = EntryConfig.BranchTag.IsValid()
					? EntryConfig.BranchTag.ToString()
					: TEXT("未配置分支标签");
				const FString ClipContextText = FString::Printf(TEXT("%s Clip[%d]"), *BranchTagText, ClipIndex);

				if (AttackClip.ProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig)
				{
					if (!InWeaponDefinition->GetDefaultProjectileConfig().IsValidConfig())
					{
						AppendLine(
							OutLines,
							FString::Printf(
								TEXT("[错误] %s 的攻击段落 %s 启用了发射物，但要求沿用武器默认发射物配置，而 DefaultProjectileConfig 无效。"),
								*InSlotContextText,
								*ClipContextText));
					}

					continue;
				}

				if (!AttackClip.ProjectileSpawnConfig.ProjectileConfigOverride.IsValidConfig())
				{
					AppendLine(
						OutLines,
						FString::Printf(
							TEXT("[错误] %s 的攻击段落 %s 启用了发射物，但 Clip 级 ProjectileConfigOverride 无效。"),
							*InSlotContextText,
							*ClipContextText));
				}
			}
		}
	}

	static void ValidateRangedAndHybridProjectilePath(
		const FHeroWeaponLoadoutDefinition& InWeaponLoadoutDefinition,
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		const FString& InSlotContextText,
		TArray<FString>& OutLines)
	{
		if (!InWeaponDefinition)
		{
			return;
		}

		ValidateProjectileAttackBranches(InWeaponDefinition, InSlotContextText, OutLines);

		const bool bHasProjectileEnabledBranch = HasAnyProjectileEnabledAttackBranch(InWeaponDefinition);
		if (InWeaponDefinition->GetWeaponCategory() == EHeroWeaponCategory::PureRanged
			&& !bHasProjectileEnabledBranch)
		{
			AppendLine(
				OutLines,
				FString::Printf(
					TEXT("[提示] %s 的默认纯远程武器当前没有任何 bShouldSpawnProjectile=true 的攻击分支，蓝图下无法验证远程发射物主链。"),
					*InSlotContextText));
		}

		if (InWeaponLoadoutDefinition.LoadoutSlot == EHeroWeaponLoadoutSlot::HybridWeapon
			&& !bHasProjectileEnabledBranch)
		{
			AppendLine(
				OutLines,
				FString::Printf(
					TEXT("[提示] %s 的默认混合武器当前没有任何 bShouldSpawnProjectile=true 的攻击分支，蓝图下无法验证混合武器发射物链。"),
					*InSlotContextText));
		}
	}
}

UDataAsset_HeroLoadoutData::UDataAsset_HeroLoadoutData()
{
	// 新建负载资产时，默认直接给出完整四固定槽模板结构。
	// 这样蓝图里只能改“每个槽里装什么”，而不会从结构层误增误删槽位。
	HeroLoadoutDataEditor::AppendRecommendedReactiveAbilities(ReactiveAbilities);
	ResetWeaponLoadoutDefinitionsToDefault();
}

void UDataAsset_HeroLoadoutData::NormalizeWeaponLoadoutDefinitions()
{
	// 无论编辑器里当前数组顺序是否被改乱，运行前都统一整理回固定四槽语义。
	// 这一步是“收回当前正式四槽顺序”，不是整轮重置能力模板。
	TArray<FHeroWeaponLoadoutDefinition> NormalizedDefinitions;
	BuildNormalizedWeaponLoadoutDefinitions(NormalizedDefinitions);
	WeaponLoadoutDefinitions = MoveTemp(NormalizedDefinitions);
}

void UDataAsset_HeroLoadoutData::ResetWeaponLoadoutDefinitionsToDefault()
{
	// 当前固定四槽模板没有“额外预设层”和“运行时派生层”的区别，
	// 因此重建默认结构本质上就是重新做一次规范化固定四槽壳。
	// 它服务的是资产结构重建，不等于运行时重新装备或清空角色状态。
	NormalizeWeaponLoadoutDefinitions();
}

void UDataAsset_HeroLoadoutData::ApplyRecommendedWeaponLoadoutAbilityTemplates()
{
	// 这一键模板只负责把四槽对应的推荐能力壳层补齐，
	// 不会替你生成武器资产，也不会覆盖默认武器定义引用。
	NormalizeWeaponLoadoutDefinitions();

	for (FHeroWeaponLoadoutDefinition& WeaponLoadoutDefinition : WeaponLoadoutDefinitions)
	{
		HeroLoadoutDataEditor::ApplyRecommendedAbilityTemplateToLoadoutDefinition(WeaponLoadoutDefinition);
	}
}

void UDataAsset_HeroLoadoutData::ApplyRecommendedUnarmedLoadoutAbilityTemplate()
{
	NormalizeWeaponLoadoutDefinitions();

	if (FHeroWeaponLoadoutDefinition* LoadoutDefinition =
		HeroLoadoutDataEditor::FindLoadoutDefinitionBySlot(WeaponLoadoutDefinitions, EHeroWeaponLoadoutSlot::Unarmed))
	{
		HeroLoadoutDataEditor::ApplyRecommendedAbilityTemplateToLoadoutDefinition(*LoadoutDefinition);
	}
}

void UDataAsset_HeroLoadoutData::ApplyRecommendedMeleeLoadoutAbilityTemplate()
{
	NormalizeWeaponLoadoutDefinitions();

	if (FHeroWeaponLoadoutDefinition* LoadoutDefinition =
		HeroLoadoutDataEditor::FindLoadoutDefinitionBySlot(WeaponLoadoutDefinitions, EHeroWeaponLoadoutSlot::MeleeWeapon))
	{
		HeroLoadoutDataEditor::ApplyRecommendedAbilityTemplateToLoadoutDefinition(*LoadoutDefinition);
	}
}

void UDataAsset_HeroLoadoutData::ApplyRecommendedRangedLoadoutAbilityTemplate()
{
	NormalizeWeaponLoadoutDefinitions();

	if (FHeroWeaponLoadoutDefinition* LoadoutDefinition =
		HeroLoadoutDataEditor::FindLoadoutDefinitionBySlot(WeaponLoadoutDefinitions, EHeroWeaponLoadoutSlot::RangedWeapon))
	{
		HeroLoadoutDataEditor::ApplyRecommendedAbilityTemplateToLoadoutDefinition(*LoadoutDefinition);
	}
}

void UDataAsset_HeroLoadoutData::ApplyRecommendedHybridLoadoutAbilityTemplate()
{
	NormalizeWeaponLoadoutDefinitions();

	if (FHeroWeaponLoadoutDefinition* LoadoutDefinition =
		HeroLoadoutDataEditor::FindLoadoutDefinitionBySlot(WeaponLoadoutDefinitions, EHeroWeaponLoadoutSlot::HybridWeapon))
	{
		HeroLoadoutDataEditor::ApplyRecommendedAbilityTemplateToLoadoutDefinition(*LoadoutDefinition);
	}
}

FString UDataAsset_HeroLoadoutData::BuildLoadoutValidationReport() const
{
	TArray<FString> ValidationLines;

	// 先统一检查三组全局能力数组，再继续下钻固定四槽。
	// 校验报告只服务编辑器整理与资产回读，不替代运行时正式资格判定。
	HeroLoadoutDataEditor::ValidateAbilityArray(ActivateOnGivenAbilities, TEXT("ActivateOnGivenAbilities"), ValidationLines);
	HeroLoadoutDataEditor::ValidateAbilityArray(ReactiveAbilities, TEXT("ReactiveAbilities"), ValidationLines);
	HeroLoadoutDataEditor::ValidateAbilityArray(PersistentInputAbilities, TEXT("PersistentInputAbilities"), ValidationLines);

	const EHeroWeaponLoadoutSlot ExpectedSlots[] =
	{
		EHeroWeaponLoadoutSlot::Unarmed,
		EHeroWeaponLoadoutSlot::MeleeWeapon,
		EHeroWeaponLoadoutSlot::RangedWeapon,
		EHeroWeaponLoadoutSlot::HybridWeapon
	};

	if (WeaponLoadoutDefinitions.Num() != UE_ARRAY_COUNT(ExpectedSlots))
	{
		HeroLoadoutDataEditor::AppendLine(
			ValidationLines,
			FString::Printf(TEXT("[错误] WeaponLoadoutDefinitions 数量应为 %d，当前为 %d。"),
				UE_ARRAY_COUNT(ExpectedSlots),
				WeaponLoadoutDefinitions.Num()));
	}

	for (int32 SlotIndex = 0; SlotIndex < WeaponLoadoutDefinitions.Num(); ++SlotIndex)
	{
		// 进入逐槽位校验后，报告会同时覆盖：
		// 固定四槽顺序、槽位语义、主攻击能力模板、额外能力数组，以及默认武器定义能否与当前槽位兼容。
		const FHeroWeaponLoadoutDefinition& WeaponLoadoutDefinition = WeaponLoadoutDefinitions[SlotIndex];
		const FString SlotName = HeroLoadoutDataEditor::GetLoadoutSlotName(WeaponLoadoutDefinition.LoadoutSlot);
		const FString SlotContextText = FString::Printf(TEXT("固定武器槽[%d:%s]"), SlotIndex, *SlotName);

		if (SlotIndex < UE_ARRAY_COUNT(ExpectedSlots) && WeaponLoadoutDefinition.LoadoutSlot != ExpectedSlots[SlotIndex])
		{
			HeroLoadoutDataEditor::AppendLine(
				ValidationLines,
				FString::Printf(
					TEXT("[错误] %s 顺序不正确，期望槽位=%s。"),
					*SlotContextText,
					*HeroLoadoutDataEditor::GetLoadoutSlotName(ExpectedSlots[SlotIndex])));
		}

		if (!WeaponLoadoutDefinition.IsValidDefinition())
		{
			HeroLoadoutDataEditor::AppendLine(
				ValidationLines,
				FString::Printf(TEXT("[错误] %s 的槽位语义配置非法。"), *SlotContextText));
		}

		HeroLoadoutDataEditor::ValidateAttackAbilities(
			WeaponLoadoutDefinition.AttackAbilities,
			SlotContextText,
			ValidationLines);
		HeroLoadoutDataEditor::ValidateRecommendedAttackAbilityTypes(
			WeaponLoadoutDefinition.LoadoutSlot,
			WeaponLoadoutDefinition.AttackAbilities,
			SlotContextText,
			ValidationLines);

		HeroLoadoutDataEditor::ValidateAbilityArray(
			WeaponLoadoutDefinition.AdditionalGrantedAbilities,
			FString::Printf(TEXT("%s.AdditionalGrantedAbilities"), *SlotContextText),
			ValidationLines);

		for (const FActionAbilitySet& AbilitySet : WeaponLoadoutDefinition.AdditionalGrantedAbilities)
		{
			if (AbilitySet.InputTag.MatchesTag(ActionGameplayTags::InputTag_GameplayAbility_Attack))
			{
				HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(
						TEXT("[错误] %s 的 AdditionalGrantedAbilities 中不应放攻击输入标签：%s"),
						*SlotContextText,
						*AbilitySet.InputTag.ToString()));
			}

			if (ActionGameplayTags::IsSpiritSkillInputTag(AbilitySet.InputTag)
				|| (AbilitySet.AbilityToGrant
					&& AbilitySet.AbilityToGrant->IsChildOf(UHeroGA_SpiritSkill::StaticClass())))
			{
				HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(
						TEXT("[错误] %s 的 AdditionalGrantedAbilities 中不应放 Spirit 主动技能输入或 UHeroGA_SpiritSkill。Spirit 主动技能当前只允许在 WeaponDefinition 的 SpiritAbilityEntryConfigs 中配置。"),
						*SlotContextText));
			}

			if (AbilitySet.InputTag == ActionGameplayTags::InputTag_GameplayAbility_Dodge
				&& AbilitySet.AbilityToGrant
				&& !AbilitySet.AbilityToGrant->IsChildOf(UHeroGA_Dodge::StaticClass()))
			{
				HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(
						TEXT("[错误] %s 的 Dodge 输入应绑定 UHeroGA_Dodge 派生类。当前=%s"),
						*SlotContextText,
						*GetNameSafe(AbilitySet.AbilityToGrant)));
			}

			if (AbilitySet.InputTag == ActionGameplayTags::InputTag_GameplayAbility_CombatModeOrDefense
				&& AbilitySet.AbilityToGrant
				&& !AbilitySet.AbilityToGrant->IsChildOf(UHeroGA_CombatModeOrDefense::StaticClass()))
			{
				HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(
						TEXT("[错误] %s 的 CombatModeOrDefense 输入应绑定 UHeroGA_CombatModeOrDefense 派生类。当前=%s"),
						*SlotContextText,
						*GetNameSafe(AbilitySet.AbilityToGrant)));
			}
		}

		if (WeaponLoadoutDefinition.LoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed
			&& WeaponLoadoutDefinition.DefaultWeaponDefinition.IsNull())
		{
			HeroLoadoutDataEditor::AppendLine(
				ValidationLines,
				FString::Printf(TEXT("[错误] %s 为空手槽，必须配置默认武器定义。"), *SlotContextText));
		}

		if (!WeaponLoadoutDefinition.DefaultWeaponDefinition.IsNull())
		{
			if (const UDataAsset_WeaponDefinition* WeaponDefinition = WeaponLoadoutDefinition.DefaultWeaponDefinition.LoadSynchronous())
			{
				// 默认武器定义在这里仍然只作为静态资产继续校验。
				// 真正运行时是否成功装备、是否允许切换发射物，仍由外层装备链和攻击链负责。
				if (!WeaponDefinition->MatchesWeaponCategory(WeaponLoadoutDefinition.AllowedWeaponCategory))
				{
					HeroLoadoutDataEditor::AppendLine(
						ValidationLines,
						FString::Printf(
							TEXT("[错误] %s 的默认武器类别与槽位允许类别不匹配。WeaponTag=%s"),
							*SlotContextText,
							*WeaponDefinition->WeaponTag.ToString()));
				}

				const FString WeaponValidationFailure = WeaponDefinition->DescribeValidationFailure();
				if (!WeaponValidationFailure.IsEmpty())
				{
					HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(
						TEXT("[错误] %s 的默认武器定义本身非法：%s"),
						*SlotContextText,
						*WeaponValidationFailure));
				}

				if (WeaponDefinition->SupportsProjectileSwitching()
					&& !HeroLoadoutDataEditor::HasAbilityInputTag(
						WeaponLoadoutDefinition.AdditionalGrantedAbilities,
						ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch))
				{
					HeroLoadoutDataEditor::AppendLine(
						ValidationLines,
						FString::Printf(
							TEXT("[提示] %s 的默认武器支持切换发射物，但 AdditionalGrantedAbilities 未包含 ProjectileSwitch 输入能力。"),
							*SlotContextText));
				}

				HeroLoadoutDataEditor::ValidateRangedAndHybridProjectilePath(
					WeaponLoadoutDefinition,
					WeaponDefinition,
					SlotContextText,
					ValidationLines);
			}
			else
			{
				HeroLoadoutDataEditor::AppendLine(
					ValidationLines,
					FString::Printf(TEXT("[错误] %s 的默认武器定义无法加载。"), *SlotContextText));
			}
		}
	}

	if (ValidationLines.Num() <= 0)
	{
		return TEXT("HeroLoadoutData 校验通过：固定武器槽顺序、主攻击能力、额外能力和默认武器定义均符合当前框架要求。");
	}

	return FString::Join(ValidationLines, TEXT("\r\n"));
}

void UDataAsset_HeroLoadoutData::ValidateAndLogLoadoutData() const
{
	const FString ValidationReport = BuildLoadoutValidationReport();
	const bool bValidationPassed = ValidationReport.StartsWith(TEXT("HeroLoadoutData 校验通过"));

	// 这里统一把校验报告收口到日志，方便资产作者在编辑器里直接查看结果。
	// 只是报告出口，不引入新的校验语义。
	if (bValidationPassed)
	{
		UE_LOG(LogHeroLoadoutData, Log, TEXT("%s"), *ValidationReport);
	}
	else
	{
		UE_LOG(LogHeroLoadoutData, Warning, TEXT("%s"), *ValidationReport);
	}
}

void UDataAsset_HeroLoadoutData::BuildNormalizedWeaponLoadoutDefinitions(TArray<FHeroWeaponLoadoutDefinition>& OutDefinitions) const
{
	TMap<EHeroWeaponLoadoutSlot, FHeroWeaponLoadoutDefinition> ExistingDefinitionsBySlot;
	for (const FHeroWeaponLoadoutDefinition& ExistingDefinition : WeaponLoadoutDefinitions)
	{
		// 先按槽位收口已有配置，后面统一按固定顺序重建输出数组。
		ExistingDefinitionsBySlot.Add(ExistingDefinition.LoadoutSlot, ExistingDefinition);
	}

	OutDefinitions.Reset();

	auto AddFixedLoadoutDefinition = [&ExistingDefinitionsBySlot, &OutDefinitions](
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const bool bInEquipOnSpawn)
	{
		FHeroWeaponLoadoutDefinition LoadoutDefinition;

		// 无论原资产顺序如何，重新输出时都强制写回固定槽位语义和唯一允许的武器类别。
		// 这里构建的是静态槽位定义壳，不生成任何运行时装备态或能力句柄。
		LoadoutDefinition.LoadoutSlot = InLoadoutSlot;
		LoadoutDefinition.AllowedWeaponCategory = FHeroWeaponLoadoutDefinition::ResolveRequiredWeaponCategory(InLoadoutSlot);
		LoadoutDefinition.bEquipOnSpawn = bInEquipOnSpawn;

		if (const FHeroWeaponLoadoutDefinition* ExistingDefinition = ExistingDefinitionsBySlot.Find(InLoadoutSlot))
		{
			// 只继承该槽位允许保留的资产层数据：默认武器定义、主攻击能力和额外常驻能力。
			LoadoutDefinition.DefaultWeaponDefinition = ExistingDefinition->DefaultWeaponDefinition;
			LoadoutDefinition.AttackAbilities = ExistingDefinition->AttackAbilities;
			LoadoutDefinition.AdditionalGrantedAbilities = ExistingDefinition->AdditionalGrantedAbilities;
		}

		// 运行时真正依赖的是“槽位语义正确的四条定义”，
		// 因此这里每次都按标准槽位重新产出，而不是直接信任原数组结构。
		OutDefinitions.Add(LoadoutDefinition);
	};

	// 输出顺序在这里被固定下来，蓝图资产里即使顺序混乱，运行时也统一视为固定武器槽顺序。
	AddFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::Unarmed, true);
	AddFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::MeleeWeapon, false);
	AddFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::RangedWeapon, false);
	AddFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::HybridWeapon, false);
}
