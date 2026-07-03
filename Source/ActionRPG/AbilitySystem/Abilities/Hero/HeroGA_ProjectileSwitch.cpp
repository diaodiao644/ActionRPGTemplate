// File: Projectile switch ability runtime logic.

#include "AbilitySystem/Abilities/Hero/HeroGA_ProjectileSwitch.h"

#include "ActionGameplayTags.h"
#include "ActionType/ActionProjectileTypes.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"

namespace HeroProjectileSwitchAbility
{
	static FString GetDefaultProjectileDisplayText()
	{
		return TEXT("\u9ed8\u8ba4\u53d1\u5c04\u7269");
	}

	static bool ResolveNextProjectileConfigTag(
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		const FGameplayTag& InCurrentSelectedTag,
		FGameplayTag& OutNextProjectileConfigTag,
		FString& OutNextDisplayText)
	{
		OutNextProjectileConfigTag = FGameplayTag();
		OutNextDisplayText = GetDefaultProjectileDisplayText();

		if (!InWeaponDefinition || !InWeaponDefinition->SupportsProjectileSwitching())
		{
			return false;
		}

		TArray<FGameplayTag> SwitchableProjectileTags;
		SwitchableProjectileTags.Reserve(InWeaponDefinition->SwitchableProjectileConfigs.Num());

		for (const FActionSwitchableProjectileConfigEntry& ConfigEntry : InWeaponDefinition->SwitchableProjectileConfigs)
		{
			if (ConfigEntry.ProjectileConfigTag.IsValid() && ConfigEntry.ProjectileConfig.IsValidConfig())
			{
				SwitchableProjectileTags.Add(ConfigEntry.ProjectileConfigTag);
			}
		}

		if (SwitchableProjectileTags.Num() <= 0)
		{
			return false;
		}

		int32 CurrentIndex = INDEX_NONE;
		if (InCurrentSelectedTag.IsValid())
		{
			CurrentIndex = SwitchableProjectileTags.IndexOfByKey(InCurrentSelectedTag);
		}

		const int32 NextIndex = CurrentIndex + 1;
		if (SwitchableProjectileTags.IsValidIndex(NextIndex))
		{
			OutNextProjectileConfigTag = SwitchableProjectileTags[NextIndex];
			OutNextDisplayText = OutNextProjectileConfigTag.ToString();
			return true;
		}

		OutNextProjectileConfigTag = FGameplayTag();
		OutNextDisplayText = GetDefaultProjectileDisplayText();
		return true;
	}
}

UHeroGA_ProjectileSwitch::UHeroGA_ProjectileSwitch()
	: Super()
{
	ActivationPolicy = EActionAbilityActivationPolicy::OnInput;

	AbilityTags.AddTag(ActionGameplayTags::Player_Ability_ProjectileSwitch);

	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Attack_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Defense_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Dodge_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_Execution_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_SpiritSkill_Active);
	ActivationBlockedTags.AddTag(ActionGameplayTags::State_Ability_WeaponSwitch_Active);

	AbilityPriority = 8;
	bCanInterruptLowerPriorityAbilities = false;
	bCanInterruptSamePriorityAbilities = false;
	bCanBeInterruptedByHigherPriority = true;
	bCanBeInterruptedBySamePriority = false;
}

bool UHeroGA_ProjectileSwitch::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	if (!ValidateHeroRuntimeObjects(OutFailureReason, false, true))
	{
		return false;
	}

	const UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();

	const UHeroDefenseComponent* HeroDefenseComponent = GetHeroDefenseComponentFromActorInfo();
	if (!HeroDefenseComponent)
	{
		OutFailureReason = TEXT("hero defense component is invalid");
		return false;
	}

	if (!HeroDefenseComponent->CanActivateNonAttackInputNow(ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch))
	{
		// ProjectileSwitch 继续维持最低层便捷动作，不单独越权绕过非攻击输入门禁。
		// 这样它在受击、空中、切武表现期和取消窗口白名单下的失败语义，都能统一回到关系系统。
		OutFailureReason = HeroCombatComponent
			? HeroCombatComponent->DescribeNonAttackInputGateForDebug(ActionGameplayTags::InputTag_GameplayAbility_ProjectileSwitch)
			: TEXT("projectile switch input is blocked by current combat state");
		return false;
	}

	const UHeroEquipmentComponent* HeroEquipmentComponent =
		HeroCombatComponent ? HeroCombatComponent->GetOwningHeroEquipmentComponent() : nullptr;
	const UHeroLoadoutContextComponent* HeroLoadoutContextComponent = GetHeroLoadoutContextComponentFromActorInfo();
	const UHeroLoadoutRuntimeComponent* HeroLoadoutRuntimeComponent = GetHeroLoadoutRuntimeComponentFromActorInfo();
	if (!HeroEquipmentComponent || !HeroLoadoutContextComponent || !HeroLoadoutRuntimeComponent)
	{
		OutFailureReason = TEXT("hero equipment/context/runtime component is invalid");
		return false;
	}

	const EHeroWeaponLoadoutSlot CurrentLoadoutSlot = HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot();
	if (CurrentLoadoutSlot == EHeroWeaponLoadoutSlot::Invalid)
	{
		OutFailureReason = TEXT("current equipped loadout slot is invalid");
		return false;
	}

	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition =
		HeroLoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(
			CurrentLoadoutSlot);
	if (!CurrentWeaponDefinition)
	{
		OutFailureReason = TEXT("current weapon definition is invalid");
		return false;
	}

	if (!CurrentWeaponDefinition->SupportsProjectileSwitching())
	{
		OutFailureReason = TEXT("current weapon does not support projectile switching");
		return false;
	}

	if (CurrentWeaponDefinition->SwitchableProjectileConfigs.Num() <= 0)
	{
		OutFailureReason = TEXT("current weapon has no switchable projectile configs");
		return false;
	}

	FGameplayTag CurrentSelectedProjectileConfigTag;
	HeroLoadoutContextComponent->GetLoadoutSlotSelectedProjectileConfigTag(
		CurrentLoadoutSlot,
		CurrentSelectedProjectileConfigTag);

	FGameplayTag NextProjectileConfigTag;
	FString NextDisplayText;
	if (!HeroProjectileSwitchAbility::ResolveNextProjectileConfigTag(
		CurrentWeaponDefinition,
		CurrentSelectedProjectileConfigTag,
		NextProjectileConfigTag,
		NextDisplayText))
	{
		OutFailureReason = TEXT("current weapon has no resolvable projectile switch target");
		return false;
	}

	return true;
}

void UHeroGA_ProjectileSwitch::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UHeroCombatComponent* HeroCombatComponent = GetHeroCombatComponentFromActorInfo();
	UHeroEquipmentComponent* HeroEquipmentComponent =
		HeroCombatComponent ? HeroCombatComponent->GetOwningHeroEquipmentComponent() : nullptr;
	UHeroLoadoutContextComponent* HeroLoadoutContextComponent = GetHeroLoadoutContextComponentFromActorInfo();
	UHeroLoadoutRuntimeComponent* HeroLoadoutRuntimeComponent = GetHeroLoadoutRuntimeComponentFromActorInfo();
	if (!HeroCombatComponent || !HeroEquipmentComponent || !HeroLoadoutContextComponent || !HeroLoadoutRuntimeComponent)
	{
		Debug::Print(TEXT("[GA][ProjectileSwitch] \u5931\u8d25\uff1a\u6218\u6597\u7ec4\u4ef6\u3001\u88c5\u5907\u7ec4\u4ef6\u6216 runtime \u7ec4\u4ef6\u65e0\u6548"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		Debug::Print(TEXT("[GA][ProjectileSwitch] \u5931\u8d25\uff1aCommitAbility \u672a\u901a\u8fc7"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const EHeroWeaponLoadoutSlot CurrentLoadoutSlot = HeroEquipmentComponent->GetCurrentEquippedLoadoutSlot();
	const UDataAsset_WeaponDefinition* CurrentWeaponDefinition =
		HeroLoadoutRuntimeComponent->GetLoadedWeaponDefinitionByLoadoutSlot(CurrentLoadoutSlot);
	if (!CurrentWeaponDefinition)
	{
		Debug::Print(TEXT("[GA][ProjectileSwitch] \u5931\u8d25\uff1a\u5f53\u524d\u6b66\u5668\u5b9a\u4e49\u65e0\u6548"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FGameplayTag CurrentSelectedProjectileConfigTag;
	HeroLoadoutContextComponent->GetLoadoutSlotSelectedProjectileConfigTag(
		CurrentLoadoutSlot,
		CurrentSelectedProjectileConfigTag);

	FGameplayTag NextProjectileConfigTag;
	FString NextDisplayText;
	if (!HeroProjectileSwitchAbility::ResolveNextProjectileConfigTag(
		CurrentWeaponDefinition,
		CurrentSelectedProjectileConfigTag,
		NextProjectileConfigTag,
		NextDisplayText))
	{
		Debug::Print(TEXT("[GA][ProjectileSwitch] \u5931\u8d25\uff1a\u5f53\u524d\u6b66\u5668\u6ca1\u6709\u53ef\u5207\u6362\u7684\u53d1\u5c04\u7269\u914d\u7f6e"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const bool bSetSucceeded = NextProjectileConfigTag.IsValid()
		? HeroLoadoutContextComponent->SetLoadoutSlotSelectedProjectileConfigTag(CurrentLoadoutSlot, NextProjectileConfigTag)
		: (HeroLoadoutContextComponent->ClearLoadoutSlotSelectedProjectileConfigTag(CurrentLoadoutSlot), true);
	if (!bSetSucceeded)
	{
		Debug::Print(TEXT("[GA][ProjectileSwitch] \u5931\u8d25\uff1a\u5199\u5165\u5f53\u524d\u53d1\u5c04\u7269\u914d\u7f6e\u6807\u7b7e\u5931\u8d25"), FColor::Red, 2.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	Debug::Print(
		FString::Printf(
			TEXT("[GA][ProjectileSwitch] \u5df2\u5207\u6362\uff1a\u69fd\u4f4d=%d \u6b66\u5668=%s \u53d1\u5c04\u7269=%s"),
			static_cast<int32>(CurrentLoadoutSlot),
			*CurrentWeaponDefinition->WeaponTag.ToString(),
			*NextDisplayText),
		FColor::Cyan,
		2.5f);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
