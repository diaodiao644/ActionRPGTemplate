// 文件说明：声明 ActionPlayerController 相关接口。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "ActionType/ActionInputTypes.h"
#include "ActionPlayerController.generated.h"

class AActionHeroCharacter;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class ACTIONRPG_API AActionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AActionPlayerController();

public:
	// APlayerController 生命周期接口。
	virtual void AcknowledgePossession(class APawn* P) override;

public:
	// 移动状态访问接口：
	// 这里只维护控制器侧移动与转向语义镜像，不替代正式战斗输入运行态。
	/** 写入控制器侧高层移动语义镜像。它只服务本地移动/转向消费，不替代正式战斗输入资格。 */
	void SetMoveState(EMoveState InMoveState);
	/** 读取控制器侧高层移动语义镜像。 */
	EMoveState GetMoveState() const;

	/** 写入“当前是否存在移动输入”的控制器侧局部镜像。 */
	void SetHasMoveInput(bool bInHasMoveInput);
	/** 读取“当前是否存在移动输入”的控制器侧局部镜像。 */
	bool HasMoveInput() const;

	/** 读取当前控制器侧输入方向镜像。 */
	FRotator GetInputDirection() const;

	/** 写入当前是否使用 MotionWarping 朝向的控制器侧局部镜像。 */
	void SetMotionWrappingEnabled(bool bInMotionWrappingEnabled);
	/** 读取当前是否使用 MotionWarping 朝向的控制器侧局部镜像。 */
	bool IsMotionWrappingEnabled() const;

	// 根据当前移动状态刷新速度等移动数据。
	// 这里只服务控制器和角色移动链，不承接战斗按钮资格判断。
	void UpdateMovementData();

	// 启动链重新进入锁定态时，清空控制器侧移动输入运行时状态。
	// 它只重置本地移动镜像，不替代组件侧正式战斗输入重置。
	void ResetRuntimeStateForHeroStartup();

	/** 通过控制台打印当前玩家实际授予的 Hero 战斗 GA 关系配置摘要。它只服务调试，不推进任何正式业务状态。 */
	UFUNCTION(Exec, meta = (ToolTip = "通过控制台打印当前玩家实际授予的 Hero 战斗 GA 关系配置摘要。它只服务调试，不推进任何正式业务状态。"))
	void ActionDebug_PrintHeroCombatAbilityRelationshipAudit();

	/** 通过控制台打印当前玩家实际授予的 Hero 战斗 GA 类别审计摘要。它只服务调试，不推进任何正式业务状态。 */
	UFUNCTION(Exec, meta = (ToolTip = "通过控制台打印当前玩家实际授予的 Hero 战斗 GA 类别审计摘要。它只服务调试，不推进任何正式业务状态。"))
	void ActionDebug_PrintHeroCombatAbilityCategoryAudit();

	/** 通过控制台按输入标签打印指定 Hero 战斗 GA 的当前调试摘要。它只服务调试，不推进任何正式业务状态。 */
	UFUNCTION(Exec, meta = (ToolTip = "通过控制台按输入标签打印指定 Hero 战斗 GA 的当前调试摘要。它只服务调试，不推进任何正式业务状态。"))
	void ActionDebug_PrintHeroCombatAbilityDebug(const FString& InputTagName);

	/** 通过控制台按输入标签打印指定 Hero 战斗 GA 的类别审计摘要。它只服务调试，不推进任何正式业务状态。 */
	UFUNCTION(Exec, meta = (ToolTip = "通过控制台按输入标签打印指定 Hero 战斗 GA 的类别审计摘要。它只服务调试，不推进任何正式业务状态。"))
	void ActionDebug_PrintHeroCombatAbilityCategoryDebug(const FString& InputTagName);

	/** 通过控制台打印最近几次 Hero 战斗 GA 关系主链失败历史。它只服务调试排错，不持久化历史。 */
	UFUNCTION(Exec, meta = (ToolTip = "通过控制台打印最近几次 Hero 战斗 GA 关系主链失败历史。它只服务调试排错，不持久化历史。"))
	void ActionDebug_PrintHeroCombatAbilityRelationshipFailureHistory(int32 MaxEntries = 8);

protected:
	// APlayerController 生命周期接口。
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

protected:
	// Pawn 切换事件。
	UFUNCTION()
	void OnControllerPawnChanged(APawn* InOldPawn, APawn* InNewPawn);

protected:
	// 默认输入绑定函数：
	// 这里只承接基础 Move / Look / Jump / Walk 输入，不持有正式战斗按钮阶段、缓冲或 Held 回放状态。
	UFUNCTION()
	void Input_Jump();

	UFUNCTION()
	void Input_StopJumping();

	UFUNCTION()
	void Input_Move(const FInputActionValue& ActionValue);

	UFUNCTION()
	void Input_StopMove();

	UFUNCTION()
	void Input_Look(const FInputActionValue& ActionValue);

	UFUNCTION()
	void Input_Walk();

public:
	// 输入资源配置：
	// 这些字段只描述控制器层资源入口，不是运行时状态。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true", ToolTip = "默认输入 MappingContext 资源入口。它只服务控制器输入装配，不是运行时状态。"))
	UInputMappingContext* DefaultMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true", ToolTip = "跳跃输入动作资源入口。它只是控制器层输入资源，不是运行时状态。"))
	UInputAction* JumpAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true", ToolTip = "移动输入动作资源入口。它只是控制器层输入资源，不是运行时状态。"))
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true", ToolTip = "视角输入动作资源入口。它只是控制器层输入资源，不是运行时状态。"))
	UInputAction* LookAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true", ToolTip = "步行输入动作资源入口。它只是控制器层输入资源，不是运行时状态。"))
	UInputAction* WalkAction = nullptr;

protected:
	// 运行时移动状态：
	// 这里只表达控制器侧移动与转向语义，不替代 HeroCombatInputComponent 的正式战斗输入状态。
	/** 控制器侧高层移动语义镜像。它只服务本地移动/战斗分支消费，不替代正式战斗输入状态。 */
	EMoveState MoveState = EMoveState::Run;
	/** 控制器侧“当前是否存在移动输入”的局部镜像。 */
	bool bHasMoveInput = false;
	/** 控制器侧输入方向镜像。 */
	FRotator InputDirection;

	// 用于判断当前使用 MotionWarping 转向还是输入方向转向。
	// 它只服务控制器侧朝向语义，不表达战斗按钮资格。
	/** 控制器侧是否使用 MotionWarping 朝向的局部镜像。 */
	bool bIsMotionWrapping = false;

private:
	// 当前控制的英雄角色缓存。
	// 它只是控制器宿主解析结果，不是新的角色正式状态源。
	TWeakObjectPtr<AActionHeroCharacter> ControlledHeroCharacter = nullptr;
};
