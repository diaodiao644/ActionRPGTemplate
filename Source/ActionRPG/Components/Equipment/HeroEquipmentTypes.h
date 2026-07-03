// 文件说明：声明英雄装备域本地结果类型。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "HeroEquipmentTypes.generated.h"

/**
 * 高层外部命中效果写入失败原因。
 * 这套枚举只服务 request 写入口的失败分层，不表达底层 effect runtime 的长期状态。
 */
UENUM(BlueprintType)
enum class EHeroExternalHitEffectSourceApplyFailureReason : uint8
{
	None,
	InvalidRequest,
	NoResolvableEquippedLoadoutSlot,
	InvalidTargetLoadoutSlot,
	NoValidWeaponDefinition,
	TargetWeaponRuntimeNotReady,
	WeaponPolicyDisallowsAdditionalHitEffects,
	InternalApplyFailed
};

/**
 * 高层外部命中效果来源写入结果。
 * 这份结果只服务上层技能、Buff 与蓝图调试，
 * 用来明确这次请求到底成功没有、最终落到哪个槽位、失败卡在哪一层。
 * 它不表达当前 effect runtime 的长期持有状态，只表达这次高层写入口如何收口。
 */
USTRUCT(BlueprintType)
struct FHeroExternalHitEffectSourceApplyResult
{
	GENERATED_BODY()

public:
	/** 当前写入请求是否成功。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	bool bSuccess = false;

	/** 这次请求最终解析到的固定武器槽。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	EHeroWeaponLoadoutSlot ResolvedLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 当前失败原因；成功时保持 None。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffectSource")
	EHeroExternalHitEffectSourceApplyFailureReason FailureReason =
		EHeroExternalHitEffectSourceApplyFailureReason::None;
};

/**
 * 高层 direct 外部额外命中效果写入结果。
 * 语义与具名来源层保持一致，只是本层不涉及来源标签。
 * 两层结果共用同一套失败原因分层，方便调用侧统一按“请求收口结果”分流。
 */
USTRUCT(BlueprintType)
struct FHeroExternalAdditionalHitEffectsApplyResult
{
	GENERATED_BODY()

public:
	/** 当前写入请求是否成功。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffect")
	bool bSuccess = false;

	/** 这次请求最终解析到的固定武器槽。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffect")
	EHeroWeaponLoadoutSlot ResolvedLoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	/** 当前失败原因；成功时保持 None。 */
	UPROPERTY(BlueprintReadOnly, Category = "HitEffect")
	EHeroExternalHitEffectSourceApplyFailureReason FailureReason =
		EHeroExternalHitEffectSourceApplyFailureReason::None;
};
