#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionReactionTypes.generated.h"

class UAnimMontage;

/** 受击方向分类语义，只服务方向蒙太奇选择与受击朝向解析。 */
UENUM(BlueprintType)
enum class EActionCombatReactDirection : uint8
{
	None,
	Front,
	Left,
	Right,
	Back
};

/**
 * 当前受击运行时处于哪一段正式阶段的公共语义。
 * Reacting 表示主受击演出阶段，AirborneUncontrolled 表示空中失控阶段，
 * Recovery 表示已经进入恢复尾段但正式受击收尾尚未完成。
 */
UENUM(BlueprintType)
enum class EActionCombatReactPhase : uint8
{
	None,
	Reacting,
	AirborneUncontrolled,
	Recovery
};

/**
 * 战斗 Ability 在受击阶段下的激活与保护规则模板。
 * 它不定义角色当前处于什么受击状态，只定义当命中某种受击阶段时，
 * 这类 Ability 是否允许激活、阻断或通过恢复取消窗口重新进入。
 */
USTRUCT(BlueprintType)
struct FActionCombatReactAbilityRule
{
	GENERATED_BODY()

public:
	/** 当前是否启用这套受击阶段规则模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bUseCombatReactRule = false;

	/** 当前 Ability 是否在主受击演出阶段被阻断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bBlockDuringPrimaryReactPhase = true;

	/** 当前 Ability 是否在空中失控阶段被阻断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bBlockDuringAirborneUncontrolledPhase = true;

	/** 当前 Ability 是否在恢复阶段被阻断。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bBlockDuringRecoveryPhase = true;

	/** 当前 Ability 是否允许在恢复阶段通过取消窗口重新激活。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bAllowActivationDuringRecoveryCancelWindow = false;
};

/**
 * 单类受击事件的静态蒙太奇模板集合。
 * 它可以只配置一条通用蒙太奇，也可以按前后左右拆成方向受击蒙太奇。
 */
USTRUCT(BlueprintType)
struct FActionCombatReactMontageSet
{
	GENERATED_BODY()

public:
	/** 当前这组受击模板是否按方向区分蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bUseDirectionalMontages = false;

	/** 不区分方向时播放的通用受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact", meta = (EditCondition = "!bUseDirectionalMontages"))
	TSoftObjectPtr<UAnimMontage> DefaultMontage;

	/** 正面受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact", meta = (EditCondition = "bUseDirectionalMontages"))
	TSoftObjectPtr<UAnimMontage> FrontMontage;

	/** 左侧受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact", meta = (EditCondition = "bUseDirectionalMontages"))
	TSoftObjectPtr<UAnimMontage> LeftMontage;

	/** 右侧受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact", meta = (EditCondition = "bUseDirectionalMontages"))
	TSoftObjectPtr<UAnimMontage> RightMontage;

	/** 背面受击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact", meta = (EditCondition = "bUseDirectionalMontages"))
	TSoftObjectPtr<UAnimMontage> BackMontage;

public:
	/** 只判断这组静态蒙太奇模板里是否至少存在一条有效引用。 */
	bool HasAnyMontage() const
	{
		if (!bUseDirectionalMontages)
		{
			return !DefaultMontage.IsNull();
		}

		return !FrontMontage.IsNull()
			|| !LeftMontage.IsNull()
			|| !RightMontage.IsNull()
			|| !BackMontage.IsNull();
	}

	/**
	 * 只根据受击方向解析当前该播放哪条蒙太奇，不推进行为提交。
	 * 若方向未命中，则按现有稳定语义回退到 Front -> Default。
	 */
	UAnimMontage* ResolveMontage(const EActionCombatReactDirection InDirection) const
	{
		if (!bUseDirectionalMontages)
		{
			return DefaultMontage.Get();
		}

		switch (InDirection)
		{
		case EActionCombatReactDirection::Front:
			return FrontMontage.Get();

		case EActionCombatReactDirection::Left:
			return LeftMontage.Get();

		case EActionCombatReactDirection::Right:
			return RightMontage.Get();

		case EActionCombatReactDirection::Back:
			return BackMontage.Get();

		default:
			break;
		}

		if (UAnimMontage* FrontResolvedMontage = FrontMontage.Get())
		{
			return FrontResolvedMontage;
		}

		return DefaultMontage.Get();
	}

	/** 只收集这组受击模板依赖的软资源路径，供外层统一预热。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		if (!DefaultMontage.IsNull())
		{
			OutAssetPaths.AddUnique(DefaultMontage.ToSoftObjectPath());
		}

		if (!FrontMontage.IsNull())
		{
			OutAssetPaths.AddUnique(FrontMontage.ToSoftObjectPath());
		}

		if (!LeftMontage.IsNull())
		{
			OutAssetPaths.AddUnique(LeftMontage.ToSoftObjectPath());
		}

		if (!RightMontage.IsNull())
		{
			OutAssetPaths.AddUnique(RightMontage.ToSoftObjectPath());
		}

		if (!BackMontage.IsNull())
		{
			OutAssetPaths.AddUnique(BackMontage.ToSoftObjectPath());
		}
	}
};

/**
 * 角色级受击表现配置。
 * 它刻意放在角色侧而不是武器侧，因为受击本质上属于角色表现。
 */
USTRUCT(BlueprintType)
struct FActionCombatReactConfig
{
	GENERATED_BODY()

public:
	/** 普通受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet HitReactMontages;

	/** 重受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet HeavyHitReactMontages;

	/** 崩防受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet GuardBreakMontages;

	/** 失衡 / 破韧受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet PoiseBreakMontages;

	/** 击飞受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet LaunchMontages;

	/** 击倒受击的静态蒙太奇模板。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FActionCombatReactMontageSet KnockdownMontages;

	/** 恢复阶段允许通过取消窗口恢复响应的输入白名单。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	FGameplayTagContainer RecoveryCancelInputTags;

	/** 受击进入时是否先打断当前正在运行的战斗蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bStopCurrentCombatMontageOnReact = true;

	/** 受击进入时是否把战斗模式正式收回到 Idle。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bResetCombatModeToIdleOnReact = true;

	/** 受击进入时是否清掉当前连段索引。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CombatReact")
	bool bResetComboIndexOnReact = true;

public:
	/** 只收集当前角色全部受击 / 恢复蒙太奇依赖，不承担阶段切换或运行时激活。 */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutAssetPaths) const
	{
		HitReactMontages.CollectSoftObjectPaths(OutAssetPaths);
		HeavyHitReactMontages.CollectSoftObjectPaths(OutAssetPaths);
		GuardBreakMontages.CollectSoftObjectPaths(OutAssetPaths);
		PoiseBreakMontages.CollectSoftObjectPaths(OutAssetPaths);
		LaunchMontages.CollectSoftObjectPaths(OutAssetPaths);
		KnockdownMontages.CollectSoftObjectPaths(OutAssetPaths);
	}
};
