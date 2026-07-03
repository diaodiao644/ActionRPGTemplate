#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionDamageScalingTypes.generated.h"

inline float ActionRoundBaseValueToInteger(const float InValue)
{
	return FMath::RoundToFloat(InValue);
}

inline float ActionRoundPositiveBaseValueToInteger(const float InValue)
{
	if (InValue <= 0.f)
	{
		return 0.f;
	}

	const float RoundedValue = ActionRoundBaseValueToInteger(InValue);
	return RoundedValue > 0.f ? RoundedValue : 1.f;
}

inline float ActionRoundRatioValueToFourPlaces(const float InValue)
{
	return FMath::RoundToFloat(InValue * 10000.f) / 10000.f;
}

/**
 * 当前伤害值从哪类正式属性读取。
 * 第一版固定只开放攻击力、防御力和最大生命值三类来源。
 */
UENUM(BlueprintType)
enum class EActionAttributeSourceType : uint8
{
	AttackPower,
	DefensePower,
	MaxHealth
};

/**
 * 显式按等级存一组浮点数。
 * 约定：
 * 1. 等级从 1 开始；
 * 2. 读取时会自动钳制到 [1, Num]；
 * 3. 这样蓝图和数据资产里能直接看见每一级具体值，便于手调。
 */
USTRUCT(BlueprintType)
struct FActionLevelScaledFloat
{
	GENERATED_BODY()

public:
	/** 每一级对应的显式数值，LevelValues[0] 表示 1 级。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LevelScaled")
	TArray<float> LevelValues;

	/** 判断当前是否至少配置了一档等级值。 */
	bool HasAnyLevelValue() const
	{
		return LevelValues.Num() > 0;
	}

	/** 按输入等级解析最终数值。等级会被自动钳制到合法范围。 */
	float ResolveValueForLevel(const int32 InLevel) const
	{
		if (LevelValues.Num() <= 0)
		{
			return 0.f;
		}

		const int32 SafeLevel = FMath::Clamp(InLevel, 1, LevelValues.Num());
		return LevelValues[SafeLevel - 1];
	}
};

/**
 * 攻击侧一次结算前先解析出的属性快照。
 * 这份快照只描述“当前攻击者此刻能拿来算伤害的正式面板值”，
 * 不负责命中窗口、受击规则或持久效果本身。
 */
USTRUCT(BlueprintType)
struct FActionCombatAttributeSnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float AttackPower = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float DefensePower = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float MaxHealth = 0.f;

	/** 当前攻击者稳定增伤乘区。默认值必须是 1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float OutgoingDamageMultiplier = 1.f;

	/** 当前攻击者额外增伤乘区。默认值必须是 1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float ExtraDamageMultiplier = 1.f;

	/** 暴击率，百分比。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float CriticalChance = 0.f;

	/** 暴击伤害倍率，百分比。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	float CriticalDamage = 150.f;

	/** 按属性来源枚举读取本次伤害公式真正需要的属性值。 */
	float ResolveAttributeValue(const EActionAttributeSourceType InSourceType) const
	{
		switch (InSourceType)
		{
		case EActionAttributeSourceType::DefensePower:
			return DefensePower;

		case EActionAttributeSourceType::MaxHealth:
			return MaxHealth;

		case EActionAttributeSourceType::AttackPower:
		default:
			break;
		}

		return AttackPower;
	}
};

/**
 * 一类伤害值如何跟属性和等级联动。
 * 公式固定为：
 * 来源属性值 × 当前等级倍率
 */
USTRUCT(BlueprintType)
struct FActionAttributeDrivenValueConfig
{
	GENERATED_BODY()

public:
	/** 当前这条值从哪类正式属性读取。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	EActionAttributeSourceType AttributeSourceType = EActionAttributeSourceType::AttackPower;

	/** 当前等级对应的属性倍率。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FActionLevelScaledFloat LevelScaledMultiplier;

	/** 判断当前是否至少配置了任意一条等级驱动信息。 */
	bool HasAnyConfiguredValue() const
	{
		return LevelScaledMultiplier.HasAnyLevelValue();
	}

	/** 按属性快照和能力等级解析这条值的原始结果。 */
	float ResolveRawValue(
		const FActionCombatAttributeSnapshot& InAttributeSnapshot,
		const int32 InAbilityLevel) const
	{
		const float SourceValue = FMath::Max(
			InAttributeSnapshot.ResolveAttributeValue(AttributeSourceType),
			0.f);
		const float LevelMultiplier = ActionRoundRatioValueToFourPlaces(LevelScaledMultiplier.ResolveValueForLevel(InAbilityLevel));
		return FMath::Max(SourceValue * LevelMultiplier, 0.f);
	}
};

/**
 * 当前活跃伤害链共用的一份等级上下文。
 * 作用：
 * 1. 让近战命中、武器命中和发射物生成都读同一份正式等级来源；
 * 2. 避免命中当帧再去猜当前究竟是哪条 GA 仍处于激活态；
 * 3. 让命中主链里需要按当前主动能力等级解析的数值都读同一来源。
 */
USTRUCT(BlueprintType)
struct FActionDamageContextRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前是否存在一条活跃的正式伤害上下文。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bActive = false;

	/** 当前正式结算等级，默认至少为 1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 AbilityLevel = 1;

	/** 当前伤害上下文来自哪条正式能力。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
	FGameplayTag SourceAbilityTag;

	/** 判断当前是否存在可供共享主链读取的正式上下文。 */
	bool HasActiveContext() const
	{
		return bActive;
	}

	/** 把上下文重置回默认空态。 */
	void Reset()
	{
		*this = FActionDamageContextRuntimeState();
	}
};

/** 统一解析攻击侧乘区。 */
inline float ActionResolveOutgoingDamageMultiplier(
	const FActionCombatAttributeSnapshot& InAttributeSnapshot)
{
	return FMath::Max(ActionRoundRatioValueToFourPlaces(InAttributeSnapshot.OutgoingDamageMultiplier), 0.f)
		* FMath::Max(ActionRoundRatioValueToFourPlaces(InAttributeSnapshot.ExtraDamageMultiplier), 0.f);
}

/** 统一解析本次是否暴击。 */
inline bool ActionShouldApplyCriticalHit(const FActionCombatAttributeSnapshot& InAttributeSnapshot)
{
	const float CriticalChancePercent = FMath::Clamp(
		ActionRoundRatioValueToFourPlaces(InAttributeSnapshot.CriticalChance),
		0.f,
		100.f);
	return CriticalChancePercent > 0.f && FMath::FRandRange(0.f, 100.f) <= CriticalChancePercent;
}

/** 把暴击伤害百分比转换为真正参与伤害的乘区。 */
inline float ActionResolveCriticalDamageMultiplier(const FActionCombatAttributeSnapshot& InAttributeSnapshot)
{
	return FMath::Max(ActionRoundRatioValueToFourPlaces(InAttributeSnapshot.CriticalDamage) / 100.f, 1.f);
}

/** 统一按“属性来源 + 等级倍率 + 乘区 + 暴击”解析一条最终伤害值。 */
inline float ActionResolveDrivenDamageValue(
	const FActionAttributeDrivenValueConfig& InValueConfig,
	const FActionCombatAttributeSnapshot& InAttributeSnapshot,
	const int32 InAbilityLevel,
	const bool bAllowCritical,
	bool& bOutDidCritical)
{
	bOutDidCritical = false;

	const float RawValue = InValueConfig.ResolveRawValue(InAttributeSnapshot, InAbilityLevel);
	if (RawValue <= 0.f)
	{
		return 0.f;
	}

	float FinalValue = RawValue * ActionRoundRatioValueToFourPlaces(
		ActionResolveOutgoingDamageMultiplier(InAttributeSnapshot));
	if (bAllowCritical && ActionShouldApplyCriticalHit(InAttributeSnapshot))
	{
		bOutDidCritical = true;
		FinalValue *= ActionResolveCriticalDamageMultiplier(InAttributeSnapshot);
	}

	return ActionRoundPositiveBaseValueToInteger(FinalValue);
}
