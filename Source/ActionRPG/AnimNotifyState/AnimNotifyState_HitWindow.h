#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionHitSourceTypes.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_HitWindow.generated.h"

class AHeroWeaponBase;
class UDataAsset_WeaponDefinition;
class UHeroCombatComponent;

/**
 * 命中窗口运行时缓存。
 * 同一个通知资产可能被多个角色同时复用，因此需要按 Mesh 分开保存窗口句柄与命中来源实例。
 */
USTRUCT()
struct FActionHitWindowRuntimeEntry
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TWeakObjectPtr<AHeroWeaponBase> Weapon;

	UPROPERTY()
	int32 HitWindowHandle = INDEX_NONE;

	UPROPERTY()
	bool bStartedWeaponDetection = false;
};

/**
 * 通用命中窗口通知。
 * 当前第一版先统一两类职责：
 * 1. 在有效帧内打开 / 关闭角色的命中窗口；
 * 2. 按配置决定是否同时驱动当前武器碰撞检测。
 */
UCLASS(meta = (DisplayName = "Hit Window"))
class ACTIONRPG_API UAnimNotifyState_HitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_HitWindow();

	/** 返回编辑器内显示名，方便直接看出本通知最终驱动的是哪段命中窗口。 */
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 解析当前动画拥有者的战斗组件。 */
	UHeroCombatComponent* ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const;

	/** 解析当前动画拥有者的已装备武器。 */
	AHeroWeaponBase* ResolveCurrentWeapon(USkeletalMeshComponent* MeshComp) const;

	/** 判断当前通知是否仍属于这次真正运行中的战斗蒙太奇。 */
	bool IsCurrentCombatMontageHitWindowNotify(
		const UHeroCombatComponent* HeroCombatComponent,
		UAnimSequenceBase* Animation) const;

	/** 生成本次命中窗口真正写入运行时的名字。 */
	FName ResolveRuntimeHitWindowName() const;

	/** 解析当前通知要使用的模板名。 */
	FName ResolveHitWindowTemplateName() const;

	/** 用通知自身字段生成本地运行时窗口配置。 */
	FActionHitWindowRuntimeConfig BuildLocalHitWindowRuntimeConfig() const;

	/** 优先尝试通过武器资产模板解析命中窗口配置。 */
	bool TryResolveTemplateHitWindowRuntimeConfig(
		const UDataAsset_WeaponDefinition* WeaponDefinition,
		FActionHitWindowRuntimeConfig& OutRuntimeConfig) const;

protected:
	/** 命中窗口名。留空时会回退到当前通知对象名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow")
	FName HitWindowName = NAME_None;

	/** 命中窗口模板名。留空时会回退到 HitWindowName。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Template")
	FName HitWindowTemplateName = NAME_None;

	/** 这段窗口额外启用的命中源列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (DisplayName = "EnabledHitSourceIds"))
	TArray<EActionHitSourceId> EnabledHitSourceIdEnums;

	/** 这段窗口额外启用的命中源组列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (DisplayName = "EnabledHitSourceGroupIds"))
	TArray<EActionHitSourceGroupId> EnabledHitSourceGroupIdEnums;

	/** 当前命中窗口内同一来源对同一目标的结算策略。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow")
	EActionHitWindowResolvePolicy ResolvePolicy = EActionHitWindowResolvePolicy::SingleHitPerSourceTarget;

	/** 当窗口策略为持续接触间隔结算时，控制两次结算之间的最小时间间隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (ClampMin = "0.01", EditCondition = "ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping", EditConditionHides))
	float RepeatResolveInterval = 0.2f;

	/** 当前窗口是否显式覆写默认命中配置里的击退强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Hit")
	bool bOverrideKnockbackStrength = false;

	/** 当窗口显式覆写击退时，本窗口最终使用的击退强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Hit", meta = (ClampMin = "0.0", EditCondition = "bOverrideKnockbackStrength", EditConditionHides))
	float OverrideKnockbackStrength = 0.f;

	/** 当前窗口是否同时驱动武器碰撞检测。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow")
	bool bUseWeaponCollisionDetection = true;

private:
	/** 按 Mesh 缓存本次通知开启时的运行时数据，避免多个角色复用同一通知资产时互相覆盖。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActionHitWindowRuntimeEntry> CachedRuntimeByMesh;
};
