#include "AbilitySystem/Abilities/Hero/HeroGA_AttackUnarmed.h"

#include "ActionGameplayTags.h"

UHeroGA_AttackUnarmedLight::UHeroGA_AttackUnarmedLight()
	: Super()
{
	// 这里做的唯一正式工作，是把“默认轻击请求 + 轻攻击 AbilityTag + 空手槽”一次性绑死。
	// 它只声明空手槽入口身份，不在这里硬编码空手动画或具体攻击差异。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Default,
		ActionGameplayTags::Player_Ability_Attack_Light,
		EHeroWeaponLoadoutSlot::Unarmed);
}

UHeroGA_AttackUnarmedHeavy::UHeroGA_AttackUnarmedHeavy()
	: Super()
{
	// Held 在这里只是入口身份，不代表当前空手武器一定提供可播的重击分支。
	// 真正分支配置仍由当前空手武器定义和 Attack 主链共同决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Held,
		ActionGameplayTags::Player_Ability_Attack_Heavy,
		EHeroWeaponLoadoutSlot::Unarmed);
}

UHeroGA_AttackUnarmedDodgeCounter::UHeroGA_AttackUnarmedDodgeCounter()
	: Super()
{
	// DodgeCounter 在这里只固定闪反入口身份，不在子类里判断当前空手武器是否真的支持该反击分支。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_DodgeCounter,
		ActionGameplayTags::Player_Ability_Attack_DodgeCounter,
		EHeroWeaponLoadoutSlot::Unarmed);
}

UHeroGA_AttackUnarmedSprint::UHeroGA_AttackUnarmedSprint()
	: Super()
{
	// Sprint 在这里只固定冲刺攻击入口；是否真的能出招，仍由运行时移动条件和空手武器定义共同决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Sprint,
		ActionGameplayTags::Player_Ability_Attack_Sprint,
		EHeroWeaponLoadoutSlot::Unarmed);
}

UHeroGA_AttackUnarmedAirborne::UHeroGA_AttackUnarmedAirborne()
	: Super()
{
	// Airborne 在这里只固定空中攻击入口，不在这里硬编码空手空中动画或特殊派生逻辑。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Airborne,
		ActionGameplayTags::Player_Ability_Attack_Airborne,
		EHeroWeaponLoadoutSlot::Unarmed);
}
