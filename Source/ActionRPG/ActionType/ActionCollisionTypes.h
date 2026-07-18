#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ActionCollisionTypes.generated.h"

/** 正式碰撞桥接里要作用到哪一类业务槽位，不是具体组件实例引用，也不等于某个组件当前一定已注册到该槽位。 */
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

/** 高层碰撞预设语义，不等于引擎底层完整碰撞配置快照，也不单独构成运行时碰撞状态。 */
UENUM(BlueprintType)
enum class EActionCollisionPreset : uint8
{
	Default,
	Disabled,
	HitQueryPawnOverlap,
	ProjectilePawnOverlap,
	ExecutionVictimPawnPassThrough
};

/** 单条碰撞通道覆写的静态请求片段，只描述“哪个通道改成什么响应”，不包含碰撞启用状态或对象类型。 */
USTRUCT(BlueprintType)
struct FActionCollisionChannelOverride
{
	GENERATED_BODY()

public:
	/** 这条静态请求片段要覆写的碰撞通道。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "这条静态请求片段要覆写的碰撞通道。它只描述“改哪条通道”，不代表已经成功应用到组件。"))
	TEnumAsByte<ECollisionChannel> Channel = ECC_Pawn;

	/** 这条静态请求片段要写入的碰撞响应。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "这条静态请求片段要写入的碰撞响应。它只定义目标响应，不包含碰撞启用状态或对象类型。"))
	TEnumAsByte<ECollisionResponse> Response = ECR_Ignore;
};

/** 某个碰撞组件当前状态的只读组件快照，只服务恢复、调试和桥接读取，不是碰撞组件正式宿主。 */
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

/** 一次正式碰撞覆写注册后的轻量句柄壳，只服务后续更新、撤销或查找，不直接表达真实碰撞是否仍在生效。 */
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

	/** 只判断句柄壳本身是否仍具备有效注册标识，不代表外层请求仍是当前最终生效项。 */
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
 * 真正哪笔请求最终生效，仍要回到碰撞运行态注册表按槽位与优先级裁决。
 */
USTRUCT(BlueprintType)
struct FActionCollisionOverrideRequest
{
	GENERATED_BODY()

public:
	/** 本次高层覆写请求想作用到的业务槽位。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层覆写请求想作用到的业务槽位。它是高层业务语义入口，不是具体组件实例引用。"))
	EActionCollisionSlot Slot = EActionCollisionSlot::Custom;

	/** 本次高层覆写请求优先采用哪套碰撞预设语义。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层覆写请求优先采用哪套碰撞预设语义。它只决定这次请求的高层默认碰撞语义，必要时仍可叠加 ChannelOverrides 做局部覆写。"))
	EActionCollisionPreset Preset = EActionCollisionPreset::Default;

	/** 本次高层覆写请求的优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层碰撞覆写请求的优先级。值越大，越容易压过低优先级请求；它只参与同槽位请求裁决，不是全局碰撞层级。"))
	int32 Priority = 0;

	/** 本次高层覆写请求的正式归属原因名，也是最小有效入口。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层碰撞覆写请求的正式归属原因名，也是最小有效入口。留空时请求无效；它通常用来表达这次请求来自 Attack、Execution、Projectile 或其它正式业务原因。"))
	FName OwnerReason = NAME_None;

	/** 本次高层覆写请求想命中的目标注册名集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层碰撞覆写请求想命中的目标注册名集合。留空时通常表示作用于该槽位默认目标；只有你明确要把请求限制到某几个注册目标时才需要填写。"))
	TArray<FName> TargetRegistrationNames;

	/** 本次高层覆写请求要叠加的单通道覆写片段集合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Collision", meta = (ToolTip = "本次高层碰撞覆写请求要叠加的单通道覆写片段集合。只有预设语义不够表达这次请求时，才需要在这里做局部通道覆写。"))
	TArray<FActionCollisionChannelOverride> ChannelOverrides;

	/** 只校验高层覆写请求是否具备最小请求入口，不负责执行或裁决优先级结果。 */
	bool IsValidRequest() const
	{
		return OwnerReason != NAME_None;
	}
};
