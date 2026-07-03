#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionAnimationTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionType/ActionProjectileTypes.h"
#include "DataAsset_WeaponDefinition.generated.h"

class AHeroWeaponBase;
class FProperty;
struct FPropertyChangedEvent;
class UGameplayAbility;
class UActionHeroLinkedAnimLayer;
class UAnimMontage;

/** Spirit Ability 条目类别。用于区分普通授予条目、SelfEnhance 与 Offensive 两类 Spirit 技能语义。 */
UENUM(BlueprintType)
enum class EActionSpiritAbilityEntryKind : uint8
{
	GrantedAbilityOnly,
	SpiritSkillSelfEnhance,
	SpiritSkillOffensive
};

/**
 * Spirit 技能单段配置。
 * 它只描述单个 SkillClip 的静态模板：
 * 要播什么蒙太奇、是否带命中、是否顺带生成发射物。
 * 运行时真正播到哪一段、当前是否已进入待命链，仍由外层 Spirit runtime 决定。
 */
USTRUCT(BlueprintType)
struct FActionSpiritSkillClipConfig
{
	GENERATED_BODY()

public:
	/** 当前技能段要播放的蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill", meta = (ToolTip = "当前 SkillClip 要播放的蒙太奇资源。它只是这段 Spirit 技能的静态表现模板，不代表运行时已经进入这一段。"))
	TSoftObjectPtr<UAnimMontage> SkillMontage;

	/** 当前技能段需要多高优先级的受击才能接管自己。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill", meta = (ClampMin = "0", ToolTip = "当前 SkillClip 允许被更高优先级受击接管的最小阈值。值越高，越不容易在演出中被普通受击打断。"))
	int32 MinIncomingReactPriorityToInterrupt = 0;

	/** 当前技能段正式使用的命中配置。只对 Offensive 模式生效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive", meta = (EditCondition = "bExposeOffensiveFields", EditConditionHides, ToolTip = "当前 SkillClip 的命中配置。只有 Offensive Spirit 技能段会消费它；SelfEnhance 条目不会读取这里。"))
	FActionWeaponHitConfig HitConfig;

	/** 当前技能段是否额外生成发射物。只对 Offensive 模式生效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Projectile", meta = (EditCondition = "bExposeOffensiveFields", EditConditionHides, ToolTip = "当前 SkillClip 是否在命中链之外额外生成发射物。只有 Offensive Spirit 技能段会读取这组开关。"))
	bool bShouldSpawnProjectile = false;

	/** 当前技能段的发射物生成配置。只对 Offensive 模式生效。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Projectile", meta = (EditCondition = "bExposeOffensiveFields && bShouldSpawnProjectile", EditConditionHides, ToolTip = "当前 SkillClip 额外生成发射物时使用的条目级 Spawn 配置。它只是静态桥接入口，真正解析与生成仍由攻击链执行。"))
	FActionProjectileSpawnConfig ProjectileSpawnConfig;

	UPROPERTY(Transient)
	bool bExposeOffensiveFields = false;

	bool HasAnyConfiguredData() const
	{
		return !SkillMontage.IsNull()
			|| HitConfig.HasAnyConfiguredValue()
			|| bShouldSpawnProjectile
			|| ProjectileSpawnConfig.HasAnyConfiguredValue();
	}

	bool HasAnyOffensiveConfiguredData() const
	{
		return HitConfig.HasAnyConfiguredValue()
			|| bShouldSpawnProjectile
			|| ProjectileSpawnConfig.HasAnyConfiguredValue();
	}

	void SyncEditorExposureFlags(const bool bInExposeOffensiveFields)
	{
		bExposeOffensiveFields = bInExposeOffensiveFields;
	}

	bool IsValidConfig(const bool bAllowsOffensiveFields, FString& OutFailureReason) const
	{
		OutFailureReason.Reset();

		if (SkillMontage.IsNull())
		{
			OutFailureReason = TEXT("SkillClips 中存在未配置 SkillMontage 的表现段。");
			return false;
		}

		if (!bAllowsOffensiveFields && HasAnyOffensiveConfiguredData())
		{
			OutFailureReason = TEXT("SelfEnhance 的当前 SkillClip 不允许配置攻击字段：HitConfig / Projectile。");
			return false;
		}

		return true;
	}

	UAnimMontage* GetSkillMontage() const
	{
		return SkillMontage.Get();
	}

	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!SkillMontage.IsNull())
		{
			OutAssetPaths.AddUnique(SkillMontage.ToSoftObjectPath());
		}

		HitConfig.CollectSoftObjectPaths(OutAssetPaths);
		if (bShouldSpawnProjectile)
		{
			ProjectileSpawnConfig.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};

/**
 * Spirit 主动技能共享配置。
 * 它负责描述一条 Spirit 技能条目的公共静态模板，
 * 包括共享修正、是否启用多段待命、每段 SkillClip 与预期持续时间。
 * 它不是 Spirit 技能当前进行到第几段的运行时状态源。
 */
USTRUCT(BlueprintType)
struct FActionSpiritSkillConfig
{
	GENERATED_BODY()

public:
	/** 运行时调试名。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill", meta = (ToolTip = "这条 Spirit 技能在日志、调试和面板里的显示名。留空时会按 EntryKind 回退到默认中文名。"))
	FString SkillDebugName;

	/** 当前技能共享的自身战斗修正效果。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|SelfEnhance", meta = (EditCondition = "bExposeSelfEnhanceFields", EditConditionHides, ToolTip = "这条 Spirit 技能共享的战斗修正规格。SelfEnhance 条目必须通过它提供正式强化语义；Offensive 条目不再读取这里。"))
	FActionCombatModifierEffectSpec CombatModifierEffect;

	/** 当前技能是否按持久 Spirit 段索引选择 SkillClip。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Combo", meta = (EditCondition = "bExposeOffensiveFields", EditConditionHides, ToolTip = "是否启用持久 Spirit 多段待命链。开启后，运行时会按外层保存的段索引解析当前要播的 SkillClip。"))
	bool bUseComboIndex = false;

	/** 当前技能段正式起手后是否推进到下一次待命索引。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Combo", meta = (EditCondition = "bExposeOffensiveFields && bUseComboIndex", EditConditionHides, ToolTip = "启用多段待命后，当前 SkillClip 成功起手时是否把待命索引推进到下一段。关闭时会反复停留在当前段模板。"))
	bool bAdvanceComboIndexOnPlay = false;

	/** 当前 Spirit 多段待命的超时秒数。只在启用索引时使用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Combo", meta = (ClampMin = "0.0", EditCondition = "bExposeOffensiveFields && bUseComboIndex", EditConditionHides, ToolTip = "启用多段待命后，运行时保留下一段资格的最长等待时长。超过这个时间会回到默认段，而不是继续保留旧待命状态。"))
	float ComboChainTimeoutSeconds = 0.f;

	/** 当前 Spirit 技能成功起手后是否重置 Attack 的全局 ComboIndex。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Offensive|Combo", meta = (EditCondition = "bExposeOffensiveFields", EditConditionHides, ToolTip = "这条 Spirit 技能正式起手后，是否顺带把普通 Attack 链的全局 ComboIndex 重置回初始值。用于避免 Spirit 条目与普通攻击连段互相串态。"))
	bool bResetAttackComboIndexOnActivate = true;

	/** 当前技能的预期持续时间语义。只对 SelfEnhance 模式有意义。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|SelfEnhance", meta = (ClampMin = "0.0", EditCondition = "bExposeSelfEnhanceFields", EditConditionHides, ToolTip = "这条 Spirit 技能预期持续多久。主要给 SelfEnhance 条目的外层逻辑和表现阅读，不直接替代真实效果运行时计时。"))
	float ExpectedDurationSeconds = 0.f;

	/** 当前技能的技能段落数组。SelfEnhance 仍可用它承接表现蒙太奇；Offensive 则继续在此基础上叠加命中与发射物。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility|Skill|Presentation", meta = (ToolTip = "这条 Spirit 技能可用的 SkillClip 静态模板列表。SelfEnhance 也可通过它承接表现蒙太奇；Offensive 则在此基础上继续读取命中、发射物与连段语义。"))
	TArray<FActionSpiritSkillClipConfig> SkillClips;

	UPROPERTY(Transient)
	bool bExposeSelfEnhanceFields = false;

	UPROPERTY(Transient)
	bool bExposeOffensiveFields = false;

	bool HasAnyConfiguredData() const
	{
		return !SkillDebugName.IsEmpty()
			|| CombatModifierEffect.HasAnyConfiguredValue()
			|| bUseComboIndex
			|| bAdvanceComboIndexOnPlay
			|| ComboChainTimeoutSeconds > 0.f
			|| !bResetAttackComboIndexOnActivate
			|| ExpectedDurationSeconds > 0.f
			|| SkillClips.ContainsByPredicate([](const FActionSpiritSkillClipConfig& SkillClip)
			{
				return SkillClip.HasAnyConfiguredData();
			});
	}

	void SyncEditorExposureFlags(const EActionSpiritAbilityEntryKind InEntryKind)
	{
		bExposeSelfEnhanceFields = InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance;
		bExposeOffensiveFields = InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive;

		for (FActionSpiritSkillClipConfig& SkillClip : SkillClips)
		{
			SkillClip.SyncEditorExposureFlags(bExposeOffensiveFields);
		}
	}

	bool IsValidConfig(const EActionSpiritAbilityEntryKind InEntryKind, FString& OutFailureReason) const
	{
		OutFailureReason.Reset();

		if (SkillClips.Num() <= 0)
		{
			OutFailureReason = InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance
				? TEXT("Spirit SelfEnhance 至少需要配置 1 个 SkillClip 作为表现段。")
				: TEXT("Spirit Offensive 至少需要配置 1 个 SkillClip 作为攻击段。");
			return false;
		}

		if (bAdvanceComboIndexOnPlay && !bUseComboIndex)
		{
			OutFailureReason = TEXT("Spirit Offensive 未启用 bUseComboIndex 时，不允许单独开启 bAdvanceComboIndexOnPlay。");
			return false;
		}

		if (ComboChainTimeoutSeconds > 0.f && !bUseComboIndex)
		{
			OutFailureReason = TEXT("Spirit Offensive 未启用 bUseComboIndex 时，不允许填写 ComboChainTimeoutSeconds。");
			return false;
		}

		if (bUseComboIndex
			&& SkillClips.Num() > 1
			&& ComboChainTimeoutSeconds <= 0.f)
		{
			OutFailureReason = TEXT("Spirit Offensive 启用多段索引且存在多个 SkillClip 时，必须填写大于 0 的 ComboChainTimeoutSeconds。");
			return false;
		}

		for (int32 ClipIndex = 0; ClipIndex < SkillClips.Num(); ++ClipIndex)
		{
			FString ClipFailureReason;
			if (!SkillClips[ClipIndex].IsValidConfig(
				InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive,
				ClipFailureReason))
			{
				OutFailureReason = FString::Printf(
					TEXT("SkillClips[%d] 无效：%s"),
					ClipIndex,
					*ClipFailureReason);
				return false;
			}
		}

		if (InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance
			&& !CombatModifierEffect.IsValidSpec())
		{
			OutFailureReason = TEXT("Spirit SelfEnhance 必须配置有效的 CombatModifierEffect。这个字段是 SelfEnhance 的正式强化效果来源。");
			return false;
		}

		if (InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance
			&& (bUseComboIndex
				|| bAdvanceComboIndexOnPlay
				|| ComboChainTimeoutSeconds > 0.f
				|| !bResetAttackComboIndexOnActivate))
		{
			OutFailureReason = TEXT("Spirit SelfEnhance 不允许配置 Combo 字段：bUseComboIndex / bAdvanceComboIndexOnPlay / ComboChainTimeoutSeconds / bResetAttackComboIndexOnActivate。");
			return false;
		}

		if (InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive
			&& CombatModifierEffect.HasAnyConfiguredValue())
		{
			OutFailureReason = TEXT("Spirit Offensive 不允许配置 CombatModifierEffect。这个字段只属于 SelfEnhance 的强化修正语义。");
			return false;
		}

		if (InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive
			&& ExpectedDurationSeconds > 0.f)
		{
			OutFailureReason = TEXT("Spirit Offensive 不允许配置 ExpectedDurationSeconds。这个字段只属于 SelfEnhance 的持续时长语义。");
			return false;
		}

		if (InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance
			&& CombatModifierEffect.HasAnyConfiguredValue()
			&& !CombatModifierEffect.IsValidSpec())
		{
			OutFailureReason = TEXT("SpiritSkillConfig 中的 CombatModifierEffect 配置不完整。");
			return false;
		}

		return true;
	}

	FString ResolveDebugName(const EActionSpiritAbilityEntryKind InEntryKind) const
	{
		if (!SkillDebugName.IsEmpty())
		{
			return SkillDebugName;
		}

		return InEntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive
			? TEXT("灵武器主动攻击")
			: TEXT("灵武器自强化");
	}

	const FActionSpiritSkillClipConfig* ResolveSkillClip(const int32 InClipIndex) const
	{
		if (SkillClips.Num() <= 0)
		{
			return nullptr;
		}

		if (!bUseComboIndex)
		{
			return &SkillClips[0];
		}

		const int32 SafeIndex = FMath::Clamp(InClipIndex, 0, SkillClips.Num() - 1);
		return SkillClips.IsValidIndex(SafeIndex)
			? &SkillClips[SafeIndex]
			: nullptr;
	}

	int32 GetSkillClipCount() const
	{
		return SkillClips.Num();
	}

	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		for (const FActionSpiritSkillClipConfig& SkillClip : SkillClips)
		{
			SkillClip.CollectSoftObjectPaths(OutAssetPaths);
		}
	}
};

/**
 * Spirit Ability 一体化条目。
 * 它把正式输入标签、授予 Ability 与可选的 Spirit 技能共享配置收口到一条静态资产数据里，
 * 方便灵武器在授予能力时直接按条目装配，而不是在多个资产入口间来回拼装。
 */
USTRUCT(BlueprintType)
struct FActionSpiritAbilityEntryConfig
{
	GENERATED_BODY()

public:
	/** 当前条目绑定的正式输入标签。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility", meta = (ToolTip = "这条 Spirit Ability 条目绑定的正式输入标签。Spirit 技能条目必须使用 SpiritSkill1~4；普通 GrantedAbilityOnly 条目不应占用这四个输入。"))
	FGameplayTag InputTag;

	/** 当前条目真正授予到 ASC 的 Ability 类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility", meta = (ToolTip = "真正授予到 ASC 的 Ability 类。数据资产只负责静态装配入口，不在这里保存 Ability 的运行时激活状态。"))
	TSubclassOf<UGameplayAbility> AbilityToGrant;

	/** 当前条目的语义类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility", meta = (ToolTip = "当前条目的正式语义类型。GrantedAbilityOnly 只做普通授予；SelfEnhance / Offensive 会额外读取下面的 SpiritSkillConfig。"))
	EActionSpiritAbilityEntryKind EntryKind = EActionSpiritAbilityEntryKind::GrantedAbilityOnly;

	/** Spirit 主动技能共享配置。仅 SpiritSkill 条目使用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SpiritAbility", meta = (EditCondition = "EntryKind != EActionSpiritAbilityEntryKind::GrantedAbilityOnly", EditConditionHides, ToolTip = "Spirit 技能条目的共享静态模板。只有 EntryKind 为 SelfEnhance 或 Offensive 时才会生效；GrantedAbilityOnly 不应额外配置它。"))
	FActionSpiritSkillConfig SpiritSkillConfig;

	bool HasAnyConfiguredData() const
	{
		return InputTag.IsValid()
			|| AbilityToGrant != nullptr
			|| SpiritSkillConfig.HasAnyConfiguredData();
	}

	bool IsSpiritSkillEntry() const
	{
		return EntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive
			|| EntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance;
	}

	bool IsOffensiveSpiritSkill() const
	{
		return EntryKind == EActionSpiritAbilityEntryKind::SpiritSkillOffensive;
	}

	bool IsSelfEnhanceSpiritSkill() const
	{
		return EntryKind == EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance;
	}

	bool IsValidConfig(FString& OutFailureReason) const
	{
		OutFailureReason.Reset();

		if (!InputTag.IsValid())
		{
			OutFailureReason = TEXT("SpiritAbilityEntryConfig 的 InputTag 无效。");
			return false;
		}

		if (!AbilityToGrant)
		{
			OutFailureReason = TEXT("SpiritAbilityEntryConfig 未配置 AbilityToGrant。");
			return false;
		}

		if (!IsSpiritSkillEntry())
		{
			if (ActionGameplayTags::IsSpiritSkillInputTag(InputTag))
			{
				OutFailureReason = TEXT("GrantedAbilityOnly 条目不应占用 SpiritSkill1~4。请改为 Spirit 技能条目，或改用其它正式输入。");
				return false;
			}

			if (SpiritSkillConfig.HasAnyConfiguredData())
			{
				OutFailureReason = TEXT("GrantedAbilityOnly 条目不应额外配置 SpiritSkillConfig。");
				return false;
			}

			return true;
		}

		if (!ActionGameplayTags::IsSpiritSkillInputTag(InputTag))
		{
			OutFailureReason = TEXT("Spirit 技能条目必须使用 SpiritSkill1~4 作为 InputTag。");
			return false;
		}

		return SpiritSkillConfig.IsValidConfig(EntryKind, OutFailureReason);
	}

	FString ResolveDebugName() const
	{
		if (IsSpiritSkillEntry())
		{
			return SpiritSkillConfig.ResolveDebugName(EntryKind);
		}

		return InputTag.IsValid() ? InputTag.ToString() : TEXT("灵武器授予能力");
	}

	const FActionSpiritSkillClipConfig* ResolveSkillClip(const int32 InClipIndex) const
	{
		return IsSpiritSkillEntry()
			? SpiritSkillConfig.ResolveSkillClip(InClipIndex)
			: nullptr;
	}

	int32 GetSkillClipCount() const
	{
		return IsSpiritSkillEntry() ? SpiritSkillConfig.GetSkillClipCount() : 0;
	}

	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (IsSpiritSkillEntry())
		{
			SpiritSkillConfig.CollectSoftObjectPaths(OutAssetPaths);
		}
	}

	void SyncEditorExposureFlags()
	{
		SpiritSkillConfig.SyncEditorExposureFlags(EntryKind);
	}
};

/**
 * 武器定义资产。
 * 这里收口武器唯一标识、武器类别、运行时武器类、动画配置和命中配置。
 */
UCLASS(BlueprintType)
class ACTIONRPG_API UDataAsset_WeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UDataAsset_WeaponDefinition();

public:
	// 运行时基础校验与分类查询：
	// 这些接口主要给装备链、切武链和运行时防御性检查使用。
	/** 校验当前武器定义是否满足运行时最小要求。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool IsValidDefinition() const;

	/** 返回当前武器定义校验失败的具体原因。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	FString DescribeValidationFailure() const;

	/** 读取武器类别。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	EHeroWeaponCategory GetWeaponCategory() const { return WeaponCategory; }

	/** 解析这把武器当前最适合给日志或 UI 使用的名称。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	FString ResolveDebugName() const;

	/** 判断当前武器是否允许装备进指定类别的固定武器槽。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool MatchesWeaponCategory(EHeroWeaponCategory InWeaponCategory) const;

	/** 判断武器 Tag 是否与自身类别的根 Tag 保持一致。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool HasMatchingCategoryRootTag() const;

	/** 读取当前武器的小类别根标签。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	FGameplayTag GetWeaponSubtypeTag() const { return WeaponSubtypeTag; }

	/** 判断武器小类别是否与当前类别允许的 subtype 集合一致。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool HasMatchingWeaponSubtypeTag() const;

	/** 判断完整 WeaponTag 是否与当前 WeaponSubtypeTag 保持一致。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool HasMatchingWeaponTagHierarchy() const;

	/** 读取当前武器的属性体系类型。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	EActionWeaponPropertyType GetWeaponPropertyType() const { return WeaponPropertyType; }

	/** 读取当前武器的默认伤害大类。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	EActionDamageType GetDamageType() const { return DamageType; }

	/** 读取当前武器的元素伤害子类型。只有元素武器时才有意义。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	FGameplayTag GetDamageElementTypeTag() const { return DamageElementTypeTag; }

	/** 判断当前武器是否允许继续叠加额外命中效果层。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool AllowsAdditionalHitEffects() const { return WeaponPropertyType == EActionWeaponPropertyType::Mundane; }

	/** 判断当前武器是否支持灵武器专属能力。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition")
	bool SupportsSpiritWeaponAbilities() const { return WeaponPropertyType == EActionWeaponPropertyType::Spirit; }

	/** 读取 Spirit Ability 一体化条目集合。 */
	const TArray<FActionSpiritAbilityEntryConfig>& GetSpiritAbilityEntryConfigs() const { return SpiritAbilityEntryConfigs; }

	/** 判断当前是否至少存在一条已配置的 Spirit Ability 条目。 */
	bool HasAnySpiritAbilityEntryConfigs() const;

	/** 判断当前是否至少存在一条已配置的 Spirit 主动技能条目。 */
	bool HasAnySpiritSkillAbilityEntryConfigs() const;

	/** 按输入标签解析当前武器的一条 Spirit Ability 条目。 */
	bool TryResolveSpiritAbilityEntryConfigByInputTag(
		const FGameplayTag& InInputTag,
		FActionSpiritAbilityEntryConfig& OutEntryConfig) const;

	/** 读取武器动画配置。 */
	const FActionWeaponAnimationConfig& GetAnimationConfig() const { return AnimationConfig; }

	/** 读取处决专用配置。 */
	const FActionExecutionConfig& GetExecutionConfig() const { return ExecutionConfig; }

	/** 读取武器默认发射物配置。 */
	const FActionProjectileConfig& GetDefaultProjectileConfig() const { return DefaultProjectileConfig; }

	/** 读取处决专用命中配置。 */
	const FActionExecutionHitConfig& GetExecutionHitConfig() const { return ExecutionConfig.HitConfig; }

	/** 判断当前武器是否允许切换发射物。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition|Projectile")
	bool SupportsProjectileSwitching() const { return bSupportsProjectileSwitching; }

	/** 按一次发射请求解析最终应使用的发射物配置。 */
	bool ResolveProjectileConfigForSpawn(
		const FActionProjectileSpawnConfig& InSpawnConfig,
		const FGameplayTag& InSelectedProjectileConfigTag,
		FActionProjectileConfig& OutProjectileConfig,
		EActionResolvedProjectileConfigSource* OutResolvedConfigSource = nullptr) const;

	/** 直接按配置标签解析一条可切换发射物配置。 */
	bool ResolveSwitchableProjectileConfigByTag(
		const FGameplayTag& InProjectileConfigTag,
		FActionProjectileConfig& OutProjectileConfig) const;

	/** 读取当前武器需要挂接的动画层。 */
	TSubclassOf<UActionHeroLinkedAnimLayer> GetLinkedAnimLayerClass() const;

	/** 按模板名解析命中窗口运行时配置。 */
	bool TryResolveHitWindowConfigByName(
		FName InTemplateName,
		FName InRuntimeWindowName,
		FActionHitWindowRuntimeConfig& OutRuntimeConfig) const;

	// 攻击链运行时解析入口：
	// 攻击组件与攻击 GA 会通过这些接口把请求标签一路解析到分支、输入阶段和执行配置。
	/** 按攻击请求标签解析最终攻击分支标签。 */
	FGameplayTag ResolveAttackBranchTag(const FGameplayTag& InRequestTag) const;

	/** 解析指定攻击请求真正应在哪个输入阶段出招。 */
	EActionInputEvent ResolveAttackTriggerInputEvent(const FGameplayTag& InRequestTag) const;

	/** 按攻击请求标签与当前连段索引直接解析完整攻击执行计划。 */
	bool TryResolveAttackExecutionConfigByRequestTag(
		const FGameplayTag& InRequestTag,
		int32 InComboIndex,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	/** 按攻击请求标签读取正式命中配置。 */
	bool TryResolveAttackHitConfigByRequestTag(
		const FGameplayTag& InRequestTag,
		FActionWeaponHitConfig& OutHitConfig) const;

	/** 按攻击分支标签与当前连段索引解析完整攻击执行计划。 */
	bool TryResolveAttackExecutionConfig(
		const FGameplayTag& InBranchTag,
		int32 InComboIndex,
		FActionResolvedAttackExecutionConfig& OutResolvedConfig) const;

	/** 读取指定攻击分支可用的蒙太奇数量。 */
	int32 GetAttackMontageCountForBranch(const FGameplayTag& InBranchTag) const;

	/** 读取正式 Combat 表现过渡蒙太奇：Idle -> Combat 读取 Enter，Combat -> Idle 读取 Exit。 */
	UAnimMontage* GetCombatModeTransitionAnimMontage(EHeroCombatMode InCombatMode) const;
	int32 GetCombatModeTransitionReactGuardThreshold(EHeroCombatMode InCombatMode) const;

	/** 读取闪避蒙太奇。 */
	UAnimMontage* GetDodgeAnimMontage(bool bHasMoveInput) const;
	int32 GetDodgeReactGuardThreshold(bool bHasMoveInput) const;

	/** 读取防御蒙太奇。 */
	UAnimMontage* GetDefenseAnimMontage() const;
	int32 GetDefenseReactGuardThreshold() const;

	/** 读取普通格挡受击蒙太奇。 */
	UAnimMontage* GetBlockedHitAnimMontage() const;

	/** 读取普通切武蒙太奇。 */
	UAnimMontage* GetNormalWeaponSwitchMontage() const;
	int32 GetNormalWeaponSwitchReactGuardThreshold() const;
	const FActionWeaponHitConfig& GetNormalWeaponSwitchHitConfig() const;

	/** 读取执行者侧处决蒙太奇。 */
	UAnimMontage* GetExecutionMontage() const;

	/** 读取特殊切武蒙太奇。 */
	UAnimMontage* GetSpecialWeaponSwitchMontage() const;
	int32 GetSpecialWeaponSwitchReactGuardThreshold() const;
	const FActionWeaponHitConfig& GetSpecialWeaponSwitchHitConfig() const;

	// 编辑器攻击链整理工具：
	// 这些接口用于新建或整理武器资产时快速补齐正式五请求攻击条目结构。
	/** 重建推荐的正式攻击条目配置。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Attack", meta = (DisplayName = "重建攻击条目默认值", ToolTip = "按当前五请求基线重建 AttackEntryConfigs。适合新建武器资产后的正式攻击入口初始化。"))
	void ResetAttackEntryConfigsToDefault();

	/** 一次性重建整套攻击配置。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Attack", meta = (DisplayName = "重建整套攻击配置", ToolTip = "一次性重建正式 AttackEntryConfigs。适合先搭好当前武器的正式攻击条目结构。"))
	void ResetAllAttackConfigsToDefault();

	// 编辑器推荐预设与结构修复：
	// 推荐预设偏“整体重置”，结构修复偏“保守补齐”，两者面向不同的资产整理场景。
	/** 套用推荐的空手预设。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "套用空手推荐预设", ToolTip = "切换到空手类别推荐模板，会重写 WeaponCategory、WeaponTag、WeaponActorClass，并清空现有动画引用后重建攻击链。仅适合需要整体重置时使用。"))
	void ApplyRecommendedUnarmedPreset();

	/** 套用推荐的纯近战预设。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "套用近战推荐预设", ToolTip = "切换到纯近战推荐模板，会重写类别、Tag、武器类，并清空现有动画引用后重建攻击链。"))
	void ApplyRecommendedMeleePreset();

	/** 套用推荐的纯远程预设。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "套用远程推荐预设", ToolTip = "切换到纯远程推荐模板，会重写类别、Tag、武器类，并清空现有动画引用后重建攻击链。"))
	void ApplyRecommendedRangedPreset();

	/** 套用推荐的近远程混合预设。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "套用混合推荐预设", ToolTip = "切换到近远程混合推荐模板，会重写类别、Tag、武器类，并清空现有动画引用后重建攻击链。"))
	void ApplyRecommendedHybridPreset();

	/** 按当前 WeaponCategory 一键套用推荐预设。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "按当前类别套用推荐预设", ToolTip = "根据当前 WeaponCategory 套用对应推荐预设。注意：这属于重置型操作，会清空现有动画引用。"))
	void ApplyRecommendedPresetByCurrentCategory();

	/** 基于当前类别补齐武器定义的最小正式结构，避免遗漏基础攻击链和武器类配置。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "按当前类别修复定义结构", ToolTip = "保守补齐当前类别下的最小正式结构：整理攻击条目、补缺失的 Tag 和 WeaponActorClass，但不会清空你已经配好的动画。推荐优先用这个，而不是直接套重置型预设。"))
	void RepairDefinitionShellForCurrentCategory();

	/** 按空手类别补齐武器定义的最小正式结构，不清空现有动画资源。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "修复空手定义结构", ToolTip = "把当前资产按空手类别的正式结构规则补齐，不清空现有动画资源。适合整理空手武器定义。"))
	void RepairUnarmedDefinitionShell();

	/** 按纯近战类别补齐武器定义的最小正式结构，不清空现有动画资源。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "修复近战定义结构", ToolTip = "把当前资产按纯近战类别的正式结构规则补齐，不清空现有动画资源。适合整理近战武器定义。"))
	void RepairMeleeDefinitionShell();

	/** 按纯远程类别补齐武器定义的最小正式结构，不清空现有动画资源。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "修复远程定义结构", ToolTip = "把当前资产按纯远程类别的正式结构规则补齐，不清空现有动画资源。适合整理远程武器定义。"))
	void RepairRangedDefinitionShell();

	/** 按近远程混合类别补齐武器定义的最小正式结构，不清空现有动画资源。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Preset", meta = (DisplayName = "修复混合定义结构", ToolTip = "把当前资产按近远程混合类别的正式结构规则补齐，不清空现有动画资源。适合整理混合武器定义。"))
	void RepairHybridDefinitionShell();

	// 编辑器校验入口：
	// 配完武器资产后，优先用这组接口检查类别、Tag、攻击链结构和关键动画资源是否齐全。
	/** 构建当前武器资产的编辑器校验报告。 */
	UFUNCTION(BlueprintPure, Category = "WeaponDefinition|Validation", meta = (DisplayName = "构建武器资产校验报告", ToolTip = "返回当前 WeaponDefinition 的详细校验结果字符串。适合在编辑器工具蓝图或自定义面板中展示。"))
	FString BuildEditorValidationReport() const;

	/** 在日志中输出当前武器资产的编辑器校验报告。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "WeaponDefinition|Validation", meta = (DisplayName = "校验并输出武器资产报告", ToolTip = "检查类别、Tag、武器类、攻击请求链、攻击分支链和关键动画资源，并把结果输出到日志。推荐每次改完攻击链后点一次。"))
	void ValidateAndLogWeaponDefinition() const;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

public:
	// 核心数据入口：
	// 这几项就是装备链与攻击链最终真正依赖的武器资产数据本体。
	/** 武器唯一 GameplayTag。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition", meta = (ToolTip = "武器唯一标识标签。根标签必须与 WeaponCategory 匹配，例如近战武器应使用 Player.Weapon.Melee.*。"))
	FGameplayTag WeaponTag;

	/** 武器类别。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition", meta = (ToolTip = "武器所属类别。它决定该武器能进入哪个固定武器槽，也决定推荐预设与校验规则。"))
	EHeroWeaponCategory WeaponCategory = EHeroWeaponCategory::PureMelee;

	/** 武器小类别标签。它决定当前武器的正式小类别语义和角色挂点映射。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition", meta = (ToolTip = "武器小类别标签。当前正式使用 Player.Weapon.<Category>.<Subtype> 这一层，运行时挂点映射和层级校验都以它为准。"))
	FGameplayTag WeaponSubtypeTag;

	/** 武器属性体系类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Semantic", meta = (ToolTip = "武器的属性体系类型。凡武器允许额外附加效果；灵武器可配置专属能力；元素武器固定造成元素伤害。"))
	EActionWeaponPropertyType WeaponPropertyType = EActionWeaponPropertyType::Mundane;

	/** 武器默认伤害大类。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Semantic", meta = (ToolTip = "武器默认造成的伤害大类。当前正式规则要求它与 WeaponPropertyType 保持一致。"))
	EActionDamageType DamageType = EActionDamageType::Physical;

	/** 武器默认元素伤害子类型。只有元素武器时才应配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Semantic", meta = (ToolTip = "元素武器的具体元素子类型标签，例如 Damage.Element.Fire。非元素武器不应配置。"))
	FGameplayTag DamageElementTypeTag;

	/** 运行时真正生成的武器 Actor 类型。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition", meta = (ToolTip = "运行时要实例化的武器 Actor 类。空手武器通常留空，其余类别一般都需要配置。"))
	TSoftClassPtr<AHeroWeaponBase> WeaponActorClass;

	/** 武器动画配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Animation", meta = (ToolTip = "武器所有常规动画资源入口。包括动画层、攻击请求路由、攻击分支蒙太奇、闪避、防御和特殊切武动画。"))
	FActionWeaponAnimationConfig AnimationConfig;

	/** 处决专用配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Execution", meta = (ToolTip = "正式处决配置。统一收口执行者处决蒙太奇、目标转向等待时长和处决专用 HitConfig。"))
	FActionExecutionConfig ExecutionConfig;

	/** 当前武器默认使用的发射物配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Projectile", meta = (ToolTip = "远程或混合武器的默认发射物配置。当前阶段先作为统一数据入口与资源预热入口，不直接展开完整发射物系统。"))
	FActionProjectileConfig DefaultProjectileConfig;

	/** 当前武器是否允许在默认发射物之外切换到其它发射物配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Projectile", meta = (ToolTip = "只有法杖类远程武器才允许开启。开启后当前武器会读取 SwitchableProjectileConfigs 作为可切换入口。"))
	bool bSupportsProjectileSwitching = false;

	/** 法杖类远程武器可切换的发射物配置集合。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Projectile", meta = (ToolTip = "只有法杖类远程武器才应配置。每条配置都需要唯一的 ProjectileConfigTag。"))
	TArray<FActionSwitchableProjectileConfigEntry> SwitchableProjectileConfigs;

	/**
	 * 武器装入固定武器槽后为角色提供的属性增量。
	 * 当前第一阶段先把这组数据缓存到固定武器槽运行时状态中，
	 * 后续再由伤害解析、角色面板与 UI 汇总统一消费。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Attributes", meta = (ToolTip = "武器装入固定武器槽后提供的属性增量。当前版本先缓存到槽位运行时状态，不直接回写角色 AttributeSet。"))
	FActionWeaponAttributeCacheData EquippedAttributeBonuses;

	/** Spirit Ability 一体化正式条目集合。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponDefinition|Ability", meta = (ToolTip = "Spirit Ability 一体化正式条目集合。只有灵武器语义才应配置；每条条目同时承接输入、授予 Ability，与可选的 SpiritSkill 多段静态模板。"))
	TArray<FActionSpiritAbilityEntryConfig> SpiritAbilityEntryConfigs;

private:
#if WITH_EDITOR
	bool CanEditWeaponActorClass() const;
	bool CanEditDamageElementTypeTag() const;
	bool CanEditSpiritAbilityFields() const;
	bool CanEditDefaultProjectileConfig() const;
	bool CanEditProjectileSwitching() const;
	bool CanEditSwitchableProjectileConfigs() const;
#endif

	// 内部共用实现：
	// 外部统一走公开编辑器工具接口，不直接接触这两个内部重建 / 修复实现。
	/** 套用预设时共用的内部实现。 */
	void ApplyRecommendedPreset(
		EHeroWeaponCategory InWeaponCategory,
		const FGameplayTag& InWeaponSubtypeTag,
		const FGameplayTag& InWeaponTag);

	/** 补齐当前类别下的武器定义最小正式结构，可选择是否同步切换类别。 */
	void RepairDefinitionShell(EHeroWeaponCategory InWeaponCategory, bool bOverrideWeaponCategory);

	/** 把正式攻击条目整理回当前固定五请求条目结构。 */
	void NormalizeAttackConfigArrays();

	/** 把 Spirit 条目的编辑器暴露标记同步到当前 EntryKind，避免面板长期显示错类别字段。 */
	void SyncSpiritAbilityEditorExposureFlags();
};
