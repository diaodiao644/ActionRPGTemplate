#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Attack.h"
#include "HeroGA_AttackHybrid.generated.h"

/**
 * 混合槽攻击入口壳。
 * 这一组子类只负责给混合槽固定不同攻击请求的入口身份：
 * 1. 固定混合槽；
 * 2. 固定请求标签；
 * 3. 固定专属 AbilityTag。
 * 不在这里实现近远切换、动画分支、命中差异或连段规则；
 * 真正攻击解析、近远分支选择、命中窗口、连段推进与正式收尾，
 * 全部回到 HeroGA_Attack 基类和当前混合武器定义。
 */
/** 混合轻攻击入口壳。只固定“默认轻击请求 + 混合槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackHybridLight : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackHybridLight();
};

/** 混合重攻击入口壳。只固定“Held 重击请求 + 混合槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackHybridHeavy : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackHybridHeavy();
};

/** 混合闪避反击入口壳。只固定“DodgeCounter 请求 + 混合槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackHybridDodgeCounter : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackHybridDodgeCounter();
};

/** 混合冲刺攻击入口壳。只固定“Sprint 请求 + 混合槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackHybridSprint : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackHybridSprint();
};

/** 混合空中攻击入口壳。只固定“Airborne 请求 + 混合槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackHybridAirborne : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackHybridAirborne();
};
