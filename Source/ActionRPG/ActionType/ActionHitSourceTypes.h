#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "ActionHitSourceTypes.generated.h"

class AActor;

/**
 * 命中来源类型的公共业务语义。
 * 这一层只回答“本次命中究竟来自哪里”，不直接代表某个组件当前是否已激活这类来源。
 */
UENUM(BlueprintType)
enum class EActionHitSourceType : uint8
{
	None,
	CharacterBody,
	WeaponCollision,
	Projectile,
	Execution
};

/**
 * 配置层命中源枚举。
 * 它只服务动画通知、WeaponDefinition 和命中窗口模板选择内建来源，
 * 不替代运行时正式使用的 FName SourceId。
 */
UENUM(BlueprintType)
enum class EActionHitSourceId : uint8
{
	None,
	WeaponCollision,
	MainWeaponBlade,
	OffhandWeaponBlade,
	SpearHead,
	SpearShaft,
	GreatswordBlade,
	CharacterBody,
	LeftFist,
	RightFist,
	LeftFoot,
	RightFoot,
	LeftElbow,
	RightElbow,
	LeftKnee,
	RightKnee,
	Projectile,
	Execution
};

/**
 * 配置层命中源组枚举。
 * 它只服务动画通知与 WeaponDefinition 模板选择内建命中源组，
 * 不替代运行时正式使用的 FName GroupId。
 */
UENUM(BlueprintType)
enum class EActionHitSourceGroupId : uint8
{
	None,
	UnarmedDefault,
	UnarmedLight,
	UnarmedKick,
	UnarmedElbow,
	WeaponDefault,
	DualBladeDefault,
	SpearDefault
};

/**
 * 命中窗口结算策略。
 * 它只决定“同一窗口内，同一命中源与同一目标之间如何重复结算”，
 * 不负责决定伤害本身，也不直接持有接触状态生命周期。
 */
UENUM(BlueprintType)
enum class EActionHitWindowResolvePolicy : uint8
{
	/** 同一窗口内，同一命中源对同一目标只结算一次。 */
	SingleHitPerSourceTarget,

	/** 首次接触立即结算，后续只要持续接触就按固定间隔重复结算。 */
	IntervalWhileOverlapping
};

namespace ActionHitSourceDefaults
{
	/** 把配置层命中源枚举翻译成运行时正式使用的 SourceId 名字。 */
	inline FName ResolveHitSourceIdName(const EActionHitSourceId InId)
	{
		switch (InId)
		{
		case EActionHitSourceId::WeaponCollision:
			return TEXT("WeaponCollision");
		case EActionHitSourceId::MainWeaponBlade:
			return TEXT("MainWeaponBlade");
		case EActionHitSourceId::OffhandWeaponBlade:
			return TEXT("OffhandWeaponBlade");
		case EActionHitSourceId::SpearHead:
			return TEXT("SpearHead");
		case EActionHitSourceId::SpearShaft:
			return TEXT("SpearShaft");
		case EActionHitSourceId::GreatswordBlade:
			return TEXT("GreatswordBlade");
		case EActionHitSourceId::CharacterBody:
			return TEXT("CharacterBody");
		case EActionHitSourceId::LeftFist:
			return TEXT("LeftFist");
		case EActionHitSourceId::RightFist:
			return TEXT("RightFist");
		case EActionHitSourceId::LeftFoot:
			return TEXT("LeftFoot");
		case EActionHitSourceId::RightFoot:
			return TEXT("RightFoot");
		case EActionHitSourceId::LeftElbow:
			return TEXT("LeftElbow");
		case EActionHitSourceId::RightElbow:
			return TEXT("RightElbow");
		case EActionHitSourceId::LeftKnee:
			return TEXT("LeftKnee");
		case EActionHitSourceId::RightKnee:
			return TEXT("RightKnee");
		case EActionHitSourceId::Projectile:
			return TEXT("Projectile");
		case EActionHitSourceId::Execution:
			return TEXT("Execution");
		default:
			return NAME_None;
		}
	}

	/** 把配置层命中源组枚举翻译成运行时正式使用的 GroupId 名字。 */
	inline FName ResolveHitSourceGroupIdName(const EActionHitSourceGroupId InId)
	{
		switch (InId)
		{
		case EActionHitSourceGroupId::UnarmedDefault:
			return TEXT("UnarmedDefault");
		case EActionHitSourceGroupId::UnarmedLight:
			return TEXT("UnarmedLight");
		case EActionHitSourceGroupId::UnarmedKick:
			return TEXT("UnarmedKick");
		case EActionHitSourceGroupId::UnarmedElbow:
			return TEXT("UnarmedElbow");
		case EActionHitSourceGroupId::WeaponDefault:
			return TEXT("WeaponDefault");
		case EActionHitSourceGroupId::DualBladeDefault:
			return TEXT("DualBladeDefault");
		case EActionHitSourceGroupId::SpearDefault:
			return TEXT("SpearDefault");
		default:
			return NAME_None;
		}
	}

	/** 读取武器碰撞来源的内建正式 SourceId。 */
	inline FName GetWeaponCollisionSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::WeaponCollision);
	}

	/** 读取角色身体来源的内建正式 SourceId。 */
	inline FName GetCharacterBodySourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::CharacterBody);
	}

	/** 读取左拳来源的内建正式 SourceId。 */
	inline FName GetLeftFistSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::LeftFist);
	}

	/** 读取右拳来源的内建正式 SourceId。 */
	inline FName GetRightFistSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::RightFist);
	}

	/** 读取左脚来源的内建正式 SourceId。 */
	inline FName GetLeftFootSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::LeftFoot);
	}

	/** 读取右脚来源的内建正式 SourceId。 */
	inline FName GetRightFootSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::RightFoot);
	}

	/** 读取左肘来源的内建正式 SourceId。 */
	inline FName GetLeftElbowSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::LeftElbow);
	}

	/** 读取右肘来源的内建正式 SourceId。 */
	inline FName GetRightElbowSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::RightElbow);
	}

	/** 读取左膝来源的内建正式 SourceId。 */
	inline FName GetLeftKneeSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::LeftKnee);
	}

	/** 读取右膝来源的内建正式 SourceId。 */
	inline FName GetRightKneeSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::RightKnee);
	}

	/** 读取发射物来源的内建正式 SourceId。 */
	inline FName GetProjectileSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::Projectile);
	}

	/** 读取处决来源的内建正式 SourceId。 */
	inline FName GetExecutionSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::Execution);
	}

	/** 读取主手刃来源的内建正式 SourceId。 */
	inline FName GetMainWeaponBladeSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::MainWeaponBlade);
	}

	/** 读取副手刃来源的内建正式 SourceId。 */
	inline FName GetOffhandWeaponBladeSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::OffhandWeaponBlade);
	}

	/** 读取枪头来源的内建正式 SourceId。 */
	inline FName GetSpearHeadSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::SpearHead);
	}

	/** 读取枪杆来源的内建正式 SourceId。 */
	inline FName GetSpearShaftSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::SpearShaft);
	}

	/** 读取大剑刃来源的内建正式 SourceId。 */
	inline FName GetGreatswordBladeSourceId()
	{
		return ResolveHitSourceIdName(EActionHitSourceId::GreatswordBlade);
	}

	/** 读取徒手默认来源组的内建正式 GroupId。 */
	inline FName GetUnarmedDefaultGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::UnarmedDefault);
	}

	/** 读取徒手轻击来源组的内建正式 GroupId。 */
	inline FName GetUnarmedLightGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::UnarmedLight);
	}

	/** 读取徒手踢击来源组的内建正式 GroupId。 */
	inline FName GetUnarmedKickGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::UnarmedKick);
	}

	/** 读取徒手肘击来源组的内建正式 GroupId。 */
	inline FName GetUnarmedElbowGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::UnarmedElbow);
	}

	/** 读取武器默认来源组的内建正式 GroupId。 */
	inline FName GetWeaponDefaultGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::WeaponDefault);
	}

	/** 读取双刃默认来源组的内建正式 GroupId。 */
	inline FName GetDualBladeDefaultGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::DualBladeDefault);
	}

	/** 读取长枪默认来源组的内建正式 GroupId。 */
	inline FName GetSpearDefaultGroupId()
	{
		return ResolveHitSourceGroupIdName(EActionHitSourceGroupId::SpearDefault);
	}
}

/**
 * 单个命中源的静态定义模板。
 * 它只描述“系统里有哪些可以被启用的来源”，
 * 但不描述当前这一帧是否激活，也不描述这次命中的伤害。
 */
USTRUCT(BlueprintType)
struct FActionHitSourceDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	EActionHitSourceType SourceType = EActionHitSourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	FGameplayTag SourceTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	FName SourceComponentName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	FName SourceSocketName = NAME_None;

	/** 只校验定义模板自身是否具备最小合法入口，不推进运行时注册或结算。 */
	bool IsValidDefinition() const
	{
		return SourceId != NAME_None && SourceType != EActionHitSourceType::None;
	}
};

/**
 * 多个命中源的静态分组模板。
 * 它只定义可复用的来源组合关系，不表达当前窗口是否已经启用这整组来源。
 */
USTRUCT(BlueprintType)
struct FActionHitSourceGroupDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	FName GroupId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitSource")
	TArray<FName> SourceIds;

	/** 只校验这份来源组模板自身是否成立。 */
	bool IsValidDefinition() const
	{
		return GroupId != NAME_None && SourceIds.Num() > 0;
	}
};

/**
 * 一次正式命中实例的运行时来源语义快照。
 * 它统一近战、发射物和处决三条命中链的来源描述，
 * 但不回写静态模板，也不单独充当命中解析器的状态宿主。
 */
USTRUCT(BlueprintType)
struct FActionHitSourceInfo
{
	GENERATED_BODY()

public:
	/** 当前命中来源的运行时唯一名。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FName SourceId = NAME_None;

	/** 当前真正造成命中的来源类型。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	EActionHitSourceType SourceType = EActionHitSourceType::None;

	/** 当前命中来源的语义标签。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FGameplayTag SourceTag;

	/** 若当前来源由上一层来源派生而来，这里记录上一层来源类型。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	EActionHitSourceType ParentSourceType = EActionHitSourceType::None;

	/** 若当前来源由上一层来源派生而来，这里记录上一层来源语义标签。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FGameplayTag ParentSourceTag;

	/** 命中时所属的武器标签快照。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FGameplayTag WeaponTag;

	/** 命中时所属的固定武器槽快照。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	EHeroWeaponLoadoutSlot LoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 本次命中来源组件名。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FName SourceComponentName = NAME_None;

	/** 本次命中来源使用的 Socket 名。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitSource")
	FName SourceSocketName = NAME_None;

	/** 只判断这次命中实例是否已经形成有效来源语义，不代表命中一定成功结算。 */
	bool HasValidSource() const
	{
		return SourceType != EActionHitSourceType::None
			|| SourceId != NAME_None
			|| SourceTag.IsValid()
			|| WeaponTag.IsValid()
			|| LoadoutSlot != EHeroWeaponLoadoutSlot::Invalid
			|| SourceComponentName != NAME_None
			|| SourceSocketName != NAME_None;
	}
};

/**
 * 命中窗口层的正式运行时配置。
 * 它把窗口名、启用来源、结算策略和重复间隔统一打包，
 * 供命中窗口通知、命中源组件和武器命中体共用。
 */
USTRUCT(BlueprintType)
struct FActionHitWindowRuntimeConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow")
	FName WindowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow")
	bool bUseWeaponCollisionDetection = true;

	/** 运行时是否已解析到应启用的正式 SourceId 名字集合，而不是配置层枚举。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow")
	TArray<FName> EnabledHitSourceIds;

	/** 运行时是否已解析到应启用的正式 GroupId 名字集合，而不是配置层枚举。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow")
	TArray<FName> EnabledHitSourceGroupIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow")
	EActionHitWindowResolvePolicy ResolvePolicy = EActionHitWindowResolvePolicy::SingleHitPerSourceTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow", meta = (ClampMin = "0.01", EditCondition = "ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping", EditConditionHides))
	float RepeatResolveInterval = 0.2f;

	/** 当前窗口是否显式覆写默认命中配置里的击退强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow|Hit")
	bool bOverrideKnockbackStrength = false;

	/** 当窗口显式覆写击退时，本窗口最终使用的击退强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitWindow|Hit", meta = (ClampMin = "0.0", EditCondition = "bOverrideKnockbackStrength", EditConditionHides))
	float OverrideKnockbackStrength = 0.f;

	/** 只回答当前窗口是否按持续接触重复结算，不承担接触状态生命周期宿主。 */
	bool UsesIntervalWhileOverlapping() const
	{
		return ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping
			&& RepeatResolveInterval > 0.f;
	}
};

/**
 * 单个命中源与单个目标之间的持续接触状态壳。
 * 它只服务 IntervalWhileOverlapping 这类窗口重复结算策略，
 * 不是通用命中历史，也不是解析器的全局状态源。
 */
USTRUCT()
struct FActionActiveHitContactState
{
	GENERATED_BODY()

public:
	/** 当前这份持续接触状态归属的正式 SourceId。 */
	UPROPERTY()
	FName SourceId = NAME_None;

	/** 当前正在与该来源保持接触的目标。 */
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** 当前接触对应的来源组件名，用于区分同源不同命中体。 */
	UPROPERTY()
	FName SourceComponentName = NAME_None;

	/** 上一次对这组 source-target 接触正式完成结算的世界时间。 */
	UPROPERTY()
	float LastResolvedWorldTime = 0.f;

	/** 当前这组 source-target 接触是否仍处于重叠态。 */
	UPROPERTY()
	bool bIsOverlapping = false;
};
