#include "AbilitySystem/Abilities/Hero/HeroGA_AttackMelee.h"

#include "ActionGameplayTags.h"

UHeroGA_AttackMeleeLight::UHeroGA_AttackMeleeLight()
	: Super()
{
	// 这里做的唯一正式工作，是把“默认轻击请求 + 轻攻击 AbilityTag + 近战槽”一次性绑死。
	// 这不是新的攻击实现层；是否真的有可播分支，仍由近战武器定义和 HeroGA_Attack 主链决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Default,
		ActionGameplayTags::Player_Ability_Attack_Light,
		EHeroWeaponLoadoutSlot::MeleeWeapon);
}

UHeroGA_AttackMeleeHeavy::UHeroGA_AttackMeleeHeavy()
	: Super()
{
	// Held 在这里只是入口身份，不代表当前近战武器一定提供可播的重击段位。
	// 同槽位下不同攻击种类共用同一套 Attack 主链，只靠请求标签分流。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Held,
		ActionGameplayTags::Player_Ability_Attack_Heavy,
		EHeroWeaponLoadoutSlot::MeleeWeapon);
}

UHeroGA_AttackMeleeDodgeCounter::UHeroGA_AttackMeleeDodgeCounter()
	: Super()
{
	// DodgeCounter 在这里只固定闪反入口身份，不在子类里判断当前近战武器是否真的支持该反击分支。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_DodgeCounter,
		ActionGameplayTags::Player_Ability_Attack_DodgeCounter,
		EHeroWeaponLoadoutSlot::MeleeWeapon);
}

UHeroGA_AttackMeleeSprint::UHeroGA_AttackMeleeSprint()
	: Super()
{
	// Sprint 在这里只固定冲刺攻击入口，不在这里实现“当前是否满足 FastRun 或是否有对应分支”的判断。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Sprint,
		ActionGameplayTags::Player_Ability_Attack_Sprint,
		EHeroWeaponLoadoutSlot::MeleeWeapon);
}

UHeroGA_AttackMeleeAirborne::UHeroGA_AttackMeleeAirborne()
	: Super()
{
	// Airborne 在这里只固定空中攻击入口；是否真的能出招，仍由运行时空中条件和近战武器定义共同决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Airborne,
		ActionGameplayTags::Player_Ability_Attack_Airborne,
		EHeroWeaponLoadoutSlot::MeleeWeapon);
}
