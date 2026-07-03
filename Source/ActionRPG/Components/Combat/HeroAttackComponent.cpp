#include "Components/Combat/HeroAttackComponent.h"

#include "ActionGameplayTags.h"
#include "ActionType/ActionDamageScalingTypes.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutEffectComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "Engine/World.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/Projectiles/ActionProjectileBase.h"
#include "Items/Weapons/HeroWeaponBase.h"
#include "TimerManager.h"

namespace HeroAttackTagRouting
{
	static FGameplayTag ResolveTagByLoadoutSlot(
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FGameplayTag& UnarmedTag,
		const FGameplayTag& MeleeTag,
		const FGameplayTag& RangedTag,
		const FGameplayTag& HybridTag)
	{
		switch (InLoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			return UnarmedTag;

		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			return MeleeTag;

		case EHeroWeaponLoadoutSlot::RangedWeapon:
			return RangedTag;

		case EHeroWeaponLoadoutSlot::HybridWeapon:
			return HybridTag;

		default:
			break;
		}

		return FGameplayTag();
	}
}

namespace HeroAttackProjectileDebug
{
	static const TCHAR* GetResolvedConfigSourceText(const EActionResolvedProjectileConfigSource InSource)
	{
		switch (InSource)
		{
		case EActionResolvedProjectileConfigSource::DefaultProjectileConfig:
			return TEXT("武器默认发射物");

		case EActionResolvedProjectileConfigSource::SwitchableProjectileConfig:
			return TEXT("当前切换发射物");

		case EActionResolvedProjectileConfigSource::BranchProjectileOverride:
			return TEXT("攻击条目覆写发射物");

		case EActionResolvedProjectileConfigSource::None:
		default:
			break;
		}

		return TEXT("未解析");
	}
}

namespace HeroAttackDamageRuntime
{
	static bool TryGetCurrentWeaponAttributeCache(
		const UHeroCombatComponent* InCombatComponent,
		FActionWeaponAttributeCacheData& OutWeaponAttributeCache)
	{
		OutWeaponAttributeCache.Reset();
		const UHeroEquipmentComponent* EquipmentComponent =
			InCombatComponent ? InCombatComponent->GetOwningHeroEquipmentComponent() : nullptr;
		const UHeroLoadoutContextComponent* LoadoutContextComponent =
			InCombatComponent ? InCombatComponent->GetOwner()->FindComponentByClass<UHeroLoadoutContextComponent>() : nullptr;
		if (!EquipmentComponent || !LoadoutContextComponent)
		{
			return false;
		}

		return LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
			EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
			OutWeaponAttributeCache);
	}

	static bool ResolveAllowsAdditionalHitEffects(
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		const FActionWeaponAttributeCacheData* InWeaponAttributeCache)
	{
		if (InWeaponDefinition)
		{
			return InWeaponDefinition->AllowsAdditionalHitEffects();
		}

		return InWeaponAttributeCache
			? InWeaponAttributeCache->bAllowAdditionalHitEffects
			: true;
	}

	static void BuildCombatAttributeSnapshot(
		const UActionAttributeSetBase* InAttributeSet,
		const FActionWeaponAttributeCacheData* InWeaponAttributeCache,
		FActionCombatAttributeSnapshot& OutAttributeSnapshot)
	{
		OutAttributeSnapshot = FActionCombatAttributeSnapshot();

		const float AttackPowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->AttackPowerBonus : 0.f;
		const float DefensePowerBonus = InWeaponAttributeCache ? InWeaponAttributeCache->DefensePowerBonus : 0.f;
		const float MaxHealthBonus = InWeaponAttributeCache ? InWeaponAttributeCache->MaxHealthBonus : 0.f;
		const float CriticalChanceBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalChanceBonus : 0.f;
		const float CriticalDamageBonus = InWeaponAttributeCache ? InWeaponAttributeCache->CriticalDamageBonus : 0.f;
		const float OutgoingDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->OutgoingDamageMultiplierBonus : 0.f;
		const float ExtraDamageMultiplierBonus =
			InWeaponAttributeCache ? InWeaponAttributeCache->ExtraDamageMultiplierBonus : 0.f;

		OutAttributeSnapshot.AttackPower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetAttackPower() : 0.f) + AttackPowerBonus),
			0.f);
		OutAttributeSnapshot.DefensePower = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetDefensePower() : 0.f) + DefensePowerBonus),
			0.f);
		OutAttributeSnapshot.MaxHealth = FMath::Max(ActionRoundBaseValueToInteger(
			(InAttributeSet ? InAttributeSet->GetMaxHealth() : 0.f) + MaxHealthBonus),
			0.f);
		OutAttributeSnapshot.OutgoingDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetOutgoingDamageMultiplier() : 1.f) + OutgoingDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.ExtraDamageMultiplier = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetExtraDamageMultiplier() : 1.f) + ExtraDamageMultiplierBonus),
			0.f);
		OutAttributeSnapshot.CriticalChance = ActionRoundRatioValueToFourPlaces(FMath::Clamp(
			(InAttributeSet ? InAttributeSet->GetCriticalChance() : 0.f) + CriticalChanceBonus,
			0.f,
			100.f));
		OutAttributeSnapshot.CriticalDamage = FMath::Max(ActionRoundRatioValueToFourPlaces(
			(InAttributeSet ? InAttributeSet->GetCriticalDamage() : 150.f) + CriticalDamageBonus),
			100.f);
	}

	static FActionDamageContextRuntimeState ResolveDamageContext(const UHeroCombatComponent* InCombatComponent)
	{
		FActionDamageContextRuntimeState DamageContext;
		DamageContext.AbilityLevel = 1;
		if (InCombatComponent)
		{
			InCombatComponent->TryGetActiveDamageContext(DamageContext);
		}

		DamageContext.AbilityLevel = FMath::Max(DamageContext.AbilityLevel, 1);
		return DamageContext;
	}

}

UHeroAttackComponent::UHeroAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHeroAttackComponent::TryResolveAttackExecutionConfigByRequestTag(
	const FGameplayTag& AttackRequestTag,
	FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
{
	OutResolvedConfig = FActionResolvedAttackExecutionConfig();

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CombatComponent || !CurrentWeaponDefinition || !AttackRequestTag.IsValid())
	{
		return false;
	}

	// 正式攻击条目会把请求标签直接解析到完整执行计划，
	// 攻击组件本身不再维护第二套请求路由或旧数组消费逻辑。
	return CurrentWeaponDefinition->TryResolveAttackExecutionConfigByRequestTag(
		AttackRequestTag,
		CombatComponent->GetComboIndex(),
		OutResolvedConfig);
}

bool UHeroAttackComponent::TryResolveAttackExecutionConfig(
	const FGameplayTag& InBranchTag,
	FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
{
	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition())
	{
		// 这里直接基于当前连段索引解析，
		// 供攻击 GA 在已经确定分支后，继续查询当前段该播放哪条动画、使用哪套命中参数。
		return CurrentWeaponDefinition->TryResolveAttackExecutionConfig(
			InBranchTag,
			GetOwningHeroCombatComponent()->GetComboIndex(),
			OutResolvedConfig);
	}

	OutResolvedConfig = FActionResolvedAttackExecutionConfig();
	return false;
}

FName UHeroAttackComponent::GetMontageContextNameForAttackBranch(const FGameplayTag& InBranchTag)
{
	// 蒙太奇上下文名统一按“攻击分支语义”返回，
	// 这样动画层、调试输出与后续编辑器检索都能用同一套稳定名字。
	if (InBranchTag == ActionGameplayTags::Attack_Branch_Heavy)
	{
		return TEXT("HeavyAttack");
	}

	if (InBranchTag == ActionGameplayTags::Attack_Branch_DodgeCounter)
	{
		return TEXT("DodgeFollowUpAttack");
	}

	if (InBranchTag == ActionGameplayTags::Attack_Branch_Sprint)
	{
		return TEXT("SprintAttack");
	}

	if (InBranchTag == ActionGameplayTags::Attack_Branch_Airborne)
	{
		return TEXT("AirAttack");
	}

	return TEXT("LightAttack");
}

FGameplayTag UHeroAttackComponent::ResolveScopedAttackAbilityInputTag(const FGameplayTag& AttackRequestTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return FGameplayTag();
	}

	const EHeroWeaponLoadoutSlot CurrentLoadoutSlot = CombatComponent->GetCurrentCombatLoadoutSlot();
	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Default)
	{
		// 轻攻击不是“一套通用 GA”，而是根据当前战斗武器槽路由到对应槽位自己的 Attack GA。
		return HeroAttackTagRouting::ResolveTagByLoadoutSlot(
			CurrentLoadoutSlot,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Light_Hybrid);
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Held)
	{
		// 重击同样按固定四槽分流，保证空手 / 近战 / 远程 / 混合可以各自拥有独立的 GA 与动画入口。
		return HeroAttackTagRouting::ResolveTagByLoadoutSlot(
			CurrentLoadoutSlot,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy_Hybrid);
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter)
	{
		// 闪避反击走独立标签，避免和普通轻攻击共用入口后再在 GA 内二次分流。
		return HeroAttackTagRouting::ResolveTagByLoadoutSlot(
			CurrentLoadoutSlot,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter_Hybrid);
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Sprint)
	{
		// 冲刺攻击也直接映射到当前槽位专属输入标签，方便每种武器槽配置不同冲刺动作。
		return HeroAttackTagRouting::ResolveTagByLoadoutSlot(
			CurrentLoadoutSlot,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint_Hybrid);
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Airborne)
	{
		// 空中攻击同理按槽位拆分，后续如果某个槽位没有空中攻击，只需要不授予对应 GA 即可。
		return HeroAttackTagRouting::ResolveTagByLoadoutSlot(
			CurrentLoadoutSlot,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Unarmed,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Melee,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Ranged,
			ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne_Hybrid);
	}

	return FGameplayTag();
}

bool UHeroAttackComponent::ShouldTreatAttackInputAsSprintAttack() const
{
	const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter();
	const AActionPlayerController* HeroController = HeroCharacter
		? Cast<AActionPlayerController>(HeroCharacter->GetController())
		: nullptr;
	if (!HeroCharacter || !HeroController)
	{
		return false;
	}

	// 冲刺攻击资格完全由运行时移动状态决定：
	// 只有当前仍处于 FastRun 且角色没有落入普通下落态时，才把这次攻击提升为 Sprint 请求。
	return HeroController->GetMoveState() == EMoveState::FastRun
		&& !HeroCharacter->GetCharacterMovement()->IsFalling();
}

FGameplayTag UHeroAttackComponent::ResolveAttackRequestTag(const FGameplayTag InputTag) const
{
	if (InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return FGameplayTag();
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return FGameplayTag();
	}

	if (CombatComponent->IsInNormalFallingState())
	{
		// 空中优先级最高，任何普通攻击输入只要处于正常下落，就先解析为空中攻击请求。
		return ActionGameplayTags::Attack_Request_Airborne;
	}

	if (CombatComponent->IsDodgeCounterAvailable())
	{
		// 完美闪避产生的闪反资格在地面态下优先于冲刺和轻重击分支。
		return ActionGameplayTags::Attack_Request_DodgeCounter;
	}

	if (ShouldTreatAttackInputAsSprintAttack())
	{
		// 仍处于 FastRun 的首个攻击输入被解析为冲刺攻击。
		return ActionGameplayTags::Attack_Request_Sprint;
	}

	if (const UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
		CombatInputComponent
		&& CombatInputComponent->GetInputButtonStateByTag(InputTag) == EActionInputButtonState::Held)
	{
		// 当运行时输入状态已经进入 Held，就把这次攻击视为重击请求。
		return ActionGameplayTags::Attack_Request_Held;
	}

	// 其余地面普通攻击全部回落为默认轻攻击请求。
	return ActionGameplayTags::Attack_Request_Default;
}

bool UHeroAttackComponent::TryResolveAttackBranchTag(FGameplayTag InputTag, FGameplayTag& OutBranchTag) const
{
	OutBranchTag = ActionGameplayTags::Attack_Branch_Light;

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CurrentWeaponDefinition)
	{
		return false;
	}

	// 输入本身并不直接决定最终分支；
	// 先解析成攻击请求，再由武器定义把请求映射到真正的攻击分支标签。
	OutBranchTag = CurrentWeaponDefinition->ResolveAttackBranchTag(ResolveAttackRequestTag(InputTag));
	return OutBranchTag.IsValid();
}

bool UHeroAttackComponent::TryResolveCurrentAttackExecutionConfig(
	FGameplayTag InputTag,
	FActionResolvedAttackExecutionConfig& OutResolvedConfig) const
{
	OutResolvedConfig = FActionResolvedAttackExecutionConfig();

	const FGameplayTag AttackRequestTag = ResolveAttackRequestTag(InputTag);
	if (!AttackRequestTag.IsValid())
	{
		return false;
	}

	// 这是“从当前输入直接走到当前段执行配置”的便捷入口。
	return TryResolveAttackExecutionConfigByRequestTag(AttackRequestTag, OutResolvedConfig);
}

void UHeroAttackComponent::ApplyResolvedAttackExecutionState(const FActionResolvedAttackExecutionConfig& InResolvedConfig)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !InResolvedConfig.IsValid())
	{
		return;
	}

	// 新一段攻击真正开始前，先把上一段残留的衔接窗口和取消窗口收掉。
	// 这样同一时刻只会存在当前这一段攻击自己的窗口状态，避免旧窗口跨段残留。
	ConsumePreserveComboIndexOnNextAttackFinalize();
	CombatComponent->CloseAbilityChainWindow();
	CombatComponent->CloseAbilityCancelWindow();

	if (InResolvedConfig.bResetComboIndexOnPlay)
	{
		CombatComponent->ResetComboIndex();
	}

	// 从这一刻开始，当前这条解析结果正式成为“正在执行的攻击段”：
	// 战斗模式、运行中蒙太奇、命中参数和连段索引都要同步切到这一段。
	CombatComponent->SetCombatMode(EHeroCombatMode::Combo);
	CombatComponent->UpdateRunningAnimMontage(InResolvedConfig.ResolvedMontage);
	CombatComponent->SetRunningAnimationReactGuardContext(
		InResolvedConfig.ResolvedMontage,
		EActionRunningAnimationSemantic::NonReact,
		InResolvedConfig.MinIncomingReactPriorityToInterrupt);

	// AttackClip 已经成为正式命中入口：
	// 每一段攻击都直接把当前 Clip 的命中配置写进运行时，不再借其他历史默认层补命中参数。
	SetCurrentAttackHitConfig(InResolvedConfig.ResolvedHitConfig);

	if (InResolvedConfig.bShouldSpawnProjectile)
	{
		SetCurrentAttackProjectileSpawnConfig(InResolvedConfig.ResolvedProjectileSpawnConfig);
	}
	else
	{
		ClearCurrentAttackProjectileSpawnConfig();
	}

	if (InResolvedConfig.bAdvanceComboIndexOnPlay)
	{
		// 连段索引在“真正开始播放这一段”时推进，
		// 这样中途未能起手成功的请求不会污染后续连段状态。
		CombatComponent->AdvanceComboIndex(InResolvedConfig.MontageCount);
	}
}

bool UHeroAttackComponent::IsAttackExecutionStillAuthorized(
	const FGameplayTag& AttackRequestTag,
	const bool bHasLocalDodgeCounterAuthorization) const
{
	if (AttackRequestTag != ActionGameplayTags::Attack_Request_DodgeCounter)
	{
		return true;
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	return bHasLocalDodgeCounterAuthorization
		|| (CombatComponent && CombatComponent->IsDodgeCounterAvailable());
}

void UHeroAttackComponent::ApplyAttackExecutionStartedForRequest(const FGameplayTag& AttackRequestTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter)
	{
		// 闪避反击一旦正式起手，就立刻把外部可见资格消费掉，
		// 防止同一轮资格又被后续其它攻击输入重复拿去使用。
		if (UHeroDefenseComponent* DefenseComponent = CombatComponent->GetOwningHeroDefenseComponent())
		{
			DefenseComponent->ConsumeDodgeCounterAvailability();
		}
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Sprint)
	{
		if (AActionPlayerController* HeroController = GetOwningHeroCharacter()
			? Cast<AActionPlayerController>(GetOwningHeroCharacter()->GetController())
			: nullptr)
		{
			// 冲刺攻击正式出招后，立刻把角色移动状态从 FastRun 拉回 Run，
			// 避免整个攻击期间仍然被当作高速冲刺。
			if (HeroController->GetMoveState() == EMoveState::FastRun)
			{
				// 冲刺攻击的起手副作用是“结束这轮 FastRun 语义”，
				// 后续即使攻击中断，也不应该继续把角色视为冲刺中。
				HeroController->SetMoveState(EMoveState::Run);
				HeroController->UpdateMovementData();
			}
		}
	}
}

void UHeroAttackComponent::SetCurrentAttackHitConfig(const FActionWeaponHitConfig& InHitConfig)
{
	CurrentAttackHitConfig = InHitConfig;
	bHasCurrentAttackHitConfig = true;
}

void UHeroAttackComponent::ClearCurrentAttackHitConfig()
{
	CurrentAttackHitConfig = FActionWeaponHitConfig();
	bHasCurrentAttackHitConfig = false;
}

bool UHeroAttackComponent::TryGetCurrentAttackHitConfigSnapshot(FActionWeaponHitConfig& OutHitConfig) const
{
	if (!bHasCurrentAttackHitConfig)
	{
		OutHitConfig = FActionWeaponHitConfig();
		return false;
	}

	OutHitConfig = CurrentAttackHitConfig;
	return true;
}

void UHeroAttackComponent::SetCurrentAttackProjectileSpawnConfig(const FActionProjectileSpawnConfig& InProjectileSpawnConfig)
{
	CurrentAttackProjectileSpawnConfig = InProjectileSpawnConfig;
	bHasCurrentAttackProjectileSpawnConfig = true;
}

void UHeroAttackComponent::ClearCurrentAttackProjectileSpawnConfig()
{
	CurrentAttackProjectileSpawnConfig = FActionProjectileSpawnConfig();
	bHasCurrentAttackProjectileSpawnConfig = false;
}

bool UHeroAttackComponent::TryBuildCurrentAttackDamagePayload(
	AActor* InTargetActor,
	FActionDamagePayload& OutDamagePayload) const
{
	return TryBuildCurrentAttackDamagePayloadForHitSource(
		InTargetActor,
		NAME_None,
		NAME_None,
		OutDamagePayload);
}

bool UHeroAttackComponent::TryBuildCurrentAttackDamagePayloadForHitSource(
	AActor* InTargetActor,
	const FName InPreferredHitSourceId,
	const FName InPreferredSourceComponentName,
	FActionDamagePayload& OutDamagePayload) const
{
	OutDamagePayload = FActionDamagePayload();

	if (!bHasCurrentAttackHitConfig || !GetOwner() || !IsValid(InTargetActor))
	{
		return false;
	}

	const UActionAttributeSetBase* OwnerAttributeSet = GetOwningActionAttributeSet();
	if (!OwnerAttributeSet)
	{
		return false;
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	AHeroWeaponBase* CurrentWeapon = GetCurrentEquippedWeapon();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	FActionWeaponAttributeCacheData WeaponAttributeCache;
	const bool bHasWeaponAttributeCache =
		HeroAttackDamageRuntime::TryGetCurrentWeaponAttributeCache(CombatComponent, WeaponAttributeCache);
	FActionCombatAttributeSnapshot AttributeSnapshot;
	HeroAttackDamageRuntime::BuildCombatAttributeSnapshot(
		OwnerAttributeSet,
		bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr,
		AttributeSnapshot);
	const FActionDamageContextRuntimeState DamageContext =
		HeroAttackDamageRuntime::ResolveDamageContext(CombatComponent);
	// 载荷层明确区分两种来源：
	// 1. InstigatorActor 表示这次攻击真正归属给谁；
	// 2. SourceActor 表示这次命中判定从哪个对象发出，例如当前装备武器。
	OutDamagePayload.InstigatorActor = GetOwner();
	OutDamagePayload.SourceActor = GetOwner();

	if (CurrentAttackHitConfig.HasAnyLevelDrivenDamageConfig())
	{
		// 这里先把“当前攻击段配置 + 当前角色属性快照 + 当前武器属性缓存”
		// 解析成一次数值层真正会提交的基础伤害、格挡体力消耗和削韧值。
		// 后面的命中来源、附加效果和受击表现配置，都会继续附着在同一份载荷上。
		bool bDidCritical = false;
		OutDamagePayload.BaseDamage = ActionResolveDrivenDamageValue(
			CurrentAttackHitConfig.HealthDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			true,
			bDidCritical);

		bool bIgnoredCritical = false;
		OutDamagePayload.GuardStaminaCost = ActionResolveDrivenDamageValue(
			CurrentAttackHitConfig.GuardStaminaCostValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
		OutDamagePayload.PoiseDamage = ActionResolveDrivenDamageValue(
			CurrentAttackHitConfig.PoiseDamageValueConfig,
			AttributeSnapshot,
			DamageContext.AbilityLevel,
			false,
			bIgnoredCritical);
	}

	// 命中收益已经正式切到 SpecialWeaponSwitchEnergy。
	// 因此这里直接把当前攻击段声明的奖励值写进伤害载荷，
	// 让后续命中解析器只按“这次命中最终有没有成功结算”来决定是否发奖。
	OutDamagePayload.SpecialWeaponSwitchEnergyRewardOnHit =
		FMath::Max(CurrentAttackHitConfig.SpecialWeaponSwitchEnergyRewardOnHit, 0.f);
	OutDamagePayload.HitTag = CurrentAttackHitConfig.HitTag;
	OutDamagePayload.DamageType = CurrentWeaponDefinition
		? CurrentWeaponDefinition->GetDamageType()
		: (bHasWeaponAttributeCache ? WeaponAttributeCache.DamageType : EActionDamageType::Physical);
	OutDamagePayload.DamageElementTypeTag = CurrentWeaponDefinition
		? CurrentWeaponDefinition->GetDamageElementTypeTag()
		: (bHasWeaponAttributeCache ? WeaponAttributeCache.DamageElementTypeTag : FGameplayTag());
	OutDamagePayload.bAllowAdditionalHitEffects = HeroAttackDamageRuntime::ResolveAllowsAdditionalHitEffects(
		CurrentWeaponDefinition,
		bHasWeaponAttributeCache ? &WeaponAttributeCache : nullptr);
	FillCurrentAttackHitSourceInfo(
		OutDamagePayload.HitSource,
		InPreferredHitSourceId,
		InPreferredSourceComponentName);
	OutDamagePayload.DefaultEffects = CurrentAttackHitConfig.DefaultEffects;
	OutDamagePayload.AdditionalEffects = CurrentAttackHitConfig.AdditionalEffects;
	OutDamagePayload.HitPresentationConfig = CurrentAttackHitConfig.HitPresentationConfig;
	OutDamagePayload.ExternalAdditionalEffects = WeaponAttributeCache.ExternalAdditionalHitEffects;
	OutDamagePayload.bCanBeBlocked = CurrentAttackHitConfig.bCanBeBlocked;
	OutDamagePayload.bCanBeParried = CurrentAttackHitConfig.bCanBeParried;
	OutDamagePayload.bCanBePerfectDodged = CurrentAttackHitConfig.bCanBePerfectDodged;
	OutDamagePayload.HitReactType = CurrentAttackHitConfig.HitReactType;
	OutDamagePayload.HitStateDuration = FMath::Max(CurrentAttackHitConfig.HitStateDuration, 0.f);

	float ResolvedKnockbackStrength = CurrentAttackHitConfig.KnockbackStrength;
	if (CombatComponent)
	{
		if (const UHeroHitSourceComponent* HitSourceComponent = CombatComponent->GetOwningHeroHitSourceComponent())
		{
			const FActionHitWindowRuntimeConfig& ActiveHitWindowRuntimeConfig =
				HitSourceComponent->GetActiveHitWindowRuntimeConfig();
			if (HitSourceComponent->IsHitWindowActive() && ActiveHitWindowRuntimeConfig.bOverrideKnockbackStrength)
			{
				// 命中窗口允许在极少数攻击帧上临时覆写默认击退。
				// 这里优先认当前正在运行的命中窗口配置，避免“同一段攻击里不同帧的击退差异”
				// 又回退成武器默认值。
				ResolvedKnockbackStrength = ActiveHitWindowRuntimeConfig.OverrideKnockbackStrength;
			}
		}
	}

	OutDamagePayload.KnockbackStrength = FMath::Max(ResolvedKnockbackStrength, 0.f);

	const bool bWeaponCollisionSource =
		OutDamagePayload.HitSource.SourceType == EActionHitSourceType::WeaponCollision;
	const AActor* ImpactSourceActor = GetOwner();
	if (bWeaponCollisionSource && CurrentWeapon)
	{
		OutDamagePayload.SourceActor = CurrentWeapon;
		ImpactSourceActor = CurrentWeapon;
	}

	OutDamagePayload.ImpactDirection = ImpactSourceActor
		? (InTargetActor->GetActorLocation() - ImpactSourceActor->GetActorLocation()).GetSafeNormal()
		: FVector::ZeroVector;
	// 冲击方向统一以命中源朝向目标的位置差计算，
	// 供受击解析器后续决定击退、击飞与受击朝向。
	return true;
}

bool UHeroAttackComponent::TrySpawnCurrentAttackProjectile()
{
	if (!bHasCurrentAttackProjectileSpawnConfig)
	{
		Debug::Print(TEXT("[发射物] 当前攻击段未启用发射物生成"), FColor::Yellow, 1.5f);
		return false;
	}

	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	AHeroWeaponBase* CurrentWeapon = GetCurrentEquippedWeapon();
	UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	UWorld* World = GetWorld();
	if (!World || !OwnerHeroCharacter || !CurrentWeapon || !CurrentWeaponDefinition)
	{
		Debug::Print(TEXT("[发射物] 缺少角色、武器或武器定义，无法生成发射物"), FColor::Red, 2.0f);
		return false;
	}

	FActionWeaponHitConfig ResolvedHitConfig;
	if (!ResolveCurrentAttackSourceHitConfig(ResolvedHitConfig))
	{
		Debug::Print(TEXT("[发射物] 未能解析当前攻击段命中配置"), FColor::Red, 2.0f);
		return false;
	}

	FActionProjectileConfig ResolvedProjectileConfig;
	EActionResolvedProjectileConfigSource ResolvedConfigSource =
		EActionResolvedProjectileConfigSource::None;
	FGameplayTag SelectedProjectileConfigTag;
	if (!ResolveCurrentAttackProjectileConfig(
		ResolvedProjectileConfig,
		&ResolvedConfigSource,
		&SelectedProjectileConfigTag))
	{
		Debug::Print(TEXT("[发射物] 未能解析当前攻击段发射物配置"), FColor::Red, 2.0f);
		return false;
	}

	TSubclassOf<AActionProjectileBase> ProjectileClass = ResolvedProjectileConfig.ProjectileClass.Get();
	if (!ProjectileClass)
	{
		Debug::Print(TEXT("[发射物] 发射物类未加载或未配置"), FColor::Red, 2.0f);
		return false;
	}

	FActionDamagePayload ProjectileDamagePayloadTemplate;
	if (!CurrentWeapon->BuildProjectileDamagePayloadTemplate(
		ResolvedHitConfig,
		ResolvedProjectileConfig,
		ProjectileDamagePayloadTemplate))
	{
		Debug::Print(TEXT("[发射物] 未能构建发射物命中载荷模板"), FColor::Red, 2.0f);
		return false;
	}

	Debug::Print(FString::Printf(
		TEXT("[发射物] 模板构建成功：来源=%s，选中标签=%s，发射物标签=%s，Socket=%s"),
		HeroAttackProjectileDebug::GetResolvedConfigSourceText(ResolvedConfigSource),
		SelectedProjectileConfigTag.IsValid() ? *SelectedProjectileConfigTag.ToString() : TEXT("默认发射物"),
		ResolvedProjectileConfig.ProjectileTag.IsValid() ? *ResolvedProjectileConfig.ProjectileTag.ToString() : TEXT("未配置"),
		CurrentAttackProjectileSpawnConfig.SpawnSocketName != NAME_None
			? *CurrentAttackProjectileSpawnConfig.SpawnSocketName.ToString()
			: TEXT("无")),
		FColor::Cyan,
		2.0f);

	ProjectileDamagePayloadTemplate.HitSource.SourceSocketName =
		CurrentAttackProjectileSpawnConfig.SpawnSocketName;

	FActionProjectileInitializationContext ProjectileInitializationContext;
	ProjectileInitializationContext.ResolvedConfigSource = ResolvedConfigSource;
	ProjectileInitializationContext.SelectedProjectileConfigTag = SelectedProjectileConfigTag;
	ProjectileInitializationContext.SpawnSocketName =
		CurrentAttackProjectileSpawnConfig.SpawnSocketName;

	const FTransform SpawnTransform = ResolveCurrentAttackProjectileSpawnTransform(CurrentAttackProjectileSpawnConfig);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerHeroCharacter;
	SpawnParameters.Instigator = OwnerHeroCharacter;

	AActionProjectileBase* SpawnedProjectile = World->SpawnActor<AActionProjectileBase>(
		ProjectileClass,
		SpawnTransform,
		SpawnParameters);
	if (!SpawnedProjectile)
	{
		Debug::Print(TEXT("[发射物] 生成发射物 Actor 失败"), FColor::Red, 2.0f);
		return false;
	}

	SpawnedProjectile->ApplyProjectileConfig(ResolvedProjectileConfig);
	SpawnedProjectile->InitializeProjectile(
		ProjectileDamagePayloadTemplate,
		ProjectileInitializationContext);
	Debug::Print(FString::Printf(
		TEXT("[发射物] 已生成当前攻击段发射物：来源=%s，发射物标签=%s"),
		HeroAttackProjectileDebug::GetResolvedConfigSourceText(ResolvedConfigSource),
		ResolvedProjectileConfig.ProjectileTag.IsValid() ? *ResolvedProjectileConfig.ProjectileTag.ToString() : TEXT("未配置")),
		FColor::Green,
		1.5f);
	return true;
}

void UHeroAttackComponent::RestoreComboIndexAfterFailedAttackStart()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	// 只有在上一段攻击已经显式要求“下一次失败时回退连段索引”时，才执行回滚。
	// 这样可以避免普通激活失败也把本来无关的连段状态错误清掉。
	if (ConsumePreserveComboIndexOnNextAttackFinalize())
	{
		// 这里的回滚只处理“接段预保留后，结果新段没能真正起手”的情况，
		// 避免连段索引卡在一个已经不存在的中间段位。
		CombatComponent->ResetComboIndex();
	}
}

void UHeroAttackComponent::FinalizeAttackExecutionRuntime(const bool bShouldResetComboIndexOnFinalizeIfNotChained)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	// 攻击 Ability 结束时再兜底关闭一次当前武器的命中检测，
	// 防止蒙太奇在中断边界下没有完整走到 NotifyEnd，残留攻击判定继续存在。
	if (AHeroWeaponBase* CurrentWeapon = CombatComponent->GetCurrentEquippedWeapon())
	{
		CurrentWeapon->EndAttackDetection();
	}

	// 收尾阶段统一回收这一段攻击留下的局部运行时状态：
	// 命中配置、攻击开关、蒙太奇引用、链窗和取消窗都必须在这里关干净。
	ClearCurrentAttackHitConfig();
	CombatComponent->SetAttackEnabled(true);
	CombatComponent->ClearRunningAnimMontageReference();
	CombatComponent->CloseAbilityChainWindow();
	CombatComponent->CloseAbilityCancelWindow();
	ClearCurrentAttackProjectileSpawnConfig();

	if (bShouldResetComboIndexOnFinalizeIfNotChained
		&& !ShouldPreserveComboIndexOnNextAttackFinalize())
	{
		// 这一段攻击已经推进过连段，但最终没有接上下一段时，
		// 就在这里把索引恢复到起始位置，避免后续输入错误沿用中间段位。
		CombatComponent->ResetComboIndex();
	}

	if (AActionPlayerController* HeroController = GetOwningHeroCharacter()
		? Cast<AActionPlayerController>(GetOwningHeroCharacter()->GetController())
		: nullptr)
	{
		// 冲刺攻击可能在起手时把移动状态从 FastRun 改回 Run。
		// 这里保留兜底收尾，避免中断路径残留高速移动状态。
		if (HeroController->GetMoveState() == EMoveState::FastRun)
		{
			HeroController->SetMoveState(EMoveState::Run);
			HeroController->UpdateMovementData();
		}
	}

	// 最后把输入恢复延后到下一帧，确保本帧动画结束、窗口关闭和状态回收已经完全落地。
	RequestRecoverCombatInputAfterAttack();
}

void UHeroAttackComponent::MarkPreserveComboIndexOnNextAttackFinalize()
{
	bPreserveComboIndexOnNextAttackFinalize = true;
}

bool UHeroAttackComponent::ShouldPreserveComboIndexOnNextAttackFinalize() const
{
	return bPreserveComboIndexOnNextAttackFinalize;
}

bool UHeroAttackComponent::ConsumePreserveComboIndexOnNextAttackFinalize()
{
	const bool bShouldPreserve = bPreserveComboIndexOnNextAttackFinalize;
	bPreserveComboIndexOnNextAttackFinalize = false;
	return bShouldPreserve;
}

bool UHeroAttackComponent::HasPendingDodgeCounterExecutionAuthorization() const
{
	return bPendingDodgeCounterExecutionAuthorization;
}

void UHeroAttackComponent::SetPendingDodgeCounterExecutionAuthorization(const bool bAuthorized)
{
	bPendingDodgeCounterExecutionAuthorization = bAuthorized;
}

bool UHeroAttackComponent::ConsumePendingDodgeCounterExecutionAuthorization()
{
	const bool bAuthorized = bPendingDodgeCounterExecutionAuthorization;
	bPendingDodgeCounterExecutionAuthorization = false;
	return bAuthorized;
}

bool UHeroAttackComponent::CanActivateAttackInputNow(const FGameplayTag InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent
		? CombatComponent->GetOwningHeroWeaponSwitchComponent()
		: nullptr;
	if (!CombatComponent)
	{
		return false;
	}

	if (!CombatComponent->IsAttackInputTag(InputTag))
	{
		// 非攻击输入不走这里的战斗限制，直接视为“当前可继续向下游处理”。
		return true;
	}

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchTransactionInProgress())
	{
		// 切武交易态还没有完成时，不允许攻击抢在新武器状态稳定前启动。
		return false;
	}

	const bool bWeaponSwitchAttackAllowedByChainWindow =
		CombatComponent->IsWeaponSwitchPresentationChainInputAllowed(InputTag);
	const bool bWeaponSwitchAttackAllowedByCancelWindow =
		CombatComponent->IsWeaponSwitchPresentationCancelInputAllowed(InputTag);

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive())
	{
		if (!(bWeaponSwitchAttackAllowedByChainWindow || bWeaponSwitchAttackAllowedByCancelWindow))
		{
			// 切武表现期默认阻断攻击，只有链窗或取消窗明确对白名单放行后才允许抢入。
			return false;
		}

		// 切武表现期里的攻击抢入不依赖“当前是否处于普通攻击连段态”或攻击总开关。
		// 只要切武事务已经结束，且当前链窗 / 取消窗明确接收攻击，就允许继续向下游提交。
		return true;
	}

	if (IsAttackChainRelevantState())
	{
		// 连段相关状态下不看“是否启用攻击”的总开关，而是严格依赖连段窗口是否接收本次输入。
		return CombatComponent->AbilityWindowRuntimeState.IsAbilityChainWindowActive()
			&& CombatComponent->AbilityWindowRuntimeState.AcceptsChainInput(InputTag);
	}

	if (CombatComponent->IsAttackInputBlockedByCombatReact(InputTag))
	{
		// 受击硬锁期间直接封锁攻击入口，避免攻击链越过受击状态强行启动。
		return false;
	}

	if (!CombatComponent->IsAttackEnabled())
	{
		// 普通攻击入口关闭时，只允许走“取消窗口里允许的攻击输入”这一条例外恢复链。
		return CombatComponent->IsAbilityCancelContextActive()
			&& CombatComponent->IsAbilityCancelInputAllowedNow(InputTag);
	}

	return true;
}

bool UHeroAttackComponent::ShouldBufferAttackInputNow(const FGameplayTag& InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent
		? CombatComponent->GetOwningHeroWeaponSwitchComponent()
		: nullptr;
	if (!CombatComponent || InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive())
	{
		// 切武表现期里的攻击输入默认先缓冲。
		// 后续既可能在切武链窗 / 取消窗开启时被立刻消费，
		// 也可能在表现期彻底结束后走恢复链补发。
		return true;
	}

	if (IsAttackChainRelevantState())
	{
		// 连段阶段的攻击输入默认允许缓存，等链窗真正打开时再决定是否接下一段。
		return true;
	}

	if (CombatComponent->IsAbilityCancelContextActive())
	{
		// 取消上下文里，关闭中的取消窗与“当前帧不接收这类取消输入”的情况都应先缓存，
		// 恢复链会在窗口合法时再次尝试消费，避免玩家提早输入被丢掉。
		return !CombatComponent->AbilityWindowRuntimeState.IsAbilityCancelWindowActive()
			|| CombatComponent->AbilityWindowRuntimeState.AcceptsCancelInput(InputTag);
	}

	// 其余情况只要攻击总开关仍未恢复，就允许把这次输入先记下来。
	return !CombatComponent->IsAttackEnabled();
}

bool UHeroAttackComponent::TryQueueBufferedAttackInput(
	const FGameplayTag& InputTag,
	const EActionInputEvent InputEvent,
	const bool bCanBuffer,
	FGameplayTag ResolvedAttackRequestTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !CombatComponent->IsAttackInputTag(InputTag) || !bCanBuffer)
	{
		return false;
	}

	if (!ShouldBufferAttackInputNow(InputTag))
	{
		return false;
	}

	if (InputEvent == EActionInputEvent::Held && ResolvedAttackRequestTag.IsValid())
	{
		if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition())
		{
			if (CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(ResolvedAttackRequestTag)
				!= EActionInputEvent::Held)
			{
				// Held 阶段只应该为“真正以 Held 触发”的攻击建立缓冲。
				// 对 Light / Airborne / Sprint / DodgeCounter 这类按 Pressed 或 Released 成立的请求来说，
				// Held 只是中间经过的输入阶段，不应覆盖掉原先已经建立的按下/释放语义。
				return false;
			}
		}
	}

	// 缓冲里不仅保存输入标签，还要连同当前已解析出的攻击请求一起存下来，
	// 这样恢复时就不会因为角色状态变化而把原本想出的攻击类型解析错。
	CombatComponent->QueueBufferedInput(InputTag, InputEvent, ResolvedAttackRequestTag);
	return true;
}

bool UHeroAttackComponent::CanConsumeBufferedAttackInputNow(const FActionBufferedInput& BufferedInput) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return false;
	}

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CurrentWeaponDefinition)
	{
		return false;
	}

	FGameplayTag BufferedAttackRequestTag;
	EActionInputEvent BufferedTriggerInputEvent = BufferedInput.TriggerEvent;
	if (!TryResolveBufferedAttackConsumeSpec(
		BufferedInput,
		BufferedAttackRequestTag,
		BufferedTriggerInputEvent))
	{
		return false;
	}

	if (CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(BufferedAttackRequestTag) != BufferedTriggerInputEvent)
	{
		// 如果当前武器配置要求的触发阶段已经和缓冲记录不一致，说明这条缓冲不再安全，先拒绝消费。
		return false;
	}

	if (IsAttackChainRelevantState())
	{
		// 连段恢复只接受“攻击已重新开放 + 链窗已打开 + 当前输入被链窗接纳”这一组条件。
		return CombatComponent->IsAttackEnabled()
			&& CombatComponent->AbilityWindowRuntimeState.IsAbilityChainWindowActive()
			&& CombatComponent->AbilityWindowRuntimeState.AcceptsChainInput(BufferedInput.InputTag);
	}

	const UHeroWeaponSwitchComponent* WeaponSwitchComponent = CombatComponent->GetOwningHeroWeaponSwitchComponent();
	if (WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive())
	{
		return CombatComponent->IsWeaponSwitchPresentationChainInputAllowed(BufferedInput.InputTag)
			|| CombatComponent->IsWeaponSwitchPresentationCancelInputAllowed(BufferedInput.InputTag);
	}

	if (CombatComponent->IsAbilityCancelContextActive())
	{
		// 取消上下文里的缓冲消费复用即时激活判断，确保恢复行为与实时输入走同一套门禁规则。
		return CanActivateAttackInputNow(BufferedInput.InputTag);
	}
	// 普通恢复场景下，只要攻击已重新开放且不处于切武演出期，就允许把缓冲输入重新投递。
	return CombatComponent->IsAttackEnabled()
		&& !(WeaponSwitchComponent && WeaponSwitchComponent->IsWeaponSwitchPresentationActive());
}

bool UHeroAttackComponent::TryConsumeBufferedAttackInput()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !GetWorld())
	{
		return false;
	}

	FActionBufferedInput LocalBufferedInput;
	if (!CombatInputComponent->PeekBufferedInputSnapshot(LocalBufferedInput))
	{
		return false;
	}

	if (!CombatComponent->IsAttackInputTag(LocalBufferedInput.InputTag))
	{
		return false;
	}

	// 先把缓冲里保存的输入重新还原成“本次真正要出的攻击请求 + 触发阶段”，
	// 然后再做当前时刻是否允许恢复消费的判定。
	FGameplayTag BufferedAttackRequestTag;
	EActionInputEvent BufferedTriggerInputEvent = LocalBufferedInput.TriggerEvent;
	if (!TryResolveBufferedAttackConsumeSpec(
		LocalBufferedInput,
		BufferedAttackRequestTag,
		BufferedTriggerInputEvent))
	{
		return false;
	}

	if (!CanConsumeBufferedAttackInputNow(LocalBufferedInput))
	{
		return false;
	}

	// 真正消费前先把缓冲快照从输入组件里弹出，
	// 这样后续无论是成功重新投递到 ASC，还是被更上层链路再次缓存，
	// 都不会和这条旧快照形成重复消费。
	if (!CombatInputComponent->ConsumeBufferedInputSnapshot(LocalBufferedInput))
	{
		return false;
	}
	CombatComponent->BroadcastCombatEvent(ActionGameplayTags::Player_Event_InputBuffer_Consumed);
	return CombatComponent->ProcessAbilityInput(
		LocalBufferedInput.InputTag,
		BufferedTriggerInputEvent,
		false,
		BufferedAttackRequestTag);
}

bool UHeroAttackComponent::HandleAttackInputReleased(const FGameplayTag& InputTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !GetWorld() || !CombatComponent->IsAttackInputTag(InputTag))
	{
		return false;
	}

	FActionBufferedInput BufferedInputSnapshot;
	if (CombatInputComponent->PeekBufferedInputSnapshot(BufferedInputSnapshot)
		&& BufferedInputSnapshot.InputTag == InputTag)
	{
		// Released 阶段先查看当前是否已经有同一按键的攻击缓冲，
		// 避免一条“更晚到的松手语义”把之前已经成立的攻击请求覆盖掉。
		const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
		const EActionInputEvent BufferedResolvedTriggerEvent =
			(CurrentWeaponDefinition && BufferedInputSnapshot.ResolvedAttackRequestTag.IsValid())
				? CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(
					BufferedInputSnapshot.ResolvedAttackRequestTag)
				: BufferedInputSnapshot.TriggerEvent;

		if (BufferedInputSnapshot.TriggerEvent == EActionInputEvent::Held
			&& BufferedResolvedTriggerEvent == EActionInputEvent::Held)
		{
			// 重击在 Held 阈值那一刻就已经算“输入意图成立”。
			// 如果这次 Held 攻击因为处决等硬锁被缓冲下来，玩家随后松手不应把它取消，
			// 否则会出现“明明已经完成长按重击输入，但恢复后什么都不出”的问题。
			return true;
		}

		if (CurrentWeaponDefinition
			&& BufferedInputSnapshot.ResolvedAttackRequestTag.IsValid()
			&& BufferedResolvedTriggerEvent == EActionInputEvent::Pressed)
		{
			// 冲刺攻击、闪避反击这类 Pressed 即成立的请求，一旦已经在硬锁期间被缓冲下来，
			// Released 阶段就不应再把这次输入覆盖成新的释放语义。
			// 否则恢复首帧时，之前已经成立的按下意图会被一条晚到的释放事件抹掉。
			return true;
		}
	}

	return false;
}

bool UHeroAttackComponent::CanReplayHeldAttackInput(const FGameplayTag& InputTag) const
{
	if (InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CombatComponent || !CurrentWeaponDefinition)
	{
		return false;
	}

	FGameplayTag AttackRequestTag;
	EActionInputEvent TriggerInputEvent = EActionInputEvent::Pressed;
	if (!TryResolveHeldReplayAttackSpec(InputTag, AttackRequestTag, TriggerInputEvent))
	{
		return false;
	}

	// Held 回放只服务两类输入：
	// 1. 真正以 Held 触发的攻击，例如重击。
	// 2. 在闪反升级窗口里，从普通攻击提升为 DodgeCounter 的特殊回放。
	return IsDeferredAttackRequestStillValid(AttackRequestTag)
		&& (TriggerInputEvent == EActionInputEvent::Held
			|| (AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter
				&& TriggerInputEvent == EActionInputEvent::Pressed));
}

bool UHeroAttackComponent::TryReplayHeldAttackInputAfterRecovery()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent)
	{
		return false;
	}

	const FGameplayTag AttackInputTag = ActionGameplayTags::InputTag_GameplayAbility_Attack;
	FActionInputRuntimeStateEntry InputState;
	if (!CombatInputComponent->TryGetInputRuntimeStateEntry(AttackInputTag, InputState)
		|| InputState.ButtonState != EActionInputButtonState::Held
		|| InputState.bIsConsumed
		|| !CanReplayHeldAttackInput(AttackInputTag))
	{
		return false;
	}

	// Held 回放不是读取缓冲，而是直接读取“当前这次按住输入仍然活着”的运行时状态，
	// 适合重击这类已经在 Held 阶段成立、但因为硬锁延迟到恢复首帧才真正补发的情况。
	FGameplayTag AttackRequestTag;
	EActionInputEvent TriggerInputEvent = EActionInputEvent::Pressed;
	if (!TryResolveHeldReplayAttackSpec(AttackInputTag, AttackRequestTag, TriggerInputEvent))
	{
		return false;
	}

	// 回放时关闭缓冲标记，表示这次是“恢复链补发”而不是新的缓存写入。
	return CombatComponent->ProcessAbilityInput(
		AttackInputTag,
		TriggerInputEvent,
		false,
		AttackRequestTag);
}

bool UHeroAttackComponent::TryResolveBufferedAttackConsumeSpec(
	const FActionBufferedInput& BufferedInput,
	FGameplayTag& OutAttackRequestTag,
	EActionInputEvent& OutTriggerInputEvent) const
{
	OutAttackRequestTag = FGameplayTag();
	OutTriggerInputEvent = BufferedInput.TriggerEvent;

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CurrentWeaponDefinition || BufferedInput.InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	if (ShouldPromoteAttackInputToDodgeCounter(BufferedInput.InputTag))
	{
		// 完美闪避成功后，闪避期间缓存下来的普通攻击输入允许升级成闪避反击。
		// 这里的优先级高于沿用旧请求，因为闪反资格本身就是一种“恢复后应重解释”的临时战斗语义。
		OutAttackRequestTag = ActionGameplayTags::Attack_Request_DodgeCounter;
		OutTriggerInputEvent = CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(OutAttackRequestTag);
		return IsDeferredAttackRequestStillValid(OutAttackRequestTag);
	}

	OutAttackRequestTag = BufferedInput.ResolvedAttackRequestTag.IsValid()
		? BufferedInput.ResolvedAttackRequestTag
		: ResolveAttackRequestTag(BufferedInput.InputTag);
	// 普通缓冲恢复优先沿用“写入缓冲时就已经解析好的请求”，
	// 这样不会因为角色在等待期间切状态而把原来的攻击意图重新解释成另一招。
	OutTriggerInputEvent = BufferedInput.TriggerEvent;
	return IsDeferredAttackRequestStillValid(OutAttackRequestTag);
}

bool UHeroAttackComponent::TryResolveHeldReplayAttackSpec(
	const FGameplayTag& InputTag,
	FGameplayTag& OutAttackRequestTag,
	EActionInputEvent& OutTriggerInputEvent) const
{
	OutAttackRequestTag = FGameplayTag();
	OutTriggerInputEvent = EActionInputEvent::Pressed;

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CombatComponent || !CombatInputComponent || !CurrentWeaponDefinition || InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	if (ShouldPromoteAttackInputToDodgeCounter(InputTag))
	{
		// 闪反窗口开启后，Held 重放同样要优先升级成闪避反击。
		OutAttackRequestTag = ActionGameplayTags::Attack_Request_DodgeCounter;
		OutTriggerInputEvent = CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(OutAttackRequestTag);
		return IsDeferredAttackRequestStillValid(OutAttackRequestTag);
	}

	// Held 回放优先读取“这次按住期间已经锁存好的攻击请求”，
	// 只有锁存不存在时，才退回到按当前状态重新解析。
	OutAttackRequestTag = CombatInputComponent->GetLatchedAttackRequestTagByInputTag(InputTag);
	if (!OutAttackRequestTag.IsValid())
	{
		OutAttackRequestTag = ResolveAttackRequestTag(InputTag);
	}

	// Held 回放和缓冲恢复不同，它要尽量基于输入锁存结果继续推进，
	// 这样重击等“长按已判定成立”的请求在恢复首帧仍能回到原来的攻击分支。
	OutTriggerInputEvent = CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(OutAttackRequestTag);
	return IsDeferredAttackRequestStillValid(OutAttackRequestTag);
}

FGameplayTag UHeroAttackComponent::PrepareAttackRequestTagForEvent(
	const FGameplayTag& InputTag,
	EActionInputEvent InputEvent,
	FGameplayTag ResolvedAttackRequestTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return ResolvedAttackRequestTag;
	}

	if (ResolvedAttackRequestTag.IsValid())
	{
		// 外部如果已经在更早阶段解析好了请求，这里直接沿用，
		// 避免在同一条输入链上因为状态瞬时变化而重复解析出不同招式。
		return ResolvedAttackRequestTag;
	}

	if (InputEvent == EActionInputEvent::Released)
	{
		// Released 优先复用 Held 阶段锁存下来的攻击请求，
		// 保证“按下时已决定攻击类型、松手时再真正触发”的分支不会重新解析跑偏。
		ResolvedAttackRequestTag = CombatInputComponent->GetLatchedAttackRequestTagByInputTag(InputTag);
	}

	if (!ResolvedAttackRequestTag.IsValid())
	{
		ResolvedAttackRequestTag = ResolveAttackRequestTag(InputTag);
	}

	if (InputEvent == EActionInputEvent::Held
		&& ResolvedAttackRequestTag.IsValid())
	{
		// Held 阶段一旦解析出明确攻击请求，就先把结果锁存起来，
		// 给后续 Released 触发和恢复链回放复用同一份攻击意图。
		CombatInputComponent->SetLatchedAttackRequestTagByInputTag(InputTag, ResolvedAttackRequestTag);
	}

	return ResolvedAttackRequestTag;
}

bool UHeroAttackComponent::ProcessAttackInput(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag,
	const EActionInputEvent InputEvent,
	const bool bCanBuffer,
	FGameplayTag ResolvedAttackRequestTag)
{
	if (InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	// 攻击输入入口统一先补齐“这次输入到底想出哪一招”，
	// 后续无论即时处理还是写入缓冲，都使用同一份解析结果。
	ResolvedAttackRequestTag = PrepareAttackRequestTagForEvent(
		InputTag,
		InputEvent,
		ResolvedAttackRequestTag);

	// 第一优先级永远是“当帧直接出招”。
	// 只有直接处理失败，才允许回退到输入缓冲链。
	if (TryHandleAttackInputByEvent(
		InActionASC,
		InputTag,
		InputEvent,
		ResolvedAttackRequestTag))
	{
		return true;
	}

	// 只有即时处理失败时才回退到缓冲链，避免同一帧既出招又重复缓存。
	TryQueueBufferedAttackInput(
		InputTag,
		InputEvent,
		bCanBuffer,
		ResolvedAttackRequestTag);
	return false;
}

bool UHeroAttackComponent::TryHandleAttackInputByEvent(
	UActionAbilitySystemComponent* InActionASC,
	const FGameplayTag& InputTag,
	EActionInputEvent InputEvent,
	FGameplayTag ResolvedAttackRequestTag)
{
	if (InputTag != ActionGameplayTags::InputTag_GameplayAbility_Attack)
	{
		return false;
	}

	// 当前组件只在这里集中分发攻击输入，方便以后继续扩展不同事件阶段的专门处理分支。
	return HandleAttackInput(InActionASC, InputTag, InputEvent, ResolvedAttackRequestTag);
}

bool UHeroAttackComponent::HandleAttackInput(
	UActionAbilitySystemComponent* InActionASC,
	FGameplayTag InputTag,
	EActionInputEvent InputEvent,
	FGameplayTag ResolvedAttackRequestTag)
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !InActionASC)
	{
		return false;
	}

	FGameplayTag AttackRequestTag = ResolvedAttackRequestTag;
	if (!AttackRequestTag.IsValid())
	{
		if (InputEvent == EActionInputEvent::Released)
		{
			// Released 分支优先使用之前锁存的请求，
			// 这样重击这类“按住时决定攻击类型、松手时真正检查触发”的输入不会重新走错分支。
			AttackRequestTag = CombatInputComponent->GetLatchedAttackRequestTagByInputTag(InputTag);
		}

		if (!AttackRequestTag.IsValid())
		{
			AttackRequestTag = ResolveAttackRequestTag(InputTag);
		}
	}

	if (InputEvent == EActionInputEvent::Released
		&& !IsDeferredAttackRequestStillValid(AttackRequestTag))
	{
		// 如果这条延迟到 Released 的攻击请求已经失效，就直接终止，
		// 避免在玩家状态变化后把一条过时输入硬塞进当前武器分支。
		return false;
	}

	if (InputEvent == EActionInputEvent::Held
		&& AttackRequestTag.IsValid())
	{
		// Held 成立后再次锁存，保证后续 Released / 恢复链读取到的是同一份请求。
		CombatInputComponent->SetLatchedAttackRequestTagByInputTag(InputTag, AttackRequestTag);
	}

	EActionInputEvent TriggerInputEvent = InputEvent;
	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition())
	{
		// 武器定义决定这条攻击真正应该在哪个输入阶段触发。
		TriggerInputEvent = CurrentWeaponDefinition->ResolveAttackTriggerInputEvent(AttackRequestTag);
	}

	if (TriggerInputEvent != InputEvent)
	{
		if (InputEvent == EActionInputEvent::Released
			&& TriggerInputEvent == EActionInputEvent::Held)
		{
			// 这说明当前这次攻击请求已经在 Held 阶段成立过。
			// Released 阶段不应再把它当成一次新的输入去失败、缓冲或回退成轻击，
			// 直接吞掉这次释放即可，让之前那次已经成立的 Held 意图继续保留。
			return true;
		}

		// 请求虽然存在，但它真正应该触发的阶段不是当前阶段。
		// 这里直接返回 false，让上层决定是否写入缓冲或等待后续阶段。
		return false;
	}

	if (!CanActivateAttackInputNow(InputTag))
	{
		return false;
	}

	const bool bAttackTriggeredFromCancelWindow =
		CombatComponent->IsAbilityCancelContextActive()
		&& CombatComponent->IsAbilityCancelInputAllowedNow(InputTag);
	const bool bAttackTriggeredFromWeaponSwitchPresentation =
		CombatComponent->IsWeaponSwitchPresentationChainInputAllowed(InputTag)
		|| CombatComponent->IsWeaponSwitchPresentationCancelInputAllowed(InputTag);

	if (bAttackTriggeredFromCancelWindow || bAttackTriggeredFromWeaponSwitchPresentation)
	{
		// 攻击若从取消窗口或切武表现窗口抢入，
		// 需要先把当前活动 Ability 正式结束，再提交新的 Attack。
		// 否则 ASC 关系系统仍会看到旧 Ability 的 ActivationOwnedTags，
		// 从而把这次攻击拦成“被活动 Ability 阻挡”。
		InActionASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_Dodge);
		InActionASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_CombatModeOrDefense);
		InActionASC->CancelAbilityByAbilityTag(ActionGameplayTags::Player_Ability_WeaponSwitch);

		Debug::Print(TEXT("[攻击] 抢占前先结束闪避/防御/切武，再提交攻击"), FColor::Yellow, 2.0f);
	}

	// 先把攻击请求映射到当前武器槽对应的攻击能力输入标签，再交给 ASC 激活。
	const FGameplayTag RoutedInputTag = ResolveScopedAttackAbilityInputTag(AttackRequestTag);
	if (!RoutedInputTag.IsValid())
	{
		return false;
	}

	// 记录当前是否正处于可接段攻击链。
	// 只有在“旧段还活着”的情况下，才需要为这次新输入临时保留连段索引。
	const bool bWasAttackChainRelevant = IsAttackChainRelevantState();
	if (bWasAttackChainRelevant)
	{
		// 连段接续时先标记“本次结束后保留连段索引”，
		// 只有后面能力真正没触发成功时才回滚，避免接段失败时把索引错误推进。
		MarkPreserveComboIndexOnNextAttackFinalize();
	}

	// 同一时刻只允许一个攻击能力主导演出与窗口，新的攻击输入进来前先清理旧的活动攻击能力。
	CancelActiveAttackAbilityIfNeeded(InActionASC);

	const bool bShouldAuthorizePendingDodgeCounter =
		AttackRequestTag == ActionGameplayTags::Attack_Request_DodgeCounter
		&& (CombatComponent->IsDodgeCounterAvailable() || HasPendingDodgeCounterExecutionAuthorization());
	if (bShouldAuthorizePendingDodgeCounter)
	{
		// 闪避反击在真正激活前先挂起一次授权，
		// 让后续执行链能够区分“普通攻击输入”与“已被批准的闪反请求”。
		SetPendingDodgeCounterExecutionAuthorization(true);
	}

	// 真正把攻击请求转换成当前槽位专属的 GA 输入。
	// Held 请求走 Held 入口，其余请求统一按 Pressed 入口提交给 ASC。
	bool bAttackTriggered = false;
	if (InputEvent == EActionInputEvent::Held)
	{
		bAttackTriggered = InActionASC->OnAbilityInputHeld(RoutedInputTag);
	}
	else
	{
		bAttackTriggered = InActionASC->OnAbilityInputPressed(RoutedInputTag);
	}

	if (!bAttackTriggered)
	{
		if (bShouldAuthorizePendingDodgeCounter)
		{
			// 能力没有成功起手时，必须把这次闪反授权撤回，避免脏授权污染后续输入。
			SetPendingDodgeCounterExecutionAuthorization(false);
		}

		if (bWasAttackChainRelevant
			&& ConsumePreserveComboIndexOnNextAttackFinalize())
		{
			// 接段失败时把“预保留的连段索引”撤销，恢复到原先的稳定连段状态。
			CombatComponent->ResetComboIndex();
		}

		return false;
	}

	// 只有攻击能力真正触发成功后，才把这次原始攻击输入标记为已消费。
	CombatInputComponent->MarkInputConsumedByTag(InputTag);
	return true;
}

void UHeroAttackComponent::RequestRecoverCombatInputAfterAttack()
{
	if (bDeferredCombatInputRecoveryAfterAttackRequested || !GetWorld())
	{
		return;
	}

	// 放到下一帧恢复输入，避免与本帧攻击收尾、窗口关闭、受击状态收束发生时序冲突。
	bDeferredCombatInputRecoveryAfterAttackRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredCombatInputRecoveryAfterAttack);
}

void UHeroAttackComponent::RequestConsumeBufferedAttackInputOnNextTick()
{
	if (bDeferredBufferedAttackInputConsumeRequested || !GetWorld())
	{
		return;
	}

	// 缓冲消费同样延迟到下一帧执行，确保上一条能力的状态清理已经完整落地。
	bDeferredBufferedAttackInputConsumeRequested = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::HandleDeferredBufferedAttackInputConsume);
}

void UHeroAttackComponent::HandleDeferredCombatInputRecoveryAfterAttack()
{
	UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	UHeroCombatInputComponent* CombatInputComponent = GetOwningHeroCombatInputComponent();
	if (!CombatComponent || !CombatInputComponent || !bDeferredCombatInputRecoveryAfterAttackRequested)
	{
		return;
	}

	// 先撤销“已登记下一帧恢复”的本地标记，
	// 再调用总控恢复输入，避免恢复过程中再次进入这里时出现重复登记。
	bDeferredCombatInputRecoveryAfterAttackRequested = false;
	// 这里复用战斗总控的恢复入口，统一让受击、攻击、取消窗口等输入门禁回到正常状态。
	CombatInputComponent->RecoverCombatInputAfterCombatReact();
}

void UHeroAttackComponent::HandleDeferredBufferedAttackInputConsume()
{
	if (!bDeferredBufferedAttackInputConsumeRequested)
	{
		return;
	}

	// 同样先清标记，再尝试消费，
	// 保证这次补发即使失败，也不会因为标记残留而卡住后续新的缓冲恢复请求。
	bDeferredBufferedAttackInputConsumeRequested = false;
	// 下一帧只尝试消费一次，真正能不能出招仍由缓冲消费判定函数再次把关。
	TryConsumeBufferedAttackInput();
}

void UHeroAttackComponent::ClearDeferredAttackRequests()
{
	// 这里只清组件内部维护的“下一帧延迟请求”标记，
	// 不直接碰 ASC 或输入状态表，避免把其它系统的运行时状态一并误清掉。
	bDeferredCombatInputRecoveryAfterAttackRequested = false;
	bDeferredBufferedAttackInputConsumeRequested = false;
}

void UHeroAttackComponent::ResetRuntimeStateForHeroStartup()
{
	// Hero 启动或重置时，把攻击组件的局部运行时状态恢复到干净初始值，
	// 避免上一局残留的命中配置、连段保留标记或闪反授权串到新一轮流程。
	ClearCurrentAttackHitConfig();
	ClearCurrentAttackProjectileSpawnConfig();
	bPreserveComboIndexOnNextAttackFinalize = false;
	bPendingDodgeCounterExecutionAuthorization = false;
	ClearDeferredAttackRequests();
}

UHeroCombatComponent* UHeroAttackComponent::GetOwningHeroCombatComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		// 攻击组件只通过角色拿总控组件，不自己缓存裸指针，
		// 这样角色重建或组件装配调整后，访问入口仍保持单一。
		return HeroCharacter->GetHeroCombatComponent();
	}

	return nullptr;
}

UHeroCombatInputComponent* UHeroAttackComponent::GetOwningHeroCombatInputComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->GetHeroCombatInputComponent();
	}

	return nullptr;
}

UHeroLoadoutContextComponent* UHeroAttackComponent::GetOwningHeroLoadoutContextComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->FindComponentByClass<UHeroLoadoutContextComponent>();
	}

	return nullptr;
}

UHeroLoadoutEffectComponent* UHeroAttackComponent::GetOwningHeroLoadoutEffectComponent() const
{
	if (const AActionHeroCharacter* HeroCharacter = GetOwningHeroCharacter())
	{
		return HeroCharacter->FindComponentByClass<UHeroLoadoutEffectComponent>();
	}

	return nullptr;
}

AActionHeroCharacter* UHeroAttackComponent::GetOwningHeroCharacter() const
{
	return Cast<AActionHeroCharacter>(GetOwner());
}

UDataAsset_WeaponDefinition* UHeroAttackComponent::GetCurrentWeaponDefinition() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		// 当前攻击逻辑始终以“当前战斗武器定义”为唯一数据源，
		// 不再在攻击组件里维护第二份武器配置缓存。
		return CombatComponent->GetCurrentWeaponDefinition();
	}

	return nullptr;
}

AHeroWeaponBase* UHeroAttackComponent::GetCurrentEquippedWeapon() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		// 命中检测、伤害来源和朝向计算都依赖当前已实例化并装备中的武器。
		return CombatComponent->GetCurrentEquippedWeapon();
	}

	return nullptr;
}

UActionAttributeSetBase* UHeroAttackComponent::GetOwningActionAttributeSet() const
{
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		// 伤害载荷统一从角色属性集读取攻击力等实时数值，
		// 保证后续属性 Buff / Debuff 对攻击结算天然生效。
		return CombatComponent->GetOwningActionAttributeSet();
	}

	return nullptr;
}

float UHeroAttackComponent::GetResolvedAttackPowerForCurrentLoadout() const
{
	const UActionAttributeSetBase* OwnerAttributeSet = GetOwningActionAttributeSet();
	if (!OwnerAttributeSet)
	{
		return 0.f;
	}

	float ResolvedAttackPower = FMath::Max(ActionRoundBaseValueToInteger(OwnerAttributeSet->GetAttackPower()), 0.f);

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return ResolvedAttackPower;
	}

	const UHeroEquipmentComponent* EquipmentComponent = CombatComponent->GetOwningHeroEquipmentComponent();
	const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent();
	if (!EquipmentComponent || !LoadoutContextComponent)
	{
		return ResolvedAttackPower;
	}

	FActionWeaponAttributeCacheData WeaponAttributeCache;
	if (LoadoutContextComponent->GetWeaponAttributeCacheByLoadoutSlot(
		EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
		WeaponAttributeCache))
	{
		ResolvedAttackPower += ActionRoundBaseValueToInteger(WeaponAttributeCache.AttackPowerBonus);
	}

	return FMath::Max(ActionRoundBaseValueToInteger(ResolvedAttackPower), 0.f);
}

bool UHeroAttackComponent::ResolveCurrentAttackSourceHitConfig(FActionWeaponHitConfig& OutHitConfig) const
{
	if (bHasCurrentAttackHitConfig)
	{
		OutHitConfig = CurrentAttackHitConfig;
		return true;
	}

	if (const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition())
	{
		return CurrentWeaponDefinition->TryResolveAttackHitConfigByRequestTag(
			ActionGameplayTags::Attack_Request_Default,
			OutHitConfig);
	}

	OutHitConfig = FActionWeaponHitConfig();
	return false;
}

bool UHeroAttackComponent::TryGetCurrentAttackProjectileDebugInfo(
	FActionProjectileResolutionDebugInfo& OutDebugInfo) const
{
	OutDebugInfo = FActionProjectileResolutionDebugInfo();
	OutDebugInfo.bHasProjectileSpawnRequest = bHasCurrentAttackProjectileSpawnConfig;
	if (!bHasCurrentAttackProjectileSpawnConfig)
	{
		return false;
	}

	OutDebugInfo.SpawnSocketName = CurrentAttackProjectileSpawnConfig.SpawnSocketName;
	OutDebugInfo.bUseWeaponDefaultProjectileConfig =
		CurrentAttackProjectileSpawnConfig.bUseWeaponDefaultProjectileConfig;

	FActionProjectileConfig ResolvedProjectileConfig;
	if (!ResolveCurrentAttackProjectileConfig(
		ResolvedProjectileConfig,
		&OutDebugInfo.ResolvedConfigSource,
		&OutDebugInfo.SelectedProjectileConfigTag))
	{
		return false;
	}

	OutDebugInfo.bResolvedSuccessfully = true;
	OutDebugInfo.ResolvedProjectileTag = ResolvedProjectileConfig.ProjectileTag;
	return true;
}

bool UHeroAttackComponent::ResolveCurrentAttackProjectileConfig(
	FActionProjectileConfig& OutProjectileConfig,
	EActionResolvedProjectileConfigSource* OutResolvedSource,
	FGameplayTag* OutSelectedProjectileConfigTag) const
{
	OutProjectileConfig = FActionProjectileConfig();
	if (OutResolvedSource)
	{
		*OutResolvedSource = EActionResolvedProjectileConfigSource::None;
	}
	if (OutSelectedProjectileConfigTag)
	{
		*OutSelectedProjectileConfigTag = FGameplayTag();
	}

	if (!bHasCurrentAttackProjectileSpawnConfig)
	{
		return false;
	}

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	if (!CurrentWeaponDefinition)
	{
		return false;
	}

	FGameplayTag SelectedProjectileConfigTag;
	if (const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		if (const UHeroEquipmentComponent* EquipmentComponent = CombatComponent->GetOwningHeroEquipmentComponent())
		{
			if (const UHeroLoadoutContextComponent* LoadoutContextComponent = GetOwningHeroLoadoutContextComponent())
			{
				LoadoutContextComponent->GetLoadoutSlotSelectedProjectileConfigTag(
					EquipmentComponent->GetCurrentEquippedLoadoutSlot(),
					SelectedProjectileConfigTag);
			}
		}
	}
	if (OutSelectedProjectileConfigTag)
	{
		*OutSelectedProjectileConfigTag = SelectedProjectileConfigTag;
	}

	return CurrentWeaponDefinition->ResolveProjectileConfigForSpawn(
		CurrentAttackProjectileSpawnConfig,
		SelectedProjectileConfigTag,
		OutProjectileConfig,
		OutResolvedSource);
}

FTransform UHeroAttackComponent::ResolveCurrentAttackProjectileSpawnTransform(
	const FActionProjectileSpawnConfig& InProjectileSpawnConfig) const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return FTransform::Identity;
	}

	const FName SpawnSocketName = InProjectileSpawnConfig.SpawnSocketName;
	if (SpawnSocketName != NAME_None)
	{
		if (USkeletalMeshComponent* OwnerMesh = OwnerHeroCharacter->GetMesh())
		{
			if (OwnerMesh->DoesSocketExist(SpawnSocketName))
			{
				return OwnerMesh->GetSocketTransform(SpawnSocketName, RTS_World);
			}
		}

		Debug::Print(FString::Printf(
			TEXT("[发射物] 角色骨骼未找到 Socket=%s，将回退到武器或角色 Transform"),
			*SpawnSocketName.ToString()),
			FColor::Yellow,
			1.5f);
	}

	if (const AHeroWeaponBase* CurrentWeapon = GetCurrentEquippedWeapon())
	{
		return CurrentWeapon->GetActorTransform();
	}

	return OwnerHeroCharacter->GetActorTransform();
}

void UHeroAttackComponent::FillCurrentAttackHitSourceInfo(
	FActionHitSourceInfo& InOutHitSourceInfo,
	const FName InPreferredHitSourceId,
	const FName InPreferredSourceComponentName) const
{
	InOutHitSourceInfo = FActionHitSourceInfo();

	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	const UHeroHitSourceComponent* HitSourceComponent =
		CombatComponent ? CombatComponent->GetOwningHeroHitSourceComponent() : nullptr;
	const AHeroWeaponBase* CurrentWeapon = GetCurrentEquippedWeapon();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
	const FGameplayTag CurrentWeaponTag = CurrentWeapon
		? CurrentWeapon->GetWeaponTag()
		: (CurrentWeaponDefinition ? CurrentWeaponDefinition->WeaponTag : FGameplayTag());

	auto ApplyWeaponTagContext = [&InOutHitSourceInfo, &CurrentWeaponTag]()
	{
		if (!CurrentWeaponTag.IsValid())
		{
			return;
		}

		InOutHitSourceInfo.WeaponTag = CurrentWeaponTag;
		if (!InOutHitSourceInfo.SourceTag.IsValid())
		{
			InOutHitSourceInfo.SourceTag = CurrentWeaponTag;
		}
	};

	auto FillFallbackSource = [&InOutHitSourceInfo](
		const FName InSourceId,
		const EActionHitSourceType InSourceType,
		const FName InSourceComponentName)
	{
		InOutHitSourceInfo.SourceId = InSourceId;
		InOutHitSourceInfo.SourceType = InSourceType;
		InOutHitSourceInfo.SourceComponentName = InSourceComponentName;
	};

	if (HitSourceComponent && InPreferredHitSourceId != NAME_None)
	{
		if (!HitSourceComponent->TryFillHitSourceInfoFromRegistration(InPreferredHitSourceId, InOutHitSourceInfo))
		{
			EActionHitSourceType FallbackSourceType = EActionHitSourceType::CharacterBody;
			FName ResolvedSourceIdByComponentName = NAME_None;
			if (InPreferredSourceComponentName != NAME_None
				&& HitSourceComponent->TryResolveRegisteredHitSourceIdByComponentName(
					InPreferredSourceComponentName,
					EActionHitSourceType::WeaponCollision,
					ResolvedSourceIdByComponentName))
			{
				FallbackSourceType = EActionHitSourceType::WeaponCollision;
			}
			else if (CurrentWeapon
				&& InPreferredHitSourceId == ActionHitSourceDefaults::GetWeaponCollisionSourceId())
			{
				FallbackSourceType = EActionHitSourceType::WeaponCollision;
			}

			FillFallbackSource(
				InPreferredHitSourceId,
				FallbackSourceType,
				InPreferredSourceComponentName);
		}

		if (InPreferredSourceComponentName != NAME_None)
		{
			InOutHitSourceInfo.SourceComponentName = InPreferredSourceComponentName;
		}

		ApplyWeaponTagContext();

		if (CombatComponent)
		{
			InOutHitSourceInfo.LoadoutSlot = CombatComponent->GetCurrentCombatLoadoutSlot();
		}
		return;
	}

	// 当前真正采用哪个来源，优先取决于命中窗口当前激活了哪类来源，
	// 而不是简单地根据“角色现在手里有没有武器”来猜。
	FActionHitSourceDefinition ActiveWeaponHitSourceDefinition;
	const bool bHasActiveWeaponHitSource = HitSourceComponent
		&& HitSourceComponent->TryGetFirstActiveHitSourceByType(
			EActionHitSourceType::WeaponCollision,
			ActiveWeaponHitSourceDefinition);

	FActionHitSourceDefinition ActiveBodyHitSourceDefinition;
	const bool bHasActiveBodyHitSource = HitSourceComponent
		&& HitSourceComponent->TryGetFirstActiveHitSourceByType(
			EActionHitSourceType::CharacterBody,
			ActiveBodyHitSourceDefinition);

	if (HitSourceComponent
		&& !HitSourceComponent->ShouldUseWeaponCollisionDetection()
		&& bHasActiveBodyHitSource)
	{
		if (!HitSourceComponent->TryFillHitSourceInfoFromRegistration(
			ActiveBodyHitSourceDefinition.SourceId,
			InOutHitSourceInfo))
		{
			FillFallbackSource(
				ActionHitSourceDefaults::GetCharacterBodySourceId(),
				EActionHitSourceType::CharacterBody,
				TEXT("CharacterBody"));
		}

		ApplyWeaponTagContext();
	}
	else if (HitSourceComponent
		&& HitSourceComponent->ShouldUseWeaponCollisionDetection()
		&& bHasActiveWeaponHitSource)
	{
		if (!HitSourceComponent->TryFillHitSourceInfoFromRegistration(
			ActiveWeaponHitSourceDefinition.SourceId,
			InOutHitSourceInfo))
		{
			FillFallbackSource(
				ActionHitSourceDefaults::GetWeaponCollisionSourceId(),
				EActionHitSourceType::WeaponCollision,
				TEXT("WeaponCollisionBox"));
		}

		ApplyWeaponTagContext();
	}
	else if (CurrentWeapon)
	{
		const FName SourceId = ActionHitSourceDefaults::GetWeaponCollisionSourceId();
		if (!(HitSourceComponent
			&& HitSourceComponent->TryFillHitSourceInfoFromRegistration(SourceId, InOutHitSourceInfo)))
		{
			FillFallbackSource(
				SourceId,
				EActionHitSourceType::WeaponCollision,
				TEXT("WeaponCollisionBox"));
		}

		ApplyWeaponTagContext();
	}
	else
	{
		// 空手状态先统一落成“角色身体来源”。
		// 后续真正接入多身体命中体时，只需要在这里继续细化 SourceTag / ComponentName。
		const FName SourceId = ActionHitSourceDefaults::GetCharacterBodySourceId();
		if (!(HitSourceComponent
			&& HitSourceComponent->TryFillHitSourceInfoFromRegistration(SourceId, InOutHitSourceInfo)))
		{
			FillFallbackSource(
				SourceId,
				EActionHitSourceType::CharacterBody,
				TEXT("CharacterBody"));
		}

		ApplyWeaponTagContext();
	}

	if (CombatComponent)
	{
		InOutHitSourceInfo.LoadoutSlot = CombatComponent->GetCurrentCombatLoadoutSlot();
	}
}

bool UHeroAttackComponent::IsAttackChainRelevantState() const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent)
	{
		return false;
	}

	if (CombatComponent->GetCombatMode() != EHeroCombatMode::Combo
		|| CombatComponent->GetCurrentRunningAnimMontage() == nullptr)
	{
		// 没有进入 Combo 模式或当前没有正在驱动窗口的蒙太奇时，不认为处于可接段的攻击链状态。
		return false;
	}

	if (const UActionAbilitySystemComponent* OwnerASC = CombatComponent->GetOwningActionAbilitySystemComponent())
	{
		// 最终还要以攻击能力激活标签兜底确认，防止只有蒙太奇引用但攻击能力实际上已结束的假链状态。
		return OwnerASC->HasMatchingGameplayTag(ActionGameplayTags::State_Ability_Attack_Active);
	}

	return false;
}

bool UHeroAttackComponent::IsDeferredAttackRequestStillValid(const FGameplayTag& AttackRequestTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	if (!CombatComponent || !AttackRequestTag.IsValid())
	{
		return true;
	}

	if (AttackRequestTag == ActionGameplayTags::Attack_Request_Airborne)
	{
		// 空中攻击是最典型的“延迟后可能失效”的请求：
		// 如果角色恢复到地面，再去补发之前缓存的空中攻击就已经不合理了。
		return CombatComponent->IsInNormalFallingState();
	}

	return true;
}

void UHeroAttackComponent::CancelActiveAttackAbilityIfNeeded(UActionAbilitySystemComponent* InActionASC) const
{
	if (UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent())
	{
		// 实际取消入口仍交给战斗总控，保证攻击组件只负责攻击语义，
		// 而能力取消的共用规则继续留在总控统一维护。
		CombatComponent->CancelActiveAttackAbilityIfNeeded(InActionASC);
	}
}

bool UHeroAttackComponent::ShouldPromoteAttackInputToDodgeCounter(const FGameplayTag& InputTag) const
{
	const UHeroCombatComponent* CombatComponent = GetOwningHeroCombatComponent();
	// 这里只有地面普通攻击输入才允许升级成闪避反击，
	// 这样不会把空中攻击或其它专用输入错误抬升到 DodgeCounter 分支。
	return InputTag == ActionGameplayTags::InputTag_GameplayAbility_Attack
		&& CombatComponent
		&& !CombatComponent->IsInNormalFallingState()
		&& CombatComponent->IsDodgeCounterAvailable();
}
