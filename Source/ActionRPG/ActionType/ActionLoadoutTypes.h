// 文件说明：声明固定武器槽、装备快照、切武事务与相关 UI 快照共用的枚举与结构体。
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionGameplayTags.h"
#include "ActionType/ActionAbilityTypes.h"
#include "ActionType/ActionEffectTypes.h"
#include "ActionLoadoutTypes.generated.h"

class AHeroWeaponBase;
class UDataAsset_WeaponDefinition;
class UGameplayAbility;

/**
 * 武器类别。这里表示武器本身属于哪一类，不表示它当前装在哪个固定武器槽。
 */
UENUM(BlueprintType)
enum class EHeroWeaponCategory : uint8
{
	Unarmed,
	PureMelee,
	PureRanged,
	MeleeRangedHybrid
};

/**
 * 固定武器槽。这里表示玩家战斗中可直接手动切换的固定槽位，当前顺序固定为空手、近战、远程、混合。
 */
UENUM(BlueprintType)
enum class EHeroWeaponLoadoutSlot : uint8
{
	/** 无效槽位，仅用于当前没有明确槽位目标的占位场景。 */
	Invalid,

	/** 固定空手槽。 */
	Unarmed,

	/** 固定近战槽，只允许纯近战武器。 */
	MeleeWeapon,

	/** 固定远程槽，只允许纯远程武器。 */
	RangedWeapon,

	/** 固定混合槽，只允许近远程混合武器。 */
	HybridWeapon
};

/** 固定武器槽 startup prewarm 公共阶段枚举。它只表达公共业务阶段，不直接绑定某个组件实现。 */
UENUM(BlueprintType)
enum class EHeroWeaponLoadoutStartupState : uint8
{
	None,
	InProgress,
	Ready,
	Failed
};

/**
 * 武器属性体系类型。它只回答这把武器在伤害、附加效果和专属能力上属于哪一类体系，
 * 不回答这把武器装在哪个固定武器槽。
 */
UENUM(BlueprintType)
enum class EActionWeaponPropertyType : uint8
{
	Mundane,
	Spirit,
	Elemental
};

/**
 * 伤害大类。当前只区分物理和元素两条正式分支，具体元素种类继续通过 GameplayTag 表达。
 */
UENUM(BlueprintType)
enum class EActionDamageType : uint8
{
	Physical,
	Elemental
};

/**
 * 武器装入固定武器槽后缓存到运行时的属性增量数据。
 * 当前只描述这把武器会带来哪些属性增量与语义镜像，不直接写回 AttributeSet。
 * 其中额外命中效果数组只是公共缓存镜像；正式生命周期仍由 `HeroLoadoutEffectComponent` 持有。
 */
USTRUCT(BlueprintType)
struct FActionWeaponAttributeCacheData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的最大生命值增量。它属于正式资产配置入口。"))
	float MaxHealthBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的最大体力值增量。"))
	float MaxStaminaBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的最大韧性值增量。"))
	float MaxPoiseBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的攻击力增量。"))
	float AttackPowerBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的防御力增量。"))
	float DefensePowerBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的格挡防御增量。"))
	float GuardDefenseBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的暴击率增量。"))
	float CriticalChanceBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的暴击伤害倍率增量。"))
	float CriticalDamageBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的减伤增量。"))
	float DamageReductionBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的生命伤害抗性增量。"))
	float HealthDamageResistanceBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的格挡耗体抗性增量。"))
	float GuardStaminaCostResistanceBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的削韧伤害抗性增量。"))
	float PoiseDamageResistanceBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的韧性防御增量。"))
	float PoiseDefenseBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的易伤增量。正值通常表示更容易受伤。"))
	float DamageVulnerabilityBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的处决伤害倍率增量。"))
	float ExecutionDamageMultiplierBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的稳定增伤乘区增量。"))
	float OutgoingDamageMultiplierBonus = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache", meta = (ToolTip = "武器装入固定武器槽后提供的额外增伤乘区增量。"))
	float ExtraDamageMultiplierBonus = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|Semantic", meta = (ToolTip = "运行时镜像字段：当前会在装备链中由 WeaponDefinition 顶层的 WeaponPropertyType 自动回写。不要把这里当成正式资产入口。"))
	EActionWeaponPropertyType WeaponPropertyType = EActionWeaponPropertyType::Mundane;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|Semantic", meta = (ToolTip = "运行时镜像字段：当前会在装备链中由 WeaponDefinition 顶层的 DamageType 自动回写。不要在这里单独配置伤害大类。"))
	EActionDamageType DamageType = EActionDamageType::Physical;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|Semantic", meta = (ToolTip = "运行时镜像字段：当前会在装备链中由 WeaponDefinition 顶层的 DamageElementTypeTag 自动回写。只有元素武器的顶层语义才是正式入口。"))
	FGameplayTag DamageElementTypeTag;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|Semantic", meta = (ToolTip = "运行时镜像字段：当前会由顶层武器语义自动决定是否允许额外命中效果；不要在缓存层手动配置。"))
	bool bAllowAdditionalHitEffects = true;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|HitEffect", meta = (ToolTip = "运行时镜像字段：当前 direct 外部额外命中效果数组由装备效果组件构建和回写，不作为稳定资产入口。"))
	TArray<FActionHitEffectEntry> DirectExternalAdditionalHitEffects;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|HitEffect", meta = (ToolTip = "运行时镜像字段：当前近战命中实际消费的聚合外部额外效果数组由装备效果组件回写，不应直接在资产里配置。"))
	TArray<FActionHitEffectEntry> ExternalAdditionalHitEffects;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponAttributeCache|HitEffect", meta = (ToolTip = "运行时镜像字段：当前发射物命中继承的外部额外效果数组由装备效果组件回写，不应直接在资产里配置。"))
	TArray<FActionHitEffectEntry> ProjectileInheritedExternalAdditionalHitEffects;

public:
	/** 只判断这份公共属性缓存自身是否持有数值增量，不越权判断 context/effect 宿主是否就绪。 */
	bool HasAnyValue() const
	{
		return !FMath::IsNearlyZero(MaxHealthBonus)
			|| !FMath::IsNearlyZero(MaxStaminaBonus)
			|| !FMath::IsNearlyZero(MaxPoiseBonus)
			|| !FMath::IsNearlyZero(AttackPowerBonus)
			|| !FMath::IsNearlyZero(DefensePowerBonus)
			|| !FMath::IsNearlyZero(GuardDefenseBonus)
			|| !FMath::IsNearlyZero(CriticalChanceBonus)
			|| !FMath::IsNearlyZero(CriticalDamageBonus)
			|| !FMath::IsNearlyZero(DamageReductionBonus)
			|| !FMath::IsNearlyZero(HealthDamageResistanceBonus)
			|| !FMath::IsNearlyZero(GuardStaminaCostResistanceBonus)
			|| !FMath::IsNearlyZero(PoiseDamageResistanceBonus)
			|| !FMath::IsNearlyZero(PoiseDefenseBonus)
			|| !FMath::IsNearlyZero(DamageVulnerabilityBonus)
			|| !FMath::IsNearlyZero(ExecutionDamageMultiplierBonus)
			|| !FMath::IsNearlyZero(OutgoingDamageMultiplierBonus)
			|| !FMath::IsNearlyZero(ExtraDamageMultiplierBonus);
	}

	/** 只清空这份公共缓存镜像，不推进任何装备域宿主状态收尾。 */
	void Reset()
	{
		*this = FActionWeaponAttributeCacheData();
	}
};

/**
 * 固定武器槽的主攻击能力配置。当前 5 个攻击入口固定存在，因此直接按语义拆成明确字段。
 * 它只是固定槽定义里的配置模板，不承担真正的能力授予或回收。
 */
USTRUCT(BlueprintType)
struct FHeroLoadoutAttackAbilityConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "固定槽轻攻击入口默认授予的 Ability 类。对应 InputTag.GameplayAbility.Attack.Light。留空时该槽不会自动授予轻攻击入口。"))
	TSubclassOf<UGameplayAbility> LightAttackAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "固定槽重攻击入口默认授予的 Ability 类。对应 InputTag.GameplayAbility.Attack.Heavy。留空时该槽不会自动授予重攻击入口。"))
	TSubclassOf<UGameplayAbility> HeavyAttackAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "固定槽闪避反击入口默认授予的 Ability 类。对应 InputTag.GameplayAbility.Attack.DodgeCounter。"))
	TSubclassOf<UGameplayAbility> DodgeCounterAttackAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "固定槽冲刺攻击入口默认授予的 Ability 类。对应 InputTag.GameplayAbility.Attack.Sprint。"))
	TSubclassOf<UGameplayAbility> SprintAttackAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "固定槽空中攻击入口默认授予的 Ability 类。对应 InputTag.GameplayAbility.Attack.Airborne。"))
	TSubclassOf<UGameplayAbility> AirborneAttackAbility;

	/** 把固定槽攻击配置翻译成可授予列表，不在这里执行真正的能力授予。 */
	void AppendGrantedAbilities(TArray<FActionAbilitySet>& OutGrantedAbilities) const
	{
		AppendAbilityIfValid(OutGrantedAbilities, ActionGameplayTags::InputTag_GameplayAbility_Attack_Light, LightAttackAbility);
		AppendAbilityIfValid(OutGrantedAbilities, ActionGameplayTags::InputTag_GameplayAbility_Attack_Heavy, HeavyAttackAbility);
		AppendAbilityIfValid(OutGrantedAbilities, ActionGameplayTags::InputTag_GameplayAbility_Attack_DodgeCounter, DodgeCounterAttackAbility);
		AppendAbilityIfValid(OutGrantedAbilities, ActionGameplayTags::InputTag_GameplayAbility_Attack_Sprint, SprintAttackAbility);
		AppendAbilityIfValid(OutGrantedAbilities, ActionGameplayTags::InputTag_GameplayAbility_Attack_Airborne, AirborneAttackAbility);
	}

private:
	static void AppendAbilityIfValid(
		TArray<FActionAbilitySet>& OutGrantedAbilities,
		const FGameplayTag& InInputTag,
		const TSubclassOf<UGameplayAbility>& InAbilityClass)
	{
		if (!InAbilityClass)
		{
			return;
		}

		FActionAbilitySet AbilitySet;
		AbilitySet.InputTag = InInputTag;
		AbilitySet.AbilityToGrant = InAbilityClass;
		OutGrantedAbilities.Add(AbilitySet);
	}
};

/**
 * 英雄默认固定武器槽配置。只描述槽位语义、默认武器和授予能力，不保存运行时状态。
 * 这是固定槽静态定义，不是当前已生效装备态。
 */
USTRUCT(BlueprintType)
struct FHeroWeaponLoadoutDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout", meta = (ToolTip = "固定武器槽语义。当前顺序固定为：空手、近战、远程、混合。它由外层结构维护，资产作者不应手动改槽位语义。"))
	EHeroWeaponLoadoutSlot LoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout", meta = (ToolTip = "这个固定槽正式允许放入的武器类别。它与 LoadoutSlot 一一对应，由结构工具自动整理。"))
	EHeroWeaponCategory AllowedWeaponCategory = EHeroWeaponCategory::Unarmed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout", meta = (ToolTip = "这个固定槽默认装备的 WeaponDefinition。它决定角色进入游戏或重建负载时该槽位使用哪把正式武器；留空时该槽启动时不会有默认武器。"))
	TSoftObjectPtr<UDataAsset_WeaponDefinition> DefaultWeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "这个固定槽的五条主攻击能力模板。当前轻击、重击、闪避反击、冲刺攻击、空中攻击都从这里授予；它是固定槽静态授予模板，不是当前已激活能力快照。"))
	FHeroLoadoutAttackAbilityConfig AttackAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout|Ability", meta = (ToolTip = "这个固定槽额外授予的战斗能力集合。适合放该槽独有、但不属于五条主攻击入口的能力；留空时表示该槽没有额外常驻授予能力。"))
	TArray<FActionAbilitySet> AdditionalGrantedAbilities;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "WeaponLoadout", meta = (ToolTip = "这个槽位是否在角色出生时直接装备。当前正式规则固定只有空手槽为 true，其它槽位由运行时切换。"))
	bool bEquipOnSpawn = false;

public:
	/** 只回答这个固定槽要求哪类武器，不决定真实切槽事务或当前装备结果。 */
	static EHeroWeaponCategory ResolveRequiredWeaponCategory(const EHeroWeaponLoadoutSlot InLoadoutSlot)
	{
		switch (InLoadoutSlot)
		{
		case EHeroWeaponLoadoutSlot::Unarmed:
			return EHeroWeaponCategory::Unarmed;
		case EHeroWeaponLoadoutSlot::MeleeWeapon:
			return EHeroWeaponCategory::PureMelee;
		case EHeroWeaponLoadoutSlot::RangedWeapon:
			return EHeroWeaponCategory::PureRanged;
		case EHeroWeaponLoadoutSlot::HybridWeapon:
			return EHeroWeaponCategory::MeleeRangedHybrid;
		default:
			break;
		}

		return EHeroWeaponCategory::Unarmed;
	}

	/** 校验固定槽定义是否满足当前正式约束：槽位合法、类别匹配、出生装备规则一致。 */
	bool IsValidDefinition() const
	{
		if (LoadoutSlot == EHeroWeaponLoadoutSlot::Invalid)
		{
			return false;
		}

		if (AllowedWeaponCategory != ResolveRequiredWeaponCategory(LoadoutSlot))
		{
			return false;
		}

		return bEquipOnSpawn == (LoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed);
	}

	/** 展开这条固定槽定义应授予的能力配置，但不承担能力生命周期管理。 */
	void BuildGrantedAbilities(TArray<FActionAbilitySet>& OutGrantedAbilities) const
	{
		OutGrantedAbilities.Reset();
		AttackAbilities.AppendGrantedAbilities(OutGrantedAbilities);

		for (const FActionAbilitySet& AbilitySet : AdditionalGrantedAbilities)
		{
			if (AbilitySet.IsValid())
			{
				OutGrantedAbilities.Add(AbilitySet);
			}
		}
	}
};

/**
 * 手动切武请求壳与延后消费状态。
 * 它只承载“玩家想切去哪”以及“是否等表现期结束后再消费”，不推进真实切武事务。
 */
USTRUCT(BlueprintType)
struct FHeroQueuedWeaponSwitchRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (ToolTip = "当前是否真的挂着一笔待消费的切武请求。它只是运行时请求壳，不代表切武事务已经正式开始。"))
	bool bHasRequest = false;

	UPROPERTY(meta = (ToolTip = "当前待消费请求想切去的目标固定槽位。只有 bHasRequest 为 true 时这里才有意义。"))
	EHeroWeaponLoadoutSlot TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 当前请求是否要等表现期结束后再消费。它不等价于事务已经开始。 */
	bool bConsumeAfterPresentation = false;

	UPROPERTY(meta = (ToolTip = "这笔请求写入时的世界时间快照。它只服务排序和调试，不是新的状态源。"))
	float RequestWorldTime = 0.f;

	UPROPERTY(meta = (ToolTip = "这笔请求写入时的顺序号快照。它只服务排序和调试。"))
	int32 RequestOrder = 0;

public:
	/** 判断当前是否真的挂着一笔待消费请求。 */
	bool HasQueuedRequest() const
	{
		return bHasRequest;
	}

	EHeroWeaponLoadoutSlot GetQueuedLoadoutSlot() const
	{
		return TargetLoadoutSlot;
	}

	float GetRequestWorldTime() const
	{
		return RequestWorldTime;
	}

	int32 GetRequestOrder() const
	{
		return RequestOrder;
	}

	/** 写入一笔新的切武请求壳，并清掉旧的延后消费标记。 */
	void SetRequest(const EHeroWeaponLoadoutSlot InLoadoutSlot, const float InRequestWorldTime, const int32 InRequestOrder)
	{
		bHasRequest = true;
		TargetLoadoutSlot = InLoadoutSlot;
		bConsumeAfterPresentation = false;
		RequestWorldTime = InRequestWorldTime;
		RequestOrder = InRequestOrder;
	}

	/** 解析当前挂起请求；是否真正消费这笔请求，由调用方显式决定。 */
	bool ResolveQueuedLoadoutSlot(EHeroWeaponLoadoutSlot& OutLoadoutSlot, const bool bConsumeRequest)
	{
		if (!bHasRequest)
		{
			return false;
		}

		OutLoadoutSlot = TargetLoadoutSlot;
		if (bConsumeRequest)
		{
			Clear();
		}

		return true;
	}

	/** 把这笔请求标记为“等表现期结束后再消费”，不等价于事务已经开始。 */
	void MarkConsumeAfterPresentation()
	{
		if (!bHasRequest)
		{
			return;
		}

		bConsumeAfterPresentation = true;
	}

	void ClearConsumeAfterPresentation()
	{
		bConsumeAfterPresentation = false;
	}

	/** 只清这笔请求壳本身，不推进切武事务收尾。 */
	void Clear()
	{
		bHasRequest = false;
		TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
		bConsumeAfterPresentation = false;
		RequestWorldTime = 0.f;
		RequestOrder = 0;
	}

	bool ShouldConsumeAfterPresentation() const
	{
		return bHasRequest && bConsumeAfterPresentation;
	}
};

/**
 * 当前装备状态运行时数据。只表达已经对战斗逻辑生效的正式装备结果。
 * 这是一份统一快照；真正的完整装备流程仍由 `HeroEquipmentComponent` 推进。
 */
USTRUCT(BlueprintType)
struct FHeroEquippedWeaponState
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, meta = (ToolTip = "当前已生效装备的武器实例快照。它只描述正式结果，不等价于完整装备流程。"))
	TObjectPtr<AHeroWeaponBase> EquippedWeaponInstance = nullptr;

	UPROPERTY(meta = (ToolTip = "当前已生效装备结果对应的武器标签快照。"))
	FGameplayTag EquippedWeaponTag = ActionGameplayTags::Player_Weapon_Unarmed_Default;

	UPROPERTY(meta = (ToolTip = "当前已生效装备结果对应的固定槽位快照。"))
	EHeroWeaponLoadoutSlot EquippedLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	UPROPERTY(meta = (ToolTip = "当前已生效装备结果对应的武器类别快照。"))
	EHeroWeaponCategory EquippedWeaponCategory = EHeroWeaponCategory::Unarmed;

	UPROPERTY(Transient, meta = (ToolTip = "当前已生效装备结果对应的 WeaponDefinition 快照。它只服务运行时读取，不反向替代装备流程。"))
	TObjectPtr<UDataAsset_WeaponDefinition> EquippedWeaponDefinition = nullptr;

public:
	/** 判断当前正式装备快照是否已等于给定槽位与武器 Tag。 */
	bool IsEquippedState(const EHeroWeaponLoadoutSlot InLoadoutSlot, const FGameplayTag& InWeaponTag) const
	{
		return EquippedLoadoutSlot == InLoadoutSlot && EquippedWeaponTag == InWeaponTag;
	}

	/** 判断两份正式装备快照是否表达同一当前装备结果。只比较当前生效语义，不扩展到装备事务细节。 */
	bool MatchesEquippedState(const FHeroEquippedWeaponState& InOtherState) const
	{
		return EquippedWeaponInstance == InOtherState.EquippedWeaponInstance
			&& EquippedWeaponTag == InOtherState.EquippedWeaponTag
			&& EquippedLoadoutSlot == InOtherState.EquippedLoadoutSlot
			&& EquippedWeaponCategory == InOtherState.EquippedWeaponCategory
			&& EquippedWeaponDefinition == InOtherState.EquippedWeaponDefinition;
	}

	/** 只写入当前已生效装备快照，不等价于完整装备事务。 */
	void SetEquippedState(
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const FGameplayTag& InWeaponTag,
		const EHeroWeaponCategory InWeaponCategory,
		UDataAsset_WeaponDefinition* InWeaponDefinition,
		AHeroWeaponBase* InWeaponInstance)
	{
		EquippedLoadoutSlot = InLoadoutSlot;
		EquippedWeaponTag = InWeaponTag;
		EquippedWeaponCategory = InWeaponCategory;
		EquippedWeaponDefinition = InWeaponDefinition;
		EquippedWeaponInstance = InWeaponInstance;
	}

	/** 把当前已生效装备快照重置为空手默认值，不处理其它宿主链收尾。 */
	void ResetToUnarmed()
	{
		SetEquippedState(
			EHeroWeaponLoadoutSlot::Unarmed,
			ActionGameplayTags::Player_Weapon_Unarmed_Default,
			EHeroWeaponCategory::Unarmed,
			nullptr,
			nullptr);
	}
};

/**
 * 单个固定武器槽的只读 UI 快照。只服务 HUD / MVVM / 蓝图展示，不反向作为正式运行时状态源。
 */
USTRUCT(BlueprintType)
struct FHeroWeaponLoadoutSlotUISnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个 UI 槽位当前对应的固定槽语义。只服务展示，不反向驱动真实切槽。"))
	EHeroWeaponLoadoutSlot LoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个 UI 槽位当前允许放入的武器类别。它只服务展示和提示。"))
	EHeroWeaponCategory AllowedWeaponCategory = EHeroWeaponCategory::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个 UI 槽位当前展示的武器标签快照。"))
	FGameplayTag WeaponTag = ActionGameplayTags::Player_Weapon_Unarmed_Default;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个 UI 槽位当前展示给玩家的武器名称文本。"))
	FString WeaponLabel;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个 UI 槽位当前展示的武器属性体系类型。"))
	EActionWeaponPropertyType WeaponPropertyType = EActionWeaponPropertyType::Mundane;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个槽位当前是否已经分配了正式 WeaponDefinition。"))
	bool bHasAssignedWeaponDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个槽位当前运行时资源是否已经就绪。它只服务 UI 展示，不代表切武事务一定完成。"))
	bool bRuntimeReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个槽位当前是否就是正式已装备槽位。"))
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个槽位当前展示的武器是否支持可切换发射物配置。"))
	bool bSupportsProjectileSwitching = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "这个槽位当前展示的已选发射物配置标签。"))
	FGameplayTag SelectedProjectileConfigTag;
};

/**
 * 固定四槽 UI 根快照。只读聚合当前装备槽、启动链状态、切武冷却状态与四个槽位的展示数据。
 * 它只服务 UI 展示，不反向作为正式状态源，也不表达事务态是否仍在处理中。
 */
USTRUCT(BlueprintType)
struct FHeroWeaponLoadoutUISnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "当前 UI 快照里聚合的四个固定槽展示数据。"))
	TArray<FHeroWeaponLoadoutSlotUISnapshot> LoadoutSlots;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "当前正式已装备槽位的 UI 快照。"))
	EHeroWeaponLoadoutSlot CurrentEquippedLoadoutSlot = EHeroWeaponLoadoutSlot::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "启动链当前高层状态。它只服务展示，不替代真实启动状态机。"))
	EHeroWeaponLoadoutStartupState StartupState = EHeroWeaponLoadoutStartupState::None;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "启动链当前进度比例快照，范围通常为 0 到 1。"))
	float StartupProgressRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "启动链当前仍待完成的槽位数量快照。"))
	int32 StartupPendingSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "启动链本次总共需要处理的槽位数量快照。"))
	int32 StartupTotalSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "启动链最近一次失败原因文本。留空表示当前没有失败信息。"))
	FString StartupFailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "当前切武是否被冷却阻断。它只服务展示，不表达切武事务细节。"))
	bool bWeaponSwitchBlockedByCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLoadout|UI", meta = (ToolTip = "当前特殊切武是否已准备就绪。它只服务 UI 提示。"))
	bool bSpecialWeaponSwitchReady = false;
};

/**
 * 切武事务运行时状态。跟踪逻辑切武是否进行中，以及目标槽位是谁。
 * 它只表达真实切武事务，不与切武表现期混存。
 */
USTRUCT(BlueprintType)
struct FHeroWeaponSwitchTransactionState
{
	GENERATED_BODY()

public:
	bool bSwitchInProgress = false;
	bool bShouldBroadcastLifecycle = false;

	UPROPERTY(meta = (ToolTip = "当前这笔真实切武事务的目标槽位。只有 bSwitchInProgress 为 true 时这里才有意义。"))
	EHeroWeaponLoadoutSlot TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

public:
	/** 标记一笔真实切武事务开始，并记录是否需要向外广播生命周期。 */
	void BeginByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot, const bool bInShouldBroadcastLifecycle)
	{
		bSwitchInProgress = true;
		bShouldBroadcastLifecycle = bInShouldBroadcastLifecycle;
		TargetLoadoutSlot = InLoadoutSlot;
	}

	/** 只清事务态壳本身，不回滚已经写入的当前装备快照。 */
	void Clear()
	{
		bSwitchInProgress = false;
		bShouldBroadcastLifecycle = false;
		TargetLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;
	}

	/** 只用当前正式装备快照判断这笔事务是否已经落到目标槽位，不替代事务宿主。 */
	bool MatchesEquippedState(const FHeroEquippedWeaponState& InEquippedState) const
	{
		if (!bSwitchInProgress)
		{
			return false;
		}

		return TargetLoadoutSlot != EHeroWeaponLoadoutSlot::Invalid
			&& InEquippedState.EquippedLoadoutSlot == TargetLoadoutSlot;
	}
};

/**
 * 特殊切武表现期运行时状态。单独记录特殊切武演出是否正在播放，不和逻辑事务混存。
 * 普通切武独立演出链已经删除，因此这里只表达特殊切武表现壳是否仍在播放。
 */
USTRUCT(BlueprintType)
struct FHeroSpecialWeaponSwitchPresentationState
{
	GENERATED_BODY()

public:
	bool bPresentationActive = false;

public:
	/** 标记特殊切武表现期开始。 */
	void BeginPresentation()
	{
		bPresentationActive = true;
	}

	/** 只清表现态壳本身，不触碰真实切武事务。 */
	void Clear()
	{
		bPresentationActive = false;
	}
};
