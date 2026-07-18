// 文件说明：声明 ANS_SetMoveMode 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_SetMoveMode.generated.h"

enum EMovementMode;

UCLASS()
class ACTIONRPG_API UANS_SetMoveMode : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	/** 移动模式窗口通知：只在这一段动画窗口内切入/恢复 MovementMode，不自持正式移动状态。 */
	UANS_SetMoveMode();

	/** 在窗口开始时把角色切到 BeginMovementMode。它只负责这一帧的 MovementMode 桥接，不承担输入门禁、Root Motion 修复或移动残留清理。 */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	/** 当前实现不承担额外运行时逻辑，也不形成第二套窗口 runtime；这里只保留 NotifyState 标准生命周期。 */
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	/** 在窗口结束时把角色恢复到 EndMovementMode。它只负责窗口收尾时的 MovementMode 桥接。 */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	/** 动画窗口开始时要切入的移动模式。它只是本通知本地配置，不是正式移动状态源。 */
	UPROPERTY(EditAnywhere, Category = "MoveMode", meta = (ToolTip = "动画窗口开始时要切入的移动模式。只在 NotifyBegin 进入窗口时应用，不负责额外移动清理、输入门禁或根运动兜底。"))
	TEnumAsByte<EMovementMode> BeginMovementMode;	

	/** 动画窗口结束时要恢复到的移动模式。它只是本通知本地配置，不替代外层正式移动状态。 */
	UPROPERTY(EditAnywhere, Category = "MoveMode", meta = (ToolTip = "动画窗口结束时要恢复到的移动模式。它只负责窗口收尾时的 MovementMode 切换，不承担状态开窗或移动残留修正职责。"))
	TEnumAsByte<EMovementMode> EndMovementMode;
};
