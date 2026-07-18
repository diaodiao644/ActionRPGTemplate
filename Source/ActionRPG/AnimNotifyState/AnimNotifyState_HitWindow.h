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
 * 同一个通知资产可能被多个角色同时复用，因此需要按 Mesh 分开保存本次通知驱动的局部缓存。
 * 它只服务当前通知 Begin/End 的成对收尾，不是新的正式命中窗口状态源。
 */
USTRUCT()
struct FActionHitWindowRuntimeEntry
{
	GENERATED_BODY()

public:
	/** 当前这次通知实际驱动到的武器实例。它只服务 Begin/End 配对收尾，不表达已装备正式状态。 */
	UPROPERTY()
	TWeakObjectPtr<AHeroWeaponBase> Weapon;

	/** 当前这次通知在正式命中窗口宿主里拿到的窗口句柄。它只是通知级临时索引，不是窗口状态本体。 */
	UPROPERTY()
	int32 HitWindowHandle = INDEX_NONE;

	/** 当前这次通知是否真的启动过武器碰撞检测。它只服务 End 时是否需要成对关闭。 */
	UPROPERTY()
	bool bStartedWeaponDetection = false;
};

/**
 * 通用命中窗口通知。
 * 它只负责在有效帧把命中窗口模板或通知本地配置桥接给正式宿主，
 * 并按需同步驱动武器碰撞检测，不在这里自持正式命中窗口状态。
 */
UCLASS(meta = (DisplayName = "Hit Window"))
class ACTIONRPG_API UAnimNotifyState_HitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_HitWindow();

	/** 返回编辑器内显示名，方便直接看出本通知最终桥接的是哪段命中窗口。 */
	virtual FString GetNotifyName_Implementation() const override;

	/** 在 Begin 帧把本通知解析成正式窗口配置，并请求宿主打开当前命中窗口。 */
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	/** 在 End 帧关闭当前通知打开的正式命中窗口，并统一结束本次通知驱动的武器检测。 */
	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 解析当前动画拥有者的战斗组件。它只是命中窗口消费宿主解析入口，不形成新的命中状态源。 */
	UHeroCombatComponent* ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const;

	/** 解析当前动画拥有者的已装备武器。它只服务当前通知驱动的武器检测开关，不等于新的武器状态源。 */
	AHeroWeaponBase* ResolveCurrentWeapon(USkeletalMeshComponent* MeshComp) const;

	/** 判断当前通知是否仍属于这次真正运行中的战斗蒙太奇。它只做时机保护，不决定窗口配置本身。 */
	bool IsCurrentCombatMontageHitWindowNotify(
		const UHeroCombatComponent* HeroCombatComponent,
		UAnimSequenceBase* Animation) const;

	/** 生成本次命中窗口真正写入运行时的名字。它只负责运行时窗口标识，不决定命中参数内容。 */
	FName ResolveRuntimeHitWindowName() const;

	/** 解析当前通知要使用的模板名。留空时会继续回退到本地窗口名。 */
	FName ResolveHitWindowTemplateName() const;

	/** 用通知自身字段生成本地运行时窗口配置。它返回的是桥接结果，不是窗口已正式打开后的长期状态。 */
	FActionHitWindowRuntimeConfig BuildLocalHitWindowRuntimeConfig() const;

	/** 优先尝试通过武器资产模板解析命中窗口配置。若解析失败，运行时会继续回退到通知本地字段。 */
	bool TryResolveTemplateHitWindowRuntimeConfig(
		const UDataAsset_WeaponDefinition* WeaponDefinition,
		FActionHitWindowRuntimeConfig& OutRuntimeConfig) const;

protected:
	/** 命中窗口名。留空时会回退到当前通知对象名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (ToolTip = "命中窗口名。留空时会回退到当前通知对象名。它主要用于运行时窗口标识、日志和命中窗口去重，不直接决定命中参数内容。"))
	FName HitWindowName = NAME_None;

	/** 命中窗口模板名。留空时会回退到 HitWindowName。它只负责模板查找入口，不是运行时结果结构。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Template", meta = (ToolTip = "命中窗口模板名。留空时会回退到 HitWindowName，再由武器定义里的模板表尝试解析。只有你希望这段窗口直接复用 WeaponDefinition 里的标准模板时才需要配它。"))
	FName HitWindowTemplateName = NAME_None;

	/** 这段窗口额外启用的命中源列表。它们属于通知本地窗口配置，不是运行时最终结果快照。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (DisplayName = "EnabledHitSourceIds", ToolTip = "这段窗口额外启用的命中源列表。它们会和模板里已有命中源一起参与本窗口命中；若模板已经完整覆盖，不需要在这里重复堆同一批来源。"))
	TArray<EActionHitSourceId> EnabledHitSourceIdEnums;

	/** 这段窗口额外启用的命中源组列表。它们属于通知本地窗口配置，不是运行时最终结果快照。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (DisplayName = "EnabledHitSourceGroupIds", ToolTip = "这段窗口额外启用的命中源组列表。适合让一段挥击同时放开多块身体/武器来源；若已逐个列了命中源，通常不必再重复添加等价分组。"))
	TArray<EActionHitSourceGroupId> EnabledHitSourceGroupIdEnums;

	/** 当前命中窗口内同一来源对同一目标的结算策略。它只控制当前窗口的重复结算方式，不负责伤害本身。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (ToolTip = "当前命中窗口内同一来源对同一目标的结算策略。普通近战通常保持 SingleHitPerSourceTarget；只有明确需要持续接触多次结算时，才改成 IntervalWhileOverlapping。"))
	EActionHitWindowResolvePolicy ResolvePolicy = EActionHitWindowResolvePolicy::SingleHitPerSourceTarget;

	/** 当窗口策略为持续接触间隔结算时，控制两次结算之间的最小时间间隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (ClampMin = "0.01", UIMin = "0.01", EditCondition = "ResolvePolicy == EActionHitWindowResolvePolicy::IntervalWhileOverlapping", EditConditionHides, ToolTip = "当窗口策略为持续接触间隔结算时，两次结算之间的最小时间间隔。只有 ResolvePolicy 改成 IntervalWhileOverlapping 时这里才会生效。"))
	float RepeatResolveInterval = 0.2f;

	/** 当前窗口是否显式覆写默认命中配置里的击退强度。关闭时正式命中继续沿用模板或 HitConfig。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Hit", meta = (ToolTip = "当前窗口是否显式覆写默认命中配置里的击退强度。关闭时继续沿用 HitConfig 或模板里的正式击退配置。"))
	bool bOverrideKnockbackStrength = false;

	/** 当窗口显式覆写击退时，本窗口最终使用的击退强度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow|Hit", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bOverrideKnockbackStrength", EditConditionHides, ToolTip = "当窗口显式覆写击退时，本窗口最终使用的击退强度。只在 bOverrideKnockbackStrength 为真时可配置。"))
	float OverrideKnockbackStrength = 0.f;

	/** 当前窗口是否同时驱动武器碰撞检测。关闭后仍可能继续消费本地/模板里显式启用的命中源与来源组。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitWindow", meta = (ToolTip = "当前窗口是否同时驱动武器碰撞检测。关闭后只消费显式启用的命中源和命中源组；若这段动作纯靠身体命中体或纯模板来源结算，可关闭。"))
	bool bUseWeaponCollisionDetection = true;

private:
	/** 按 Mesh 缓存本次通知开启时的局部 runtime 数据，避免多个角色复用同一通知资产时互相覆盖。它只是通知级临时缓存，不是正式命中状态源。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActionHitWindowRuntimeEntry> CachedRuntimeByMesh;
};
