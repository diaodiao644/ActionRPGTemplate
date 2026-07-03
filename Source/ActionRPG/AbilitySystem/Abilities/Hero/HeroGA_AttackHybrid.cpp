#include "AbilitySystem/Abilities/Hero/HeroGA_AttackHybrid.h"

#include "ActionGameplayTags.h"

UHeroGA_AttackHybridLight::UHeroGA_AttackHybridLight()
	: Super()
{
	// 这里做的唯一正式工作，是把“默认轻击请求 + 轻攻击 AbilityTag + 混合槽”一次性绑死。
	// 它只声明入口身份，不在这里实现混合武器的具体攻击差异。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Default,
		ActionGameplayTags::Player_Ability_Attack_Light,
		EHeroWeaponLoadoutSlot::HybridWeapon);
}

UHeroGA_AttackHybridHeavy::UHeroGA_AttackHybridHeavy()
	: Super()
{
	// Held 在这里只是入口身份，不代表当前混合武器一定提供可播的重击分支。
	// 混合槽与其它槽位的真正差异仍由武器定义和 Attack 主链解析决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Held,
		ActionGameplayTags::Player_Ability_Attack_Heavy,
		EHeroWeaponLoadoutSlot::HybridWeapon);
}

UHeroGA_AttackHybridDodgeCounter::UHeroGA_AttackHybridDodgeCounter()
	: Super()
{
	// DodgeCounter 在这里只固定闪反入口身份，不在子类里判断当前混合武器是否真的支持该反击分支。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_DodgeCounter,
		ActionGameplayTags::Player_Ability_Attack_DodgeCounter,
		EHeroWeaponLoadoutSlot::HybridWeapon);
}

UHeroGA_AttackHybridSprint::UHeroGA_AttackHybridSprint()
	: Super()
{
	// Sprint 在这里只固定冲刺攻击入口；是否真的能出招，仍由运行时移动条件和混合武器定义共同决定。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Sprint,
		ActionGameplayTags::Player_Ability_Attack_Sprint,
		EHeroWeaponLoadoutSlot::HybridWeapon);
}

UHeroGA_AttackHybridAirborne::UHeroGA_AttackHybridAirborne()
	: Super()
{
	// Airborne 在这里只固定空中攻击入口，不在这里实现近远切换或空中派生逻辑。
	InitializeAttackAbility(
		ActionGameplayTags::Attack_Request_Airborne,
		ActionGameplayTags::Player_Ability_Attack_Airborne,
		EHeroWeaponLoadoutSlot::HybridWeapon);
}
