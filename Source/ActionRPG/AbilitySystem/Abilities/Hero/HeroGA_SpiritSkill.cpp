// 文件说明：实现灵武器专属主动技能壳 Ability。

#include "AbilitySystem/Abilities/Hero/HeroGA_SpiritSkill.h"

#include "ActionGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace HeroSpiritSkillAbility
{
	static FString GetEntryKindText(const EActionSpiritAbilityEntryKind InEntryKind)
	{
		switch (InEntryKind)
		{
		case EActionSpiritAbilityEntryKind::SpiritSkillSelfEnhance:
			return TEXT("自强化");
		case EActionSpiritAbilityEntryKind::SpiritSkillOffensive:
			return TEXT("主动攻击");
		default:
			return TEXT("授予能力");
		}
	}

	static FString DescribeSameActivationAdvanceUnsupportedReason(
		const FActionSpiritAbilityEntryConfig& SpiritAbilityEntryConfig,
		const int32 ActiveSkillClipIndex,
		const int32 ActiveSkillClipCount)
	{
		if (!SpiritAbilityEntryConfig.SpiritSkillConfig.bUseComboIndex)
		{
			return TEXT("当前技能未启用 bUseComboIndex");
		}

		if (!SpiritAbilityEntryConfig.SpiritSkillConfig.bAdvanceComboIndexOnPlay)
		{
			return TEXT("当前技能未启用 bAdvanceComboIndexOnPlay");
		}

		if (ActiveSkillClipCount <= 1)
		{
			return TEXT("当前技能只有 1 段");
		}

		if (ActiveSkillClipIndex < 0)
		{
			return TEXT("当前技能段索引无效");
		}

		if (ActiveSkillClipIndex + 1 >= ActiveSkillClipCount)
		{
			return TEXT("当前已经是最后一段");
		}

		return TEXT("当前段不支持同次续段");
	}

	static FString DescribeBufferedChainInputReason(
		const UHeroCombatComponent* HeroCombatComponent,
		const FGameplayTag& SpiritInputTag)
	{
		if (!HeroCombatComponent)
		{
			return TEXT("未找到 HeroCombatComponent");
		}

		if (!HeroCombatComponent->IsAbilityChainWindowActive())
		{
			return TEXT("AbilityChainWindow 未开启");
		}

		if (!HeroCombatComponent->CanAcceptAbilityChainInput(SpiritInputTag))
		{
			return FString::Printf(
				TEXT("AbilityChainWindow 已开启，但未接受输入 %s"),
				*SpiritInputTag.ToString());
		}

		return TEXT("AbilityChainWindow 已接受当前输入");
	}
}

UHeroGA_SpiritSkill::UHeroGA_SpiritSkill()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_SpiritSkill);
	ActivationOwnedTags.AddTag(ActionGameplayTags::State_Ability_SpiritSkill_Active);
}

void UHeroGA_SpiritSkill::ResolveAbilityCooldownConfig(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer& OutCooldownTags,
	float& OutCooldownDuration) const
{
	Super::ResolveAbilityCooldownConfig(Handle, ActorInfo, OutCooldownTags, OutCooldownDuration);
	OutCooldownTags.Reset();

	const FGameplayTag SpiritCooldownTag = ResolveSpiritSkillCooldownTagForContext(Handle, ActorInfo);
	if (SpiritCooldownTag.IsValid())
	{
		OutCooldownTags.AddTag(SpiritCooldownTag);
	}
}

bool UHeroGA_SpiritSkill::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// Spirit 当前在关系裁决前只保留共享硬门禁和 Spirit 自己的业务前置。
	// 它不再依赖 Attack 的旧输入层 interrupt whitelist 先放行，是否能接管活跃主动 GA 统一留给 ASC。
	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo();
	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCharacter || !HeroCombatComponent)
	{
		OutFailureReason = TEXT("hero character or combat component is invalid");
		return false;
	}

	const FGameplayTag CurrentSpiritInputTag = GetCurrentSpiritSkillInputTag();
	if (!ActionGameplayTags::IsSpiritSkillInputTag(CurrentSpiritInputTag))
	{
		OutFailureReason = TEXT("current SpiritAbilitySpec has no valid SpiritSkill1~4 input tag");
		return false;
	}

	if (const UHeroLoadoutStateComponent* LoadoutStateComponent = HeroCharacter->GetHeroLoadoutStateComponent())
	{
		if (!LoadoutStateComponent->IsWeaponLoadoutStartupReady())
		{
			OutFailureReason = FString::Printf(
				TEXT("SpiritSkill stable precheck blocked: weapon loadout startup is not ready. Pending=%d Total=%d Failure=%s"),
				LoadoutStateComponent->GetWeaponLoadoutStartupPendingSlotCount(),
				LoadoutStateComponent->GetWeaponLoadoutStartupTotalSlotCount(),
				LoadoutStateComponent->GetWeaponLoadoutStartupFailureReason().IsEmpty()
					? TEXT("none")
					: *LoadoutStateComponent->GetWeaponLoadoutStartupFailureReason());
			return false;
		}
	}
	else
	{
		OutFailureReason = TEXT("hero loadout state component is invalid");
		return false;
	}

	if (const UActionCombatReactComponent* CombatReactComponent = HeroCharacter->GetActionCombatReactComponent())
	{
		// CombatReact 相关限制仍然属于共享硬门禁：
		// 这里拦的是“Spirit 当前宿主状态不允许起手”，不是主动 GA 默认优先级关系本身。
		if (CombatReactComponent->IsPrimaryReactPhaseActive())
		{
			OutFailureReason = FString::Printf(
				TEXT("SpiritSkill stable precheck blocked: combat react primary phase is active. %s"),
				*CombatReactComponent->DescribeCurrentCombatReactState());
			return false;
		}

		if (CombatReactComponent->IsRecoveryPhaseActive()
			&& !CombatReactComponent->CanActivateAbilityDuringRecoveryCancelWindow(CurrentSpiritInputTag))
		{
			OutFailureReason = FString::Printf(
				TEXT("SpiritSkill stable precheck blocked: combat react recovery phase has not formally allowed input %s. %s"),
				*CurrentSpiritInputTag.ToString(),
				*CombatReactComponent->DescribeCurrentCombatReactState());
			return false;
		}
	}

	const UCharacterMovementComponent* MovementComponent = HeroCharacter->GetCharacterMovement();
	if (MovementComponent && MovementComponent->IsFalling())
	{
		OutFailureReason = TEXT("SpiritSkill stable precheck blocked: character is in falling state");
		return false;
	}

	if (const UHeroWeaponSwitchComponent* HeroWeaponSwitchComponent = HeroCharacter->GetHeroWeaponSwitchComponent())
	{
		if (HeroWeaponSwitchComponent->IsWeaponSwitchTransactionInProgress())
		{
			OutFailureReason = TEXT("SpiritSkill stable precheck blocked: weapon switch transaction is still active");
			return false;
		}

		if (HeroWeaponSwitchComponent->IsSpecialWeaponSwitchPresentationActive())
		{
			OutFailureReason = TEXT("SpiritSkill stable precheck blocked: special weapon switch presentation is active");
			return false;
		}
	}
	else
	{
		OutFailureReason = TEXT("hero weapon switch component is invalid");
		return false;
	}

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = nullptr;
	FActionSpiritAbilityEntryConfig ResolvedEntryConfig;
	return ResolveCurrentSpiritSkillConfig(CurrentWeaponDefinition, ResolvedEntryConfig, OutFailureReason);
}

FGameplayTag UHeroGA_SpiritSkill::GetPrimaryInputTagForCombatReact() const
{
	return GetCurrentSpiritSkillInputTag();
}

void UHeroGA_SpiritSkill::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bAbilityFinished = false;
	ActiveSpiritSkillDebugName.Reset();
	ActiveSpiritInputTag = FGameplayTag();
	bAppliedSpiritOffensiveRuntimeState = false;
	ActiveSpiritAbilityEntryConfig = FActionSpiritAbilityEntryConfig();
	ActiveSpiritWeaponDefinition = nullptr;
	ActiveSkillClipIndex = INDEX_NONE;
	ActiveSkillClipCount = 0;
	bPendingAdvanceToNextClip = false;
	bHandledCurrentClipFinish = false;
	bSpiritCooldownCommitted = false;
	bImmediateSpiritClipTransitionInProgress = false;

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = nullptr;
	FActionSpiritAbilityEntryConfig ResolvedEntryConfig;
	FString FailureReason;
	if (!ResolveCurrentSpiritSkillConfig(CurrentWeaponDefinition, ResolvedEntryConfig, FailureReason))
	{
		Debug::Print(
			FString::Printf(TEXT("[GA][SpiritSkill] 激活失败：%s"), *FailureReason),
			FColor::Red,
			2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	ActiveSpiritInputTag = GetCurrentSpiritSkillInputTag();
	const bool bContinuingExistingSpiritChain =
		HeroCombatComponent
		&& HeroCombatComponent->HasCommittedSpiritSkillChainCost(ActiveSpiritInputTag);

	// 组件侧已经记住“这条 Spirit 连段已经正式提交过一次成本”时，
	// 后续续段只复用那条持久链，不在每次重新按下同一 Spirit 输入时重复扣成本。
	// 首段起手资格已经前移到 ASC 的两阶段正式激活链，这里只保留真正的成本提交。
	if (bContinuingExistingSpiritChain
		&& HeroCombatComponent)
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][SpiritSkill] 持久资格：%s"),
				*HeroCombatComponent->DescribeSpiritSkillComboRuntimeState(ActiveSpiritInputTag)),
			FColor::Yellow,
			1.5f);
	}

	if (!bContinuingExistingSpiritChain
		&& !CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		Debug::Print(TEXT("[GA][SpiritSkill] 激活失败：CommitAbilityCost 未通过"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveSpiritAbilityEntryConfig = ResolvedEntryConfig;
	ActiveSpiritWeaponDefinition = CurrentWeaponDefinition;
	ActiveSkillClipCount = ResolvedEntryConfig.GetSkillClipCount();
	if (ActiveSkillClipCount <= 0)
	{
		Debug::Print(TEXT("[GA][SpiritSkill] 激活失败：未解析到有效 SkillClips"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FString SkillDebugName = ResolvedEntryConfig.ResolveDebugName();
	const FActionCombatModifierEffectSpec CombatModifierEffect =
		ResolvedEntryConfig.SpiritSkillConfig.CombatModifierEffect;
	const bool bIsSelfEnhanceSkill = ResolvedEntryConfig.IsSelfEnhanceSpiritSkill();
	const bool bIsOffensiveSkill = ResolvedEntryConfig.IsOffensiveSpiritSkill();
	const int32 InitialClipIndex =
		HeroCombatComponent
			? HeroCombatComponent->ResolveSpiritSkillClipIndexToPlay(
				ActiveSpiritInputTag,
				ResolvedEntryConfig.SpiritSkillConfig,
				ActiveSkillClipCount)
			: 0;

	ActiveSpiritSkillDebugName = SkillDebugName;
	Debug::Print(
		FString::Printf(
			TEXT("[GA][SpiritSkill] 开始：武器=%s 技能=%s 模式=%s"),
			*CurrentWeaponDefinition->WeaponTag.ToString(),
			*ActiveSpiritSkillDebugName,
			*HeroSpiritSkillAbility::GetEntryKindText(ResolvedEntryConfig.EntryKind)),
		FColor::Cyan,
		2.0f);

	if (bContinuingExistingSpiritChain
		&& ResolvedEntryConfig.SpiritSkillConfig.bUseComboIndex
		&& InitialClipIndex > 0)
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][SpiritSkill] 待命续段恢复：技能=%s 恢复段=%d/%d"),
				*ActiveSpiritSkillDebugName,
				InitialClipIndex + 1,
				ActiveSkillClipCount),
			FColor::Yellow,
			1.5f);
	}

	// CombatModifierEffect 现在只服务 SelfEnhance 正式语义。
	// Offensive 即使资产残留旧值，也不再在运行时消费这层修正，
	// 避免把“主动攻击”和“临时强化”混成同一条正式能力链。
	if (bIsSelfEnhanceSkill
		&& CombatModifierEffect.IsValidSpec()
		&& !ApplyHeroCombatModifierEffect(CombatModifierEffect))
	{
		Debug::Print(TEXT("[GA][SpiritSkill] 激活失败：自身 CombatModifierEffect 施加失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (bIsOffensiveSkill)
	{
		if (HeroCombatComponent)
		{
			HeroCombatComponent->BeginActiveDamageContext(
				GetCurrentAbilitySpec() ? GetCurrentAbilitySpec()->Level : 1,
				ActionGameplayTags::Player_Ability_SpiritSkill);
		}
	}

	if (!StartSpiritSkillClip(InitialClipIndex))
	{
		Debug::Print(TEXT("[GA][SpiritSkill] 激活失败：首段 SkillClip 启动失败"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 这里只控制是否重置 Attack 的全局 ComboIndex。
	// Spirit 自己的持久段索引已经在组件侧建立，不会因为这个开关被顺手清零。
	if (HeroCombatComponent
		&& ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.bResetAttackComboIndexOnActivate)
	{
		HeroCombatComponent->ResetComboIndex();
	}
}

void UHeroGA_SpiritSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bAbilityFinished)
	{
		bAbilityFinished = true;

		Debug::Print(
			FString::Printf(
				TEXT("[GA][SpiritSkill] 结束：技能=%s 已取消=%d"),
				ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
				bWasCancelled ? 1 : 0),
			bWasCancelled ? FColor::Red : FColor::Green,
			2.0f);
	}

	ClearBufferedSpiritSkillInputIfAny();
	CloseSpiritChainWindowIfNeeded();

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		HeroCombatComponent->ClearActiveDamageContext();

		if (ActiveSkillClipIndex != INDEX_NONE)
		{
			if (const FActionSpiritSkillClipConfig* ActiveSkillClip =
				ActiveSpiritAbilityEntryConfig.ResolveSkillClip(ActiveSkillClipIndex))
			{
				HeroCombatComponent->ClearRunningAnimationReactGuardContextIfMatches(
					ActiveSkillClip->GetSkillMontage(),
					EActionRunningAnimationSemantic::NonReact);
				HeroCombatComponent->ClearRunningAnimMontageReferenceIfMatches(
					ActiveSkillClip->GetSkillMontage());
			}
		}
	}

	if (bAppliedSpiritOffensiveRuntimeState)
	{
		if (UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo())
		{
			HeroAttackComponent->ClearCurrentAttackHitConfig();
			HeroAttackComponent->ClearCurrentAttackProjectileSpawnConfig();
		}

		bAppliedSpiritOffensiveRuntimeState = false;
	}

	ActiveSpiritAbilityEntryConfig = FActionSpiritAbilityEntryConfig();
	ActiveSpiritWeaponDefinition = nullptr;
	ActiveSpiritInputTag = FGameplayTag();
	ActiveSkillClipIndex = INDEX_NONE;
	ActiveSkillClipCount = 0;
	bPendingAdvanceToNextClip = false;
	bHandledCurrentClipFinish = false;
	bSpiritCooldownCommitted = false;
	bImmediateSpiritClipTransitionInProgress = false;
	ActiveSpiritSkillDebugName.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UHeroGA_SpiritSkill::OnHeroMontageCompleted(FName MontageContext)
{
	(void)MontageContext;

	if (bImmediateSpiritClipTransitionInProgress)
	{
		return;
	}

	HandleSpiritSkillClipFinished(false);
}

void UHeroGA_SpiritSkill::OnHeroMontageBlendOut(FName MontageContext)
{
	(void)MontageContext;

	if (bImmediateSpiritClipTransitionInProgress)
	{
		return;
	}

	HandleSpiritSkillClipFinished(false);
}

void UHeroGA_SpiritSkill::OnHeroMontageInterrupted(FName MontageContext)
{
	(void)MontageContext;

	if (bImmediateSpiritClipTransitionInProgress)
	{
		Debug::Print(
			TEXT("[GA][SpiritSkill] 内部切段触发的 Interrupted/Cancelled 已被消费，不视为整条链取消。"),
			FColor::Yellow,
			1.5f);
		return;
	}

	HandleSpiritSkillClipFinished(true);
}

void UHeroGA_SpiritSkill::OnHeroMontageCancelled(FName MontageContext)
{
	(void)MontageContext;

	if (bImmediateSpiritClipTransitionInProgress)
	{
		Debug::Print(
			TEXT("[GA][SpiritSkill] 内部切段触发的 Interrupted/Cancelled 已被消费，不视为整条链取消。"),
			FColor::Yellow,
			1.5f);
		return;
	}

	HandleSpiritSkillClipFinished(true);
}

bool UHeroGA_SpiritSkill::ResolveCurrentSpiritSkillConfig(
	const UDataAsset_WeaponDefinition*& OutWeaponDefinition,
	FActionSpiritAbilityEntryConfig& OutEntryConfig,
	FString& OutFailureReason)
{
	OutWeaponDefinition = nullptr;
	OutEntryConfig = FActionSpiritAbilityEntryConfig();
	OutFailureReason.Reset();

	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	const UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	const UHeroEquipmentComponent* HeroEquipmentComponent =
		HeroCombatComponent ? HeroCombatComponent->GetOwningHeroEquipmentComponent() : nullptr;
	const UHeroLoadoutRuntimeComponent* HeroLoadoutRuntimeComponent = GetHeroLoadoutRuntimeComponentFromActorInfo();
	if (!HeroEquipmentComponent || !HeroLoadoutRuntimeComponent)
	{
		OutFailureReason = TEXT("未找到装备组件或 runtime 组件");
		return false;
	}

	const EHeroWeaponLoadoutSlot CurrentLoadoutSlot = HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot();
	if (CurrentLoadoutSlot == EHeroWeaponLoadoutSlot::Invalid)
	{
		OutFailureReason = TEXT("当前没有有效的已装备武器槽");
		return false;
	}

	OutWeaponDefinition = HeroLoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(CurrentLoadoutSlot);
	if (!OutWeaponDefinition)
	{
		OutFailureReason = TEXT("当前武器定义无效");
		return false;
	}

	if (!OutWeaponDefinition->SupportsSpiritWeaponAbilities())
	{
		OutFailureReason = TEXT("当前装备武器不是灵武器");
		return false;
	}

	const FGameplayTag CurrentSpiritInputTag = GetCurrentSpiritSkillInputTag();
	if (!ActionGameplayTags::IsSpiritSkillInputTag(CurrentSpiritInputTag))
	{
		OutFailureReason = TEXT("当前 SpiritAbilitySpec 未绑定有效的 SpiritSkill1~4 输入标签");
		return false;
	}

	if (!OutWeaponDefinition->TryResolveSpiritAbilityEntryConfigByInputTag(CurrentSpiritInputTag, OutEntryConfig))
	{
		OutFailureReason = FString::Printf(
			TEXT("当前灵武器没有为输入 %s 配置 SpiritAbilityEntryConfigs 条目"),
			*CurrentSpiritInputTag.ToString());
		return false;
	}

	if (!OutEntryConfig.IsSpiritSkillEntry())
	{
		OutFailureReason = TEXT("当前输入解析到的 SpiritAbility 条目不是 Spirit 主动技能条目");
		return false;
	}

	FString SkillValidationFailure;
	if (!OutEntryConfig.IsValidConfig(SkillValidationFailure))
	{
		OutFailureReason = SkillValidationFailure;
		return false;
	}

	return true;
}

FGameplayTag UHeroGA_SpiritSkill::GetCurrentSpiritSkillInputTag() const
{
	if (const FGameplayAbilitySpec* CurrentSpec = GetCurrentAbilitySpec())
	{
		for (const FGameplayTag& DynamicTag : CurrentSpec->DynamicAbilityTags)
		{
			if (ActionGameplayTags::IsSpiritSkillInputTag(DynamicTag))
			{
				return DynamicTag;
			}
		}
	}

	return FGameplayTag();
}

FGameplayTag UHeroGA_SpiritSkill::ResolveSpiritSkillCooldownTagForContext(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (const UAbilitySystemComponent* AbilitySystemComponent =
			ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
		{
			for (const FGameplayTag& DynamicTag : AbilitySpec->DynamicAbilityTags)
			{
				if (ActionGameplayTags::IsSpiritSkillInputTag(DynamicTag))
				{
					return ActionGameplayTags::ResolveSpiritSkillCooldownTag(DynamicTag);
				}
			}
		}
	}

	if (ActiveSpiritInputTag.IsValid())
	{
		return ActionGameplayTags::ResolveSpiritSkillCooldownTag(ActiveSpiritInputTag);
	}

	return ActionGameplayTags::ResolveSpiritSkillCooldownTag(GetCurrentSpiritSkillInputTag());
}

void UHeroGA_SpiritSkill::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (bAbilityFinished || ActiveSkillClipIndex == INDEX_NONE)
	{
		return;
	}

	if (!CanAdvanceSpiritClipWithinCurrentActivation())
	{
		Debug::Print(
			FString::Printf(
				TEXT("[GA][SpiritSkill] 当前段不支持同次续段：技能=%s 当前段=%d/%d 原因=%s"),
				ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
				ActiveSkillClipIndex == INDEX_NONE ? 0 : ActiveSkillClipIndex + 1,
				ActiveSkillClipCount,
				*HeroSpiritSkillAbility::DescribeSameActivationAdvanceUnsupportedReason(
					ActiveSpiritAbilityEntryConfig,
					ActiveSkillClipIndex,
					ActiveSkillClipCount)),
			FColor::Yellow,
			1.5f);
		return;
	}

	const FGameplayTag CurrentSpiritInputTag =
		ActiveSpiritInputTag.IsValid() ? ActiveSpiritInputTag : GetCurrentSpiritSkillInputTag();
	if (!ActionGameplayTags::IsSpiritSkillInputTag(CurrentSpiritInputTag))
	{
		return;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		return;
	}

	if (HeroCombatComponent->CanAcceptAbilityChainInput(CurrentSpiritInputTag))
	{
		if (TryStartImmediateNextSpiritClip())
		{
			return;
		}

		return;
	}

	if (!bPendingAdvanceToNextClip)
	{
		HeroCombatComponent->QueueBufferedInput(CurrentSpiritInputTag, EActionInputEvent::Pressed);
		Debug::Print(
			FString::Printf(
				TEXT("[GA][SpiritSkill] 接段预输入已缓冲：技能=%s 当前段=%d 原因=%s"),
				ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
				ActiveSkillClipIndex + 1,
				*HeroSpiritSkillAbility::DescribeBufferedChainInputReason(
					HeroCombatComponent,
					CurrentSpiritInputTag)),
			FColor::Yellow,
			1.5f);
	}
}

bool UHeroGA_SpiritSkill::CanAdvanceSpiritClipWithinCurrentActivation() const
{
	return ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.bUseComboIndex
		&& ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.bAdvanceComboIndexOnPlay
		&& ActiveSkillClipCount > 1
		&& ActiveSkillClipIndex >= 0
		&& ActiveSkillClipIndex + 1 < ActiveSkillClipCount;
}

void UHeroGA_SpiritSkill::CloseSpiritChainWindowIfNeeded()
{
	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		HeroCombatComponent->CloseAbilityChainWindow();
	}
}

bool UHeroGA_SpiritSkill::TryStartImmediateNextSpiritClip()
{
	if (bImmediateSpiritClipTransitionInProgress || !CanAdvanceSpiritClipWithinCurrentActivation())
	{
		return false;
	}

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	if (!HeroCombatComponent)
	{
		return false;
	}

	const int32 NextClipIndex = ActiveSkillClipIndex + 1;
	const UAnimMontage* CurrentSpiritMontage = HeroCombatComponent->GetCurrentRunningAnimMontage();

	Debug::Print(
		FString::Printf(
			TEXT("[GA][SpiritSkill] 链窗内立即切段已确认：技能=%s 当前段=%d 下一段=%d"),
			ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
			ActiveSkillClipIndex + 1,
			NextClipIndex + 1),
		FColor::Yellow,
		1.5f);

	bImmediateSpiritClipTransitionInProgress = true;
	bHandledCurrentClipFinish = true;
	bPendingAdvanceToNextClip = false;

	// 内部立即切段会主动停掉旧蒙太奇，旧通知的 NotifyEnd 之后会因为运行中蒙太奇已切走而被忽略。
	// 这里先显式关掉上一段链窗，避免它残留到新段起手瞬间。
	CloseSpiritChainWindowIfNeeded();
	StopActiveHeroMontageTaskAndCurrentMontage(true);

	if (CurrentSpiritMontage)
	{
		HeroCombatComponent->ClearRunningAnimationReactGuardContextIfMatches(CurrentSpiritMontage);
		HeroCombatComponent->ClearRunningAnimMontageReferenceIfMatches(CurrentSpiritMontage);
	}

	Debug::Print(
		TEXT("[GA][SpiritSkill] 内部切段触发的 Interrupted/Cancelled 已被消费，不视为整条链取消。"),
		FColor::Yellow,
		1.5f);

	const bool bStartedNextClip = StartSpiritSkillClip(NextClipIndex);
	bImmediateSpiritClipTransitionInProgress = false;
	if (bStartedNextClip)
	{
		return true;
	}

	HeroCombatComponent->ResetSpiritSkillComboState(ActiveSpiritInputTag);
	ClearBufferedSpiritSkillInputIfAny();
	FinishSpiritSkillAbility(true);
	return false;
}

bool UHeroGA_SpiritSkill::StartSpiritSkillClip(const int32 ClipIndex)
{
	const FActionSpiritSkillClipConfig* SkillClip = ActiveSpiritAbilityEntryConfig.ResolveSkillClip(ClipIndex);
	if (!SkillClip)
	{
		Debug::Print(
			FString::Printf(TEXT("[GA][SpiritSkill] 启动技能段失败：ClipIndex=%d 无效"), ClipIndex),
			FColor::Red,
			2.0f);
		return false;
	}

	UAnimMontage* SkillMontage = SkillClip->GetSkillMontage();
	if (!SkillMontage)
	{
		Debug::Print(
			FString::Printf(TEXT("[GA][SpiritSkill] 启动技能段失败：ClipIndex=%d 未加载到蒙太奇"), ClipIndex),
			FColor::Red,
			2.0f);
		return false;
	}

	ActiveSkillClipIndex = ClipIndex;
	bPendingAdvanceToNextClip = false;
	bHandledCurrentClipFinish = false;

	// 最后一段一旦正式起手，本条 Spirit 连段就已经进入“确定会消耗一次冷却”的阶段。
	// 中间段则先允许进入待命，只有组件侧待命超时才提交流却；取消收尾和异常收尾都不再补冷却。
	const bool bIsLastClip = ActiveSkillClipIndex >= ActiveSkillClipCount - 1;
	if (bIsLastClip && !CommitSpiritSkillCooldownIfNeeded())
	{
		Debug::Print(TEXT("[GA][SpiritSkill] 启动最后一段失败：冷却提交失败"), FColor::Red, 2.0f);
		return false;
	}

	if (ActiveSpiritAbilityEntryConfig.IsOffensiveSpiritSkill())
	{
		if (UHeroAttackComponent* HeroAttackComponent = GetHeroAttackComponentFromActorInfo())
		{
			HeroAttackComponent->SetCurrentAttackHitConfig(SkillClip->HitConfig);
			if (SkillClip->bShouldSpawnProjectile)
			{
				HeroAttackComponent->SetCurrentAttackProjectileSpawnConfig(SkillClip->ProjectileSpawnConfig);
			}
			else
			{
				HeroAttackComponent->ClearCurrentAttackProjectileSpawnConfig();
			}

			bAppliedSpiritOffensiveRuntimeState = true;
		}

		if (AActionHeroCharacter* HeroCharacter = GetHeroCharacterFromActorInfo())
		{
			if (UHeroTargetingComponent* HeroTargetingComponent = HeroCharacter->GetHeroTargetingComponent())
			{
				HeroTargetingComponent->TryApplyOffensiveFacing(
					EActionSimpleTurnAssistTriggerSource::SpiritOffensive);
			}
		}
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][SpiritSkill] 播放技能段：技能=%s 段=%d/%d"),
			ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
			ActiveSkillClipIndex + 1,
			ActiveSkillClipCount),
		FColor::Cyan,
		1.5f);

	const bool bMontageStarted = PlayHeroMontage(
		SkillMontage,
		TEXT("PlaySpiritSkillMontage"),
		TEXT("SpiritSkill"));
	if (!bMontageStarted)
	{
		return false;
	}

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		// Spirit 技能从 Idle 直接起手时，也要和普通攻击一样正式带角色进入 Combo。
		// 这样当前已装备武器会立即切到 WeaponSocket，避免 Spirit 蒙太奇和 Holster 挂点冲突。
		HeroCombatComponent->EnterComboModeImmediatelyForActivePresentation();
		HeroCombatComponent->UpdateRunningAnimMontage(SkillMontage);
		HeroCombatComponent->SetRunningAnimationReactGuardContext(
			SkillMontage,
			EActionRunningAnimationSemantic::NonReact,
			SkillClip->MinIncomingReactPriorityToInterrupt);

		// GA 只报告“这一段已经正式起手”。
		// 具体该把待命索引推进到哪里、是否记录成本已提交、是否重置旧超时计时器，
		// 都统一交给组件侧那份持久运行时状态处理。
		HeroCombatComponent->HandleSpiritSkillClipStarted(
			ActiveSpiritInputTag,
			ActiveSpiritAbilityEntryConfig.SpiritSkillConfig,
			ActiveSkillClipIndex,
			ActiveSkillClipCount);
	}

	return true;
}

void UHeroGA_SpiritSkill::HandleSpiritSkillClipFinished(const bool bWasCancelled)
{
	if (bHandledCurrentClipFinish || bAbilityFinished)
	{
		return;
	}

	bHandledCurrentClipFinish = true;
	CloseSpiritChainWindowIfNeeded();

	// 被打断、被取消或首段失败后的收口，统一按“本次链已经终止”处理：
	// 当前不再因为取消本身补冷却；Spirit 冷却正式只允许由最后一段起手或待命超时提交。
	if (bWasCancelled)
	{
		if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
		{
			if (!HeroCombatComponent->HasSpiritSkillChainQualification(ActiveSpiritInputTag)
				|| bSpiritCooldownCommitted
				|| ActiveSkillClipIndex >= ActiveSkillClipCount - 1)
			{
				HeroCombatComponent->ResetSpiritSkillComboState(ActiveSpiritInputTag);
			}
			else
			{
				Debug::Print(
					FString::Printf(
						TEXT("[GA][SpiritSkill] 中断保留资格：技能=%s State=%s"),
						ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
						*HeroCombatComponent->DescribeSpiritSkillComboRuntimeState(ActiveSpiritInputTag)),
					FColor::Yellow,
					1.5f);
			}
		}

		ClearBufferedSpiritSkillInputIfAny();
		FinishSpiritSkillAbility(true);
		return;
	}

	// 只有在同一次激活里明确收到接段输入时，才直接无缝启动下一段。
	// 这里不会读取组件侧待命态；组件只负责跨激活保留状态，不参与本次蒙太奇内部的即时接段。
	if (bPendingAdvanceToNextClip && CanAdvanceSpiritClipWithinCurrentActivation())
	{
		if (!StartSpiritSkillClip(ActiveSkillClipIndex + 1))
		{
			if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
			{
				HeroCombatComponent->ResetSpiritSkillComboState(ActiveSpiritInputTag);
			}

			ClearBufferedSpiritSkillInputIfAny();
			FinishSpiritSkillAbility(true);
		}

		return;
	}

	// 多段 Spirit 的中间段结束后，如果还没进入最后一段且当前技能允许持久索引，
	// 就把“下一次应播放哪一段”和“等待多久超时”转交给组件侧待命态。
	// 这样 GA 可以结束，但同一 Spirit 输入稍后重新按下时仍能从正确段位继续。
	if (!bSpiritCooldownCommitted
		&& ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.bUseComboIndex
		&& ActiveSkillClipCount > 1)
	{
		if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
		{
			if (HeroCombatComponent->BeginWaitingForNextSpiritSkillClip(ActiveSpiritInputTag))
			{
				Debug::Print(
					FString::Printf(
						TEXT("[GA][SpiritSkill] 进入待命：技能=%s 待命段=%d 超时=%.2f"),
						ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
						HeroCombatComponent->ResolveSpiritSkillClipIndexToPlay(
							ActiveSpiritInputTag,
							ActiveSpiritAbilityEntryConfig.SpiritSkillConfig,
							ActiveSkillClipCount) + 1,
						ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.ComboChainTimeoutSeconds),
					FColor::Yellow,
					1.5f);

				ClearBufferedSpiritSkillInputIfAny();
				FinishSpiritSkillAbility(false);
				return;
			}
		}
	}

	// 走到这里说明这条链不会再进入组件侧待命：
	// 要么已经是最后一段，要么当前配置异常导致中间段没能建立待命。
	// 最后一段的冷却应该早已在起手时提交；中间段异常则直接记 warning 并清理，不再顺手补冷却。
	const bool bCurrentClipIsLast = ActiveSkillClipIndex >= ActiveSkillClipCount - 1;
	if (!bSpiritCooldownCommitted && !bCurrentClipIsLast)
	{
		const FString SkillDebugName =
			ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : ActiveSpiritSkillDebugName;
		UE_LOG(
			ActionRPG,
			Warning,
			TEXT("[GA][SpiritSkill] 非最后一段结束时未能建立待命；本次只清理链状态，不提交冷却。Skill=%s Input=%s Clip=%d/%d UseComboIndex=%d PendingAdvance=%d"),
			*SkillDebugName,
			ActiveSpiritInputTag.IsValid() ? *ActiveSpiritInputTag.ToString() : TEXT("无效"),
			ActiveSkillClipIndex == INDEX_NONE ? 0 : ActiveSkillClipIndex + 1,
			ActiveSkillClipCount,
			ActiveSpiritAbilityEntryConfig.SpiritSkillConfig.bUseComboIndex ? 1 : 0,
			bPendingAdvanceToNextClip ? 1 : 0);
	}

	if (UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo())
	{
		HeroCombatComponent->ResetSpiritSkillComboState(ActiveSpiritInputTag);
	}

	ClearBufferedSpiritSkillInputIfAny();
	FinishSpiritSkillAbility(false);
}

bool UHeroGA_SpiritSkill::CommitSpiritSkillCooldownIfNeeded()
{
	// 同一条 Spirit 链的冷却只允许正式提交一次。
	// 当前正式提交口固定只剩两处：最后一段起手，以及组件侧待命超时后的外部补冷却。
	// 因此这里仍需要显式标记挡住重复提交，但不再由取消收尾或 EndAbility 兜底触发。
	if (bSpiritCooldownCommitted)
	{
		return true;
	}

	if (!CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false))
	{
		return false;
	}

	bSpiritCooldownCommitted = true;
	const FGameplayTag SpiritCooldownTag =
		ResolveSpiritSkillCooldownTagForContext(CurrentSpecHandle, CurrentActorInfo);
	Debug::Print(
		FString::Printf(
			TEXT("[GA][SpiritSkill] 冷却已提交：技能=%s 输入=%s 冷却=%s 当前段=%d/%d"),
			ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
			ActiveSpiritInputTag.IsValid() ? *ActiveSpiritInputTag.ToString() : TEXT("无效"),
			SpiritCooldownTag.IsValid() ? *SpiritCooldownTag.ToString() : TEXT("无效"),
			ActiveSkillClipIndex == INDEX_NONE ? 0 : ActiveSkillClipIndex + 1,
			ActiveSkillClipCount),
		FColor::Yellow,
		1.5f);
	return true;
}

void UHeroGA_SpiritSkill::ClearBufferedSpiritSkillInputIfAny()
{
	UHeroCombatInputComponent* HeroCombatInputComponent = GetHeroCombatInputComponentFromActorInfo();
	if (!HeroCombatInputComponent)
	{
		return;
	}

	const FGameplayTag CurrentSpiritInputTag = GetCurrentSpiritSkillInputTag();
	const FGameplayTag BufferedTargetInputTag =
		ActiveSpiritInputTag.IsValid() ? ActiveSpiritInputTag : CurrentSpiritInputTag;
	if (!BufferedTargetInputTag.IsValid())
	{
		return;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (HeroCombatInputComponent->PeekBufferedInputSnapshot(BufferedInputSnapshot)
		&& BufferedInputSnapshot.InputTag == BufferedTargetInputTag)
	{
		HeroCombatInputComponent->ClearBufferedInput();
	}
}

void UHeroGA_SpiritSkill::FinishSpiritSkillAbility(const bool bWasCancelled)
{
	if (bAbilityFinished)
	{
		return;
	}

	bAbilityFinished = true;

	Debug::Print(
		FString::Printf(
			TEXT("[GA][SpiritSkill] 结束：技能=%s 已取消=%d"),
			ActiveSpiritSkillDebugName.IsEmpty() ? TEXT("未命名灵武器技能") : *ActiveSpiritSkillDebugName,
			bWasCancelled ? 1 : 0),
		bWasCancelled ? FColor::Red : FColor::Green,
		2.0f);

	K2_EndAbility();
}
