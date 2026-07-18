#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCollisionTypes.h"
#include "Components/ActorComponent.h"
#include "ActionCollisionRuntimeComponent.generated.h"

class UPrimitiveComponent;

/**
 * 碰撞运行态组件。
 * 负责：
 * 1. 维护“碰撞槽位 -> 已注册组件”的正式注册表；
 * 2. 维护按 owner reason / priority 生效的碰撞覆写请求；
 * 3. 在默认快照与当前有效覆写之间重新计算组件最终碰撞表现。
 * 它不负责命中结算本身，也不替代武器/命中窗口/角色主链的业务状态源。
 * 这里的正式状态源只限于“碰撞桥接注册表 + 覆写请求集合”，不扩张为战斗业务状态。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UActionCollisionRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionCollisionRuntimeComponent();

	/**
	 * 注册一条碰撞槽位与组件的正式映射。
	 * 注册成功后会缓存组件默认碰撞快照，后续所有 override 都在这份默认值之上重算。
	 */
	bool RegisterCollisionSlot(
		EActionCollisionSlot InSlot,
		UPrimitiveComponent* InComponent,
		FName InRegistrationName = NAME_None);

	/** 提交一笔碰撞覆写请求，并返回用于释放的句柄。它只记录请求，不代表该请求一定成为最终生效项。 */
	FActionCollisionOverrideHandle AcquireCollisionOverride(const FActionCollisionOverrideRequest& InRequest);
	/** 按句柄释放一笔碰撞覆写请求，并触发对应槽位重算。 */
	void ReleaseCollisionOverride(const FActionCollisionOverrideHandle& InHandle);
	/** 按 OwnerReason 批量释放碰撞覆写请求。适合窗口结束、Ability 收尾或局部运行态清理时统一收口。 */
	void ReleaseCollisionOverridesByReason(FName InOwnerReason);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FRegisteredCollisionComponentRuntime
	{
		/** 注册到某个碰撞槽位的真实组件。 */
		TWeakObjectPtr<UPrimitiveComponent> Component;
		/** 这条注册归属的正式碰撞槽位。 */
		EActionCollisionSlot Slot = EActionCollisionSlot::Custom;
		/** 可选注册名，用于把同槽位下的覆写进一步定向到具体组件。 */
		FName RegistrationName = NAME_None;
		/** 注册当下抓取到的默认碰撞快照。恢复默认时只回到这里，不反查资产，也不回头读取业务层配置。 */
		FActionCollisionComponentSnapshot DefaultSnapshot;
	};

	struct FActiveCollisionOverrideRuntime
	{
		/** 当前覆写的唯一句柄。 */
		FActionCollisionOverrideHandle Handle;
		/** 当前覆写请求原文。优先级、owner reason、preset 和 channel override 都从这里读取。 */
		FActionCollisionOverrideRequest Request;
	};

	/** 从组件抓取一份默认碰撞快照。它只服务桥接恢复与重算，不形成第二套业务状态。 */
	FActionCollisionComponentSnapshot CaptureSnapshot(const UPrimitiveComponent* InComponent) const;
	/** 把指定槽位下的组件恢复到各自默认快照。 */
	void RestoreDefaultsForSlot(EActionCollisionSlot InSlot);
	/** 重新计算一个槽位当前应生效的碰撞结果，并回写到所有已注册组件。 */
	void RecomputeCollisionSlot(EActionCollisionSlot InSlot);
	/** 判断一笔覆写请求是否命中当前注册项。 */
	bool DoesRequestTargetRegistration(
		const FActionCollisionOverrideRequest& InRequest,
		const FRegisteredCollisionComponentRuntime& InRegistration) const;
	/** 为某条注册项选出当前真正应生效的最高优先级覆写。 */
	const FActiveCollisionOverrideRuntime* SelectEffectiveOverrideForRegistration(
		EActionCollisionSlot InSlot,
		const FRegisteredCollisionComponentRuntime& InRegistration) const;
	/** 把预设碰撞策略写进一份临时快照。它只重算本槽位碰撞表现，不参与命中语义裁决。 */
	void ApplyPresetToSnapshot(
		EActionCollisionPreset InPreset,
		FActionCollisionComponentSnapshot& InOutSnapshot) const;
	/** 把最终快照回写到真实组件。 */
	void ApplySnapshotToComponent(
		UPrimitiveComponent* InComponent,
		const FActionCollisionComponentSnapshot& InSnapshot) const;

private:
	UPROPERTY(Transient)
	/** 下一个可分配的覆写句柄 Id。仅用于组件内标识。 */
	int32 NextCollisionOverrideHandleId = 1;

	/** 已注册的“槽位 -> 组件”运行时注册表。它是碰撞覆写系统的正式宿主之一。 */
	TMap<EActionCollisionSlot, TArray<FRegisteredCollisionComponentRuntime>> RegisteredCollisionSlots;
	/** 当前仍存活的碰撞覆写请求集合。 */
	TMap<int32, FActiveCollisionOverrideRuntime> ActiveCollisionOverrides;
};
