#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Hero/HeroGA_Attack.h"
#include "HeroGA_AttackUnarmed.generated.h"

/**
 * 空手槽攻击入口壳。
 * 这一组子类只负责给空手槽固定不同攻击请求的入口身份，
 * 不在这里硬编码空手动画、特殊分支、命中差异或连段规则；
 * 真正行为全部回到 HeroGA_Attack 基类和当前空手武器定义。
 */
/** 空手轻攻击入口壳。只固定“默认轻击请求 + 空手槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackUnarmedLight : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackUnarmedLight();
};

/** 空手重攻击入口壳。只固定“Held 重击请求 + 空手槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackUnarmedHeavy : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackUnarmedHeavy();
};

/** 空手闪避反击入口壳。只固定“DodgeCounter 请求 + 空手槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackUnarmedDodgeCounter : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackUnarmedDodgeCounter();
};

/** 空手冲刺攻击入口壳。只固定“Sprint 请求 + 空手槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackUnarmedSprint : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackUnarmedSprint();
};

/** 空手空中攻击入口壳。只固定“Airborne 请求 + 空手槽”这组入口身份，不承担具体攻击逻辑。 */
UCLASS()
class ACTIONRPG_API UHeroGA_AttackUnarmedAirborne : public UHeroGA_Attack
{
	GENERATED_BODY()

public:
	UHeroGA_AttackUnarmedAirborne();
};
