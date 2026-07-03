#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ActionCollisionTypes.generated.h"

/** 正式碰撞桥接里要作用到哪一类业务槽位，不是具体组件实例引用。 */
UENUM(BlueprintType)
enum class EActionCollisionSlot : uint8
{
	CharacterCapsule,
	CharacterMesh,
	WeaponHit,
	OwnedBodyHit,
	ProjectileCollision,
	Custom
};

/** 高层碰撞预设语义，不等于引擎底层完整碰撞配置快照。 */
UENUM(BlueprintType)
enum class EActionCollisionPreset : uint8
{
	Default,
	Disabled,
	HitQueryPawnOverlap,
	ProjectilePawnOverlap,
	ExecutionVictimPawnPassThrough
};

/** 单条碰撞通道覆写的静态请求片段，只描述“哪个通道改成什么响应”。 */
USTRUCT(BlueprintType)
struct FActionCollisionChannelOverride
{
	GENERATED_BODY()

public:
	/** 这条静态请求片段要覆写的碰撞通道。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	TEnumAsByte<ECollisionChannel> Channel = ECC_Pawn;

	/** 这条静态请求片段要写入的碰撞响应。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	TEnumAsByte<ECollisionResponse> Response = ECR_Ignore;
};

/** 某个碰撞组件当前状态的只读组件快照，只服务恢复、调试和桥接读取。 */
USTRUCT(BlueprintType)
struct FActionCollisionComponentSnapshot
{
	GENERATED_BODY()

public:
	/** 快照里的碰撞启用状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::NoCollision;

	/** 快照里的碰撞对象类型。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	TEnumAsByte<ECollisionChannel> CollisionObjectType = ECC_WorldDynamic;

	/** 快照里的全通道响应结果。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	FCollisionResponseContainer CollisionResponses;

	/** 快照里的重叠事件开关。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	bool bGenerateOverlapEvents = false;
};

/** 一次正式碰撞覆写注册后的轻量句柄壳，只服务后续更新、撤销或查找。 */
USTRUCT(BlueprintType)
struct FActionCollisionOverrideHandle
{
	GENERATED_BODY()

public:
	/** 这次覆写注册分配到的句柄 Id。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	int32 HandleId = INDEX_NONE;

	/** 这次覆写句柄当前绑定到的业务槽位。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Collision")
	EActionCollisionSlot Slot = EActionCollisionSlot::Custom;

	/** 只判断句柄壳本身是否仍具备有效注册标识。 */
	bool IsValid() const
	{
		return HandleId != INDEX_NONE;
	}

	/** 只回收句柄壳本身，不回滚真实碰撞状态。 */
	void Reset()
	{
		HandleId = INDEX_NONE;
		Slot = EActionCollisionSlot::Custom;
	}
};

/**
 * 高层碰撞覆写请求模板。
 * 它只描述“想怎么改”，不等于已经成功应用，也不是碰撞组件内部正式运行态。
 */
USTRUCT(BlueprintType)
struct FActionCollisionOverrideRequest
{
	GENERATED_BODY()

public:
	/** 本次高层覆写请求想作用到的业务槽位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	EActionCollisionSlot Slot = EActionCollisionSlot::Custom;

	/** 本次高层覆写请求优先采用哪套碰撞预设语义。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	EActionCollisionPreset Preset = EActionCollisionPreset::Default;

	/** 本次高层覆写请求的优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	int32 Priority = 0;

	/** 本次高层覆写请求的正式归属原因名，也是最小有效入口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	FName OwnerReason = NAME_None;

	/** 本次高层覆写请求想命中的目标注册名集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	TArray<FName> TargetRegistrationNames;

	/** 本次高层覆写请求要叠加的单通道覆写片段集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision")
	TArray<FActionCollisionChannelOverride> ChannelOverrides;

	/** 只校验高层覆写请求是否具备最小请求入口，不负责执行或裁决优先级结果。 */
	bool IsValidRequest() const
	{
		return OwnerReason != NAME_None;
	}
};
