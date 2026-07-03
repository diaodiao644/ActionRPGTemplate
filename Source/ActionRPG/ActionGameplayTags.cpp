// 文件说明：实现项目内使用的原生 GameplayTag。

#include "ActionGameplayTags.h"

#include "GameplayTagsManager.h"
#include "Debug/ActionDebugHelper.h"

namespace ActionGameplayTags
{
	/** 基础输入标签。*/
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "InputTag.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look, "InputTag.Look");

	/** 战斗输入标签。*/
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack, "InputTag.GameplayAbility.Attack");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Light, "InputTag.GameplayAbility.Attack.Light");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Heavy, "InputTag.GameplayAbility.Attack.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_DodgeCounter, "InputTag.GameplayAbility.Attack.DodgeCounter");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Sprint, "InputTag.GameplayAbility.Attack.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Airborne, "InputTag.GameplayAbility.Attack.Airborne");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_CombatModeOrDefense, "InputTag.GameplayAbility.CombatModeOrDefense");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Dodge, "InputTag.GameplayAbility.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Execution, "InputTag.GameplayAbility.Execution");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_LockTarget, "InputTag.GameplayAbility.LockTarget");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_ProjectileSwitch, "InputTag.GameplayAbility.ProjectileSwitch");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_SpiritSkill1, "InputTag.GameplayAbility.SpiritSkill1");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_SpiritSkill2, "InputTag.GameplayAbility.SpiritSkill2");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_SpiritSkill3, "InputTag.GameplayAbility.SpiritSkill3");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_SpiritSkill4, "InputTag.GameplayAbility.SpiritSkill4");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_WeaponSwitch, "InputTag.GameplayAbility.WeaponSwitch");

	/** 槽位专属战斗输入标签。*/
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Light_Unarmed, "InputTag.GameplayAbility.Attack.Light.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Light_Melee, "InputTag.GameplayAbility.Attack.Light.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Light_Ranged, "InputTag.GameplayAbility.Attack.Light.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Light_Hybrid, "InputTag.GameplayAbility.Attack.Light.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Heavy_Unarmed, "InputTag.GameplayAbility.Attack.Heavy.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Heavy_Melee, "InputTag.GameplayAbility.Attack.Heavy.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Heavy_Ranged, "InputTag.GameplayAbility.Attack.Heavy.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Heavy_Hybrid, "InputTag.GameplayAbility.Attack.Heavy.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_DodgeCounter_Unarmed, "InputTag.GameplayAbility.Attack.DodgeCounter.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_DodgeCounter_Melee, "InputTag.GameplayAbility.Attack.DodgeCounter.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_DodgeCounter_Ranged, "InputTag.GameplayAbility.Attack.DodgeCounter.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_DodgeCounter_Hybrid, "InputTag.GameplayAbility.Attack.DodgeCounter.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Sprint_Unarmed, "InputTag.GameplayAbility.Attack.Sprint.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Sprint_Melee, "InputTag.GameplayAbility.Attack.Sprint.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Sprint_Ranged, "InputTag.GameplayAbility.Attack.Sprint.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Sprint_Hybrid, "InputTag.GameplayAbility.Attack.Sprint.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Airborne_Unarmed, "InputTag.GameplayAbility.Attack.Airborne.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Airborne_Melee, "InputTag.GameplayAbility.Attack.Airborne.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Airborne_Ranged, "InputTag.GameplayAbility.Attack.Airborne.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Attack_Airborne_Hybrid, "InputTag.GameplayAbility.Attack.Airborne.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Dodge_Unarmed, "InputTag.GameplayAbility.Dodge.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Dodge_Melee, "InputTag.GameplayAbility.Dodge.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Dodge_Ranged, "InputTag.GameplayAbility.Dodge.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_Dodge_Hybrid, "InputTag.GameplayAbility.Dodge.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_CombatModeOrDefense_Unarmed, "InputTag.GameplayAbility.CombatModeOrDefense.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_CombatModeOrDefense_Melee, "InputTag.GameplayAbility.CombatModeOrDefense.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_CombatModeOrDefense_Ranged, "InputTag.GameplayAbility.CombatModeOrDefense.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_CombatModeOrDefense_Hybrid, "InputTag.GameplayAbility.CombatModeOrDefense.Hybrid");

	/** 固定武器槽输入标签。*/
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_WeaponSlot_Unarmed, "InputTag.GameplayAbility.WeaponSlot.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_WeaponSlot_Melee, "InputTag.GameplayAbility.WeaponSlot.Melee");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_WeaponSlot_Ranged, "InputTag.GameplayAbility.WeaponSlot.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_GameplayAbility_WeaponSlot_Hybrid, "InputTag.GameplayAbility.WeaponSlot.Hybrid");

	/** 自动触发技能标签。*/
	UE_DEFINE_GAMEPLAY_TAG(GameplayAbility_Given, "GameplayAbility.Given");
	UE_DEFINE_GAMEPLAY_TAG(GameplayAbility_Triggered_ChangeCombatMode, "GameplayAbility.Triggered.ChangeCombatMode");

	/** 通用战斗事件标签。*/
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_IncomingAttack, "Combat.Event.IncomingAttack");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_HitReact, "Combat.Event.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_HeavyHitReact, "Combat.Event.HeavyHitReact");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_GuardBreak, "Combat.Event.GuardBreak");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_PoiseBreak, "Combat.Event.PoiseBreak");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_Launch, "Combat.Event.Launch");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_Knockdown, "Combat.Event.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_ExecutionWindow_Open, "Combat.Event.ExecutionWindow.Open");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_ExecutionWindow_Close, "Combat.Event.ExecutionWindow.Close");
	UE_DEFINE_GAMEPLAY_TAG(Combat_Event_Execution_Triggered, "Combat.Event.Execution.Triggered");

	/** 通用战斗状态标签。*/
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_ExecutionInvulnerable, "State.Combat.ExecutionInvulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_ExecutionVictimLocked, "State.Combat.ExecutionVictimLocked");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_ExecutionVictim_HardLock, "State.Combat.ExecutionVictim.HardLock");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_HitStun_Active, "State.Combat.HitStun.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_HeavyHitReact_Active, "State.Combat.HeavyHitReact.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_GuardBreak_Active, "State.Combat.GuardBreak.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_PoiseBreak_Active, "State.Combat.PoiseBreak.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Launch_Active, "State.Combat.Launch.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Knockdown_Active, "State.Combat.Knockdown.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_SuperArmor_Active, "State.Combat.SuperArmor.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Defense_Active, "State.Combat.Defense.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_ParryWindow_Active, "State.Combat.ParryWindow.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Parry_Success, "State.Combat.Parry.Success");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Dodge_Active, "State.Combat.Dodge.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_PerfectDodge_Success, "State.Combat.PerfectDodge.Success");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_DodgeCounter_Ready, "State.Combat.DodgeCounter.Ready");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_Attack_Active, "State.Ability.Attack.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_Defense_Active, "State.Ability.Defense.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_Dodge_Active, "State.Ability.Dodge.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_Execution_Active, "State.Ability.Execution.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_SpiritSkill_Active, "State.Ability.SpiritSkill.Active");
	UE_DEFINE_GAMEPLAY_TAG(State_Ability_WeaponSwitch_Active, "State.Ability.WeaponSwitch.Active");

	UE_DEFINE_GAMEPLAY_TAG(Attack_Request_Default, "Attack.Request.Default");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Request_Held, "Attack.Request.Held");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Request_DodgeCounter, "Attack.Request.DodgeCounter");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Request_Sprint, "Attack.Request.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Request_Airborne, "Attack.Request.Airborne");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Branch_Light, "Attack.Branch.Light");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Branch_Heavy, "Attack.Branch.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Branch_DodgeCounter, "Attack.Branch.DodgeCounter");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Branch_Sprint, "Attack.Branch.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Branch_Airborne, "Attack.Branch.Airborne");

	/** 元素伤害标签。*/
	UE_DEFINE_GAMEPLAY_TAG(Damage, "Damage");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Element, "Damage.Element");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Element_Fire, "Damage.Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Element_Water, "Damage.Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Element_Earth, "Damage.Element.Earth");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Element_Thunder, "Damage.Element.Thunder");

	/** 状态效果标签。 */
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect, "StatusEffect");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat, "StatusEffect.Combat");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_SuperArmor, "StatusEffect.Combat.SuperArmor");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_Defense, "StatusEffect.Combat.Defense");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_ExecutionInvulnerable, "StatusEffect.Combat.ExecutionInvulnerable");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_ExecutionVictimLocked, "StatusEffect.Combat.ExecutionVictimLocked");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_ExecutionVictimHardLock, "StatusEffect.Combat.ExecutionVictimHardLock");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_HitStun, "StatusEffect.Combat.HitStun");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_HeavyHitReact, "StatusEffect.Combat.HeavyHitReact");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_GuardBreak, "StatusEffect.Combat.GuardBreak");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_PoiseBreak, "StatusEffect.Combat.PoiseBreak");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_Launch, "StatusEffect.Combat.Launch");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_Knockdown, "StatusEffect.Combat.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_ParryWindow, "StatusEffect.Combat.ParryWindow");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_ParrySuccess, "StatusEffect.Combat.ParrySuccess");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_Dodge, "StatusEffect.Combat.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_PerfectDodgeSuccess, "StatusEffect.Combat.PerfectDodgeSuccess");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_Combat_DodgeCounterReady, "StatusEffect.Combat.DodgeCounterReady");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_DamageOverTime, "StatusEffect.DamageOverTime");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_DamageOverTime_Burn, "StatusEffect.DamageOverTime.Burn");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_DamageOverTime_Poison, "StatusEffect.DamageOverTime.Poison");
	UE_DEFINE_GAMEPLAY_TAG(StatusEffect_DamageOverTime_Bleed, "StatusEffect.DamageOverTime.Bleed");

	/** 玩家武器标签。*/
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Unarmed, "Player.Weapon.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Unarmed_Default, "Player.Weapon.Unarmed.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee, "Player.Weapon.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Sword, "Player.Weapon.Melee.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Sword_Default, "Player.Weapon.Melee.Sword.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Blade, "Player.Weapon.Melee.Blade");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Blade_Default, "Player.Weapon.Melee.Blade.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Spear, "Player.Weapon.Melee.Spear");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Spear_Default, "Player.Weapon.Melee.Spear.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Scythe, "Player.Weapon.Melee.Scythe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Melee_Scythe_Default, "Player.Weapon.Melee.Scythe.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged, "Player.Weapon.Ranged");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Staff, "Player.Weapon.Ranged.Staff");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Staff_Default, "Player.Weapon.Ranged.Staff.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Bow, "Player.Weapon.Ranged.Bow");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Bow_Default, "Player.Weapon.Ranged.Bow.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Gun, "Player.Weapon.Ranged.Gun");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Ranged_Gun_Default, "Player.Weapon.Ranged.Gun.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid, "Player.Weapon.Hybrid");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Sword, "Player.Weapon.Hybrid.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Sword_Default, "Player.Weapon.Hybrid.Sword.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Blade, "Player.Weapon.Hybrid.Blade");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Blade_Default, "Player.Weapon.Hybrid.Blade.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Spear, "Player.Weapon.Hybrid.Spear");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Spear_Default, "Player.Weapon.Hybrid.Spear.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Staff, "Player.Weapon.Hybrid.Staff");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Staff_Default, "Player.Weapon.Hybrid.Staff.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Bow, "Player.Weapon.Hybrid.Bow");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Bow_Default, "Player.Weapon.Hybrid.Bow.Default");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Gun, "Player.Weapon.Hybrid.Gun");
	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Hybrid_Gun_Default, "Player.Weapon.Hybrid.Gun.Default");

	/** 玩家战斗事件标签。*/
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Equip, "Player.Event.Equip");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Unequip, "Player.Event.Unequip");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_WeaponSwitch_Begin, "Player.Event.WeaponSwitch.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_WeaponSwitch_End, "Player.Event.WeaponSwitch.End");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_WeaponSwitch_Special, "Player.Event.WeaponSwitch.Special");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Execution_Begin, "Player.Event.Execution.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Execution_Hit, "Player.Event.Execution.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Execution_End, "Player.Event.Execution.End");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Execution_Cancelled, "Player.Event.Execution.Cancelled");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_InputBuffered, "Player.Event.InputBuffered");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_InputBuffer_Consumed, "Player.Event.InputBuffer.Consumed");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Defense_Begin, "Player.Event.Defense.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Defense_End, "Player.Event.Defense.End");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Defense_Blocked, "Player.Event.Defense.Blocked");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_ParryWindow_Open, "Player.Event.Defense.ParryWindow.Open");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_ParryWindow_Close, "Player.Event.Defense.ParryWindow.Close");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Parry_Success, "Player.Event.Defense.Parry.Success");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Dodge_Begin, "Player.Event.Dodge.Begin");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Dodge_End, "Player.Event.Dodge.End");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_PerfectDodgeWindow_Open, "Player.Event.Dodge.PerfectWindow.Open");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_PerfectDodgeWindow_Close, "Player.Event.Dodge.PerfectWindow.Close");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_PerfectDodge_Success, "Player.Event.Dodge.Perfect.Success");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_DodgeCounter_Available, "Player.Event.Dodge.Counter.Available");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_DodgeCounter_Consumed, "Player.Event.Dodge.Counter.Consumed");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Cooldown_Duration, "SetByCaller.Cooldown.Duration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Effect_Duration, "SetByCaller.Effect.Duration");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Effect_Period, "SetByCaller.Effect.Period");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_Health, "SetByCaller.Damage.Health");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Cost_GuardStamina, "SetByCaller.Cost.GuardStamina");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_Poise, "SetByCaller.Damage.Poise");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_ExecutionMultiplier, "SetByCaller.Damage.ExecutionMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage_IgnoreFinalDamageReductionPercent, "SetByCaller.Damage.IgnoreFinalDamageReductionPercent");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Attribute_DamageVulnerability, "SetByCaller.Attribute.DamageVulnerability");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Attribute_HealthDamageResistance, "SetByCaller.Attribute.HealthDamageResistance");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Attribute_GuardStaminaCostResistance, "SetByCaller.Attribute.GuardStaminaCostResistance");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Attribute_PoiseDamageResistance, "SetByCaller.Attribute.PoiseDamageResistance");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Attribute_ExecutionDamageMultiplier, "SetByCaller.Attribute.ExecutionDamageMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Ability_WeaponSwitch, "Cooldown.Ability.WeaponSwitch");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Ability_SpiritSkill1, "Cooldown.Ability.SpiritSkill1");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Ability_SpiritSkill2, "Cooldown.Ability.SpiritSkill2");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Ability_SpiritSkill3, "Cooldown.Ability.SpiritSkill3");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Ability_SpiritSkill4, "Cooldown.Ability.SpiritSkill4");

	/** GA 能力标签。*/
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack, "Player.Ability.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Light, "Player.Ability.Attack.Light");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Heavy, "Player.Ability.Attack.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_DodgeCounter, "Player.Ability.Attack.DodgeCounter");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Sprint, "Player.Ability.Attack.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Attack_Airborne, "Player.Ability.Attack.Airborne");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_CombatModeOrDefense, "Player.Ability.CombatModeOrDefense");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Dodge, "Player.Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Execution, "Player.Ability.Execution");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_ProjectileSwitch, "Player.Ability.ProjectileSwitch");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_SpiritSkill, "Player.Ability.SpiritSkill");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_WeaponSwitch, "Player.Ability.WeaponSwitch");

	bool IsSpiritSkillInputTag(const FGameplayTag& InputTag)
	{
		return InputTag == InputTag_GameplayAbility_SpiritSkill1
			|| InputTag == InputTag_GameplayAbility_SpiritSkill2
			|| InputTag == InputTag_GameplayAbility_SpiritSkill3
			|| InputTag == InputTag_GameplayAbility_SpiritSkill4;
	}

	FGameplayTag ResolveSpiritSkillCooldownTag(const FGameplayTag& InputTag)
	{
		if (InputTag == InputTag_GameplayAbility_SpiritSkill1)
		{
			return Cooldown_Ability_SpiritSkill1;
		}

		if (InputTag == InputTag_GameplayAbility_SpiritSkill2)
		{
			return Cooldown_Ability_SpiritSkill2;
		}

		if (InputTag == InputTag_GameplayAbility_SpiritSkill3)
		{
			return Cooldown_Ability_SpiritSkill3;
		}

		if (InputTag == InputTag_GameplayAbility_SpiritSkill4)
		{
			return Cooldown_Ability_SpiritSkill4;
		}

		return FGameplayTag();
	}

	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString)
	{
		// 先走精确匹配，这是正常运行时应当命中的路径。
		const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

		// 调试场景下允许做一次“包含式查找”，方便在不知道完整路径时快速定位标签。
		if (!Tag.IsValid() && bMatchPartialString)
		{
			FGameplayTagContainer AllTags;
			Manager.RequestAllGameplayTags(AllTags, true);

			for (const FGameplayTag& CandidateTag : AllTags)
			{
				if (CandidateTag.ToString().Contains(TagString))
				{
					UE_LOG(ActionRPG, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *CandidateTag.ToString());
					Tag = CandidateTag;
					break;
				}
			}
		}

		return Tag;
	}
}
