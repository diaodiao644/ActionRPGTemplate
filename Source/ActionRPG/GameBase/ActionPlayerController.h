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
	// 移动状态访问接口。
	void SetMoveState(EMoveState InMoveState);
	EMoveState GetMoveState() const;

	void SetHasMoveInput(bool bInHasMoveInput);
	bool HasMoveInput() const;

	FRotator GetInputDirection() const;

	void SetMotionWrappingEnabled(bool bInMotionWrappingEnabled);
	bool IsMotionWrappingEnabled() const;

	// 根据当前移动状态刷新速度等移动数据。
	void UpdateMovementData();

	// 启动链重新进入锁定态时，清空控制器侧移动输入运行时状态。
	void ResetRuntimeStateForHeroStartup();

	/** 通过控制台打印当前玩家实际授予的 Hero 战斗 GA 关系配置摘要。 */
	UFUNCTION(Exec)
	void ActionDebug_PrintHeroCombatAbilityRelationshipAudit();

	/** 通过控制台按输入标签打印指定 Hero 战斗 GA 的当前调试摘要。 */
	UFUNCTION(Exec)
	void ActionDebug_PrintHeroCombatAbilityDebug(const FString& InputTagName);

protected:
	// APlayerController 生命周期接口。
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;

protected:
	// Pawn 切换事件。
	UFUNCTION()
	void OnControllerPawnChanged(APawn* InOldPawn, APawn* InNewPawn);

protected:
	// 默认输入绑定函数。
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
	// 输入资源配置。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* JumpAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* WalkAction = nullptr;

protected:
	// 运行时移动状态。
	EMoveState MoveState = EMoveState::Run;
	bool bHasMoveInput = false;
	FRotator InputDirection;

	// 用于判断当前使用 MotionWarping 转向还是输入方向转向。
	bool bIsMotionWrapping = false;

private:
	// 当前控制的英雄角色缓存。
	TWeakObjectPtr<AActionHeroCharacter> ControlledHeroCharacter = nullptr;
};
