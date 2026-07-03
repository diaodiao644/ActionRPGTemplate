#include "AbilitySystem/Abilities/Hero/HeroGA_AttackRanged.h"

#include "ActionGameplayTags.h"

UHeroGA_AttackRangedLight::UHeroGA_AttackRangedLight()
	: Super()
{
	// 这里做的唯一正式工作，是把“默认轻击请求 + 轻攻击 AbilityTag + 远程槽”一次性绑死。
	// 它只声明远程槽入口身份，不在这里实现投射物或射击行为差异。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Default,
		ActionGameplayTags::Player_Ability_Attack_Light,
		EHeroWeaponLoadoutSlot::RangedWeapon);
}

UHeroGA_AttackRangedHeavy::UHeroGA_AttackRangedHeavy()
	: Super()
{
	// Held 在这里只是入口身份，不代表当前远程武器一定提供可播的重击分支。
	// 远程槽真正的攻击差异仍由武器定义和 Attack 主链解析决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Held,
		ActionGameplayTags::Player_Ability_Attack_Heavy,
		EHeroWeaponLoadoutSlot::RangedWeapon);
}

UHeroGA_AttackRangedDodgeCounter::UHeroGA_AttackRangedDodgeCounter()
	: Super()
{
	// DodgeCounter 在这里只固定闪反入口身份，不在子类里判断当前远程武器是否真的支持该反击分支。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_DodgeCounter,
		ActionGameplayTags::Player_Ability_Attack_DodgeCounter,
		EHeroWeaponLoadoutSlot::RangedWeapon);
}

UHeroGA_AttackRangedSprint::UHeroGA_AttackRangedSprint()
	: Super()
{
	// Sprint 在这里只固定冲刺攻击入口；是否真的能出招，仍由运行时移动条件和远程武器定义共同决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Sprint,
		ActionGameplayTags::Player_Ability_Attack_Sprint,
		EHeroWeaponLoadoutSlot::RangedWeapon);
}

UHeroGA_AttackRangedAirborne::UHeroGA_AttackRangedAirborne()
	: Super()
{
	// Airborne 在这里只固定空中攻击入口，不在这里实现跳射、投掷或其它远程空中派生行为。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Airborne,
		ActionGameplayTags::Player_Ability_Attack_Airborne,
		EHeroWeaponLoadoutSlot::RangedWeapon);
}
