#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Attack.h"
#include "HeroGA_AttackMelee.generated.h"

/**
 * 近战槽攻击入口壳。
 * 这一组子类只负责给近战槽固定不同攻击请求的入口身份：
 * 1. 固定近战槽；
 * 2. 固定请求标签；
 * 3. 固定专属 AbilityTag。
 * 真正是否有可播放分支、播哪段、怎么推进连段与如何收尾，全部回到 HeroGA_Attack 基类和当前武器定义。
 */
/** 近战轻攻击入口壳。只固定“默认轻击请求 + 近战槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackMeleeLight : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackMeleeLight();
};

/** 近战重攻击入口壳。只固定“Held 重击请求 + 近战槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackMeleeHeavy : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackMeleeHeavy();
};

/** 近战闪避反击入口壳。只固定“DodgeCounter 请求 + 近战槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackMeleeDodgeCounter : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackMeleeDodgeCounter();
};

/** 近战冲刺攻击入口壳。只固定“Sprint 请求 + 近战槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackMeleeSprint : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackMeleeSprint();
};

/** 近战空中攻击入口壳。只固定“Airborne 请求 + 近战槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackMeleeAirborne : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackMeleeAirborne();
};
