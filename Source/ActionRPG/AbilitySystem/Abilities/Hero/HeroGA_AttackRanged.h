#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Attack.h"
#include "HeroGA_AttackRanged.generated.h"

/**
 * 远程槽攻击入口壳。
 * 这一组子类只负责给远程槽固定不同攻击请求的入口身份：
 * 1. 固定远程槽；
 * 2. 固定请求标签；
 * 3. 固定专属 AbilityTag。
 * 不在这里实现投射物、射击方式、动画分支、命中差异或连段规则；
 * 真正攻击解析、发射物或近战分支读取、命中窗口、连段推进与正式收尾，
 * 全部回到 HeroGA_Attack 基类和当前远程武器定义。
 */
/** 远程轻攻击入口壳。只固定“默认轻击请求 + 远程槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackRangedLight : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackRangedLight();
};

/** 远程重攻击入口壳。只固定“Held 重击请求 + 远程槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackRangedHeavy : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackRangedHeavy();
};

/** 远程闪避反击入口壳。只固定“DodgeCounter 请求 + 远程槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackRangedDodgeCounter : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackRangedDodgeCounter();
};

/** 远程冲刺攻击入口壳。只固定“Sprint 请求 + 远程槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackRangedSprint : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackRangedSprint();
};

/** 远程空中攻击入口壳。只固定“Airborne 请求 + 远程槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackRangedAirborne : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackRangedAirborne();
};
