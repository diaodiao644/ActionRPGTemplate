// 文件说明：实现 ActionCharacterBase 相关逻辑。

#include "Characters/ActionCharacterBase.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "Components/Collision/ActionCollisionRuntimeComponent.h"
#include "Components/Combat/PawnCombatComponent.h"
#include "Components/Execution/ExecutionWindowComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace ActionCharacterSocketDefaults
{
	const FName WeaponUnarmedSocketName(TEXT("Socket_Weapon_Unarmed"));
	const FName WeaponSwordSocketName(TEXT("Socket_Weapon_Sword"));
	const FName WeaponBladeSocketName(TEXT("Socket_Weapon_Blade"));
	const FName WeaponSpearSocketName(TEXT("Socket_Weapon_Spear"));
	const FName WeaponScytheSocketName(TEXT("Socket_Weapon_Scythe"));
	const FName WeaponStaffSocketName(TEXT("Socket_Weapon_Staff"));
	const FName WeaponBowSocketName(TEXT("Socket_Weapon_Bow"));
	const FName WeaponGunSocketName(TEXT("Socket_Weapon_Gun"));
	const FName HolsteredWeaponUnarmedSocketName(TEXT("Socket_Weapon_Unarmed"));
	const FName HolsteredWeaponSwordSocketName(TEXT("Socket_Weapon_Holster_Sword"));
	const FName HolsteredWeaponBladeSocketName(TEXT("Socket_Weapon_Holster_Blade"));
	const FName HolsteredWeaponSpearSocketName(TEXT("Socket_Weapon_Holster_Spear"));
	const FName HolsteredWeaponScytheSocketName(TEXT("Socket_Weapon_Holster_Scythe"));
	const FName HolsteredWeaponStaffSocketName(TEXT("Socket_Weapon_Holster_Staff"));
	const FName HolsteredWeaponBowSocketName(TEXT("Socket_Weapon_Holster_Bow"));
	const FName HolsteredWeaponGunSocketName(TEXT("Socket_Weapon_Holster_Gun"));
}

AActionCharacterBase::AActionCharacterBase()
{
	// 基类角色默认不启用 Tick，减少无意义的帧开销。
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ExecutionWindowComponent = CreateDefaultSubobject<UExecutionWindowComponent>(TEXT("ExecutionWindowComponent"));
	ActionCombatReactComponent = CreateDefaultSubobject<UActionCombatReactComponent>(TEXT("ActionCombatReactComponent"));
	ActionCollisionRuntimeComponent = CreateDefaultSubobject<UActionCollisionRuntimeComponent>(TEXT("ActionCollisionRuntimeComponent"));

	// 角色武器挂点默认按“武器小类别 Tag -> 固定骨骼 Socket”收口。
	// 蓝图后续只需要在骨骼上创建这些名字的 Socket；如果个别角色确实不同，
	// 再分别覆写 WeaponSockets 与 HolsteredWeaponSockets 即可。
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Unarmed, ActionCharacterSocketDefaults::WeaponUnarmedSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Sword, ActionCharacterSocketDefaults::WeaponSwordSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Blade, ActionCharacterSocketDefaults::WeaponBladeSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Spear, ActionCharacterSocketDefaults::WeaponSpearSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Scythe, ActionCharacterSocketDefaults::WeaponScytheSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Staff, ActionCharacterSocketDefaults::WeaponStaffSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Bow, ActionCharacterSocketDefaults::WeaponBowSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Gun, ActionCharacterSocketDefaults::WeaponGunSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Sword, ActionCharacterSocketDefaults::WeaponSwordSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Blade, ActionCharacterSocketDefaults::WeaponBladeSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Spear, ActionCharacterSocketDefaults::WeaponSpearSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Staff, ActionCharacterSocketDefaults::WeaponStaffSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Bow, ActionCharacterSocketDefaults::WeaponBowSocketName);
	WeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Gun, ActionCharacterSocketDefaults::WeaponGunSocketName);

	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Unarmed, ActionCharacterSocketDefaults::HolsteredWeaponUnarmedSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Sword, ActionCharacterSocketDefaults::HolsteredWeaponSwordSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Blade, ActionCharacterSocketDefaults::HolsteredWeaponBladeSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Spear, ActionCharacterSocketDefaults::HolsteredWeaponSpearSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Melee_Scythe, ActionCharacterSocketDefaults::HolsteredWeaponScytheSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Staff, ActionCharacterSocketDefaults::HolsteredWeaponStaffSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Bow, ActionCharacterSocketDefaults::HolsteredWeaponBowSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Ranged_Gun, ActionCharacterSocketDefaults::HolsteredWeaponGunSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Sword, ActionCharacterSocketDefaults::HolsteredWeaponSwordSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Blade, ActionCharacterSocketDefaults::HolsteredWeaponBladeSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Spear, ActionCharacterSocketDefaults::HolsteredWeaponSpearSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Staff, ActionCharacterSocketDefaults::HolsteredWeaponStaffSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Bow, ActionCharacterSocketDefaults::HolsteredWeaponBowSocketName);
	HolsteredWeaponSockets.Add(ActionGameplayTags::Player_Weapon_Hybrid_Gun, ActionCharacterSocketDefaults::HolsteredWeaponGunSocketName);
}

UAbilitySystemComponent* AActionCharacterBase::GetAbilitySystemComponent() const
{
	return GetActionAbilitySystemComponent();
}

void AActionCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (ActionAbilitySystemComponent)
	{
		ActionAbilitySystemComponent->GetOwnedGameplayTags(TagContainer);
	}
}

bool AActionCharacterBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (ActionAbilitySystemComponent)
	{
		return ActionAbilitySystemComponent->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool AActionCharacterBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (ActionAbilitySystemComponent)
	{
		return ActionAbilitySystemComponent->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AActionCharacterBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (ActionAbilitySystemComponent)
	{
		return ActionAbilitySystemComponent->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool AActionCharacterBase::HandleCombatReactEvent_Implementation(
	FGameplayTag EventTag,
	const FActionDamagePayload& DamagePayload)
{
	if (!EventTag.IsValid())
	{
		return false;
	}

	bool bWasHandled = false;

	if (ExecutionWindowComponent)
	{
		// 处决窗口类事件优先由处决窗口组件接管，再决定是否继续传递到战斗组件。
		bWasHandled |= ExecutionWindowComponent->HandleCombatReactEvent(EventTag, DamagePayload.InstigatorActor);
	}

	if (UPawnCombatComponent* CombatComponent = FindComponentByClass<UPawnCombatComponent>())
	{
		bWasHandled |= CombatComponent->HandleIncomingCombatEvent(EventTag, DamagePayload.InstigatorActor);
	}

	if (ActionCombatReactComponent)
	{
		bWasHandled |= ActionCombatReactComponent->HandleCombatReactEvent(EventTag, DamagePayload);
	}

	return bWasHandled;
}

FName AActionCharacterBase::GetWeaponSocketBySubtypeTag(const FGameplayTag InWeaponSubtypeTag) const
{
	if (!InWeaponSubtypeTag.IsValid())
	{
		return FName();
	}

	const FName* FoundSocketName = WeaponSockets.Find(InWeaponSubtypeTag);
	return FoundSocketName ? *FoundSocketName : FName();
}

FName AActionCharacterBase::GetHolsteredWeaponSocketBySubtypeTag(const FGameplayTag InWeaponSubtypeTag) const
{
	if (!InWeaponSubtypeTag.IsValid())
	{
		return FName();
	}

	if (const FName* FoundSocketName = HolsteredWeaponSockets.Find(InWeaponSubtypeTag))
	{
		return *FoundSocketName;
	}

	return GetWeaponSocketBySubtypeTag(InWeaponSubtypeTag);
}

void AActionCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (ActionCollisionRuntimeComponent)
	{
		ActionCollisionRuntimeComponent->RegisterCollisionSlot(
			EActionCollisionSlot::CharacterCapsule,
			GetCapsuleComponent(),
			TEXT("CharacterCapsule"));
		ActionCollisionRuntimeComponent->RegisterCollisionSlot(
			EActionCollisionSlot::CharacterMesh,
			GetMesh(),
			TEXT("CharacterMesh"));
	}
}

void AActionCharacterBase::InitializeDefaultAttributes()
{
	if (bDefaultAttributesInitialized || !ActionAbilitySystemComponent || !ActionAttributeSet)
	{
		return;
	}

	// 先写各类最大值，再写当前值。
	// 这样即使后续某些属性查询或钳制依赖 Max*，当前值也能直接落到稳定范围内。
	ActionAttributeSet->SetMaxHealth(DefaultAttributeInitData.MaxHealth);
	ActionAttributeSet->SetMaxStamina(DefaultAttributeInitData.MaxStamina);
	ActionAttributeSet->SetMaxEnergy(DefaultAttributeInitData.MaxEnergy);
	ActionAttributeSet->SetMaxPoise(DefaultAttributeInitData.MaxPoise);

	ActionAttributeSet->SetHealth(FMath::Clamp(DefaultAttributeInitData.Health, 0.f, DefaultAttributeInitData.MaxHealth));
	ActionAttributeSet->SetShield(FMath::Clamp(DefaultAttributeInitData.Shield, 0.f, DefaultAttributeInitData.MaxHealth));
	ActionAttributeSet->SetStamina(FMath::Clamp(DefaultAttributeInitData.Stamina, 0.f, DefaultAttributeInitData.MaxStamina));
	ActionAttributeSet->SetEnergy(FMath::Clamp(DefaultAttributeInitData.Energy, 0.f, DefaultAttributeInitData.MaxEnergy));
	ActionAttributeSet->SetPoise(FMath::Clamp(DefaultAttributeInitData.Poise, 0.f, DefaultAttributeInitData.MaxPoise));

	// 这一组基础攻防与恢复属性先直接写入 AttributeSet，
	// 让后续攻击、闪避、受击和处决主链都能拿到一份明确数值。
	ActionAttributeSet->SetAttackPower(DefaultAttributeInitData.AttackPower);
	ActionAttributeSet->SetDefensePower(DefaultAttributeInitData.DefensePower);
	ActionAttributeSet->SetGuardDefense(DefaultAttributeInitData.GuardDefense);
	ActionAttributeSet->SetPoiseDefense(DefaultAttributeInitData.PoiseDefense);
	ActionAttributeSet->SetMoveSpeed(DefaultAttributeInitData.MoveSpeed);
	ActionAttributeSet->SetStaminaRecovery(DefaultAttributeInitData.StaminaRecovery);
	ActionAttributeSet->SetEnergyRecovery(DefaultAttributeInitData.EnergyRecovery);
	ActionAttributeSet->SetExecutionDamageMultiplier(DefaultAttributeInitData.ExecutionDamageMultiplier);

	// 角色移动组件依然直接读自身速度参数，因此这里同步刷新一次移动速度，
	// 避免“属性里是新值，但角色仍按旧速度移动”的测试错觉。
	if (UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement())
	{
		CharacterMovementComponent->MaxWalkSpeed = DefaultAttributeInitData.MoveSpeed;
	}

	bDefaultAttributesInitialized = true;
}
