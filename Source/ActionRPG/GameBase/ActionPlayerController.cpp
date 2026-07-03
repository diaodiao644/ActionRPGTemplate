// 文件说明：实现 ActionPlayerController 相关逻辑。

#include "GameBase/ActionPlayerController.h"

#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Input/ActionEnhancedInputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagsManager.h"
#include "InputActionValue.h"
#include "Debug/ActionDebugHelper.h"

namespace
{
	static bool IsControlledHeroStartupReady(const TWeakObjectPtr<AActionHeroCharacter>& InControlledHeroCharacter)
	{
		if (!InControlledHeroCharacter.IsValid())
		{
			return false;
		}

		const UHeroAssemblyComponent* HeroAssemblyComponent = InControlledHeroCharacter->GetHeroAssemblyComponent();
		return HeroAssemblyComponent && HeroAssemblyComponent->IsHeroSystemsStartupReady();
	}
}

AActionPlayerController::AActionPlayerController()
	: Super()
{
}

void AActionPlayerController::SetMoveState(EMoveState InMoveState)
{
	MoveState = InMoveState;
}

EMoveState AActionPlayerController::GetMoveState() const
{
	return MoveState;
}

void AActionPlayerController::SetHasMoveInput(bool bInHasMoveInput)
{
	bHasMoveInput = bInHasMoveInput;
}

bool AActionPlayerController::HasMoveInput() const
{
	return bHasMoveInput;
}

FRotator AActionPlayerController::GetInputDirection() const
{
	return InputDirection;
}

void AActionPlayerController::SetMotionWrappingEnabled(bool bInMotionWrappingEnabled)
{
	bIsMotionWrapping = bInMotionWrappingEnabled;
}

bool AActionPlayerController::IsMotionWrappingEnabled() const
{
	return bIsMotionWrapping;
}

void AActionPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
}

void AActionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 输入资源不完整时直接退出，避免后续绑定空指针。
	if (!DefaultMappingContext || !MoveAction || !LookAction || !JumpAction)
	{
		UE_LOG(ActionRPG, Warning, TEXT("AActionPlayerController::SetupInputComponent - Failed to find input assets! Please check your input asset references."));
		return;
	}

	// 把默认输入映射加到本地玩家子系统中。
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	// 使用项目自定义输入组件统一绑定基础移动输入。
	if (UActionEnhancedInputComponent* ActionEnhancedInputComponent = Cast<UActionEnhancedInputComponent>(InputComponent))
	{
		ActionEnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AActionPlayerController::Input_Jump);
		ActionEnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AActionPlayerController::Input_StopJumping);
		ActionEnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AActionPlayerController::Input_Move);
		ActionEnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AActionPlayerController::Input_StopMove);
		ActionEnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AActionPlayerController::Input_Look);
		ActionEnhancedInputComponent->BindAction(WalkAction, ETriggerEvent::Started, this, &AActionPlayerController::Input_Walk);
	}
}

void AActionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 监听控制 Pawn 变化，确保缓存到的英雄角色始终有效。
	OnPossessedPawnChanged.AddDynamic(this, &AActionPlayerController::OnControllerPawnChanged);
	OnControllerPawnChanged(nullptr, GetPawn());
}

void AActionPlayerController::OnControllerPawnChanged(APawn* InOldPawn, APawn* InNewPawn)
{
	ControlledHeroCharacter = Cast<AActionHeroCharacter>(InNewPawn);
}

void AActionPlayerController::Input_Jump()
{
	if (IsControlledHeroStartupReady(ControlledHeroCharacter))
	{
		if (UHeroCombatComponent* HeroCombatComponent = ControlledHeroCharacter->GetHeroCombatComponent())
		{
			// 起跳前终止残留攻击蒙太奇并重置连段，避免空中状态沿用地面连段上下文。
			HeroCombatComponent->StopCurrentAttackMontage();
			HeroCombatComponent->ResetComboIndex();

			// 完美闪避得到的闪反资格只应该服务紧接着的地面反击链。
			// 玩家一旦主动起跳，就说明当前动作语义已经切到空中链，旧的闪反窗口必须立刻失效。
			if (UHeroDefenseComponent* HeroDefenseComponent = HeroCombatComponent->GetOwningHeroDefenseComponent())
			{
				HeroDefenseComponent->ClearDodgeCounterAvailability();
			}

			// 若跳跃发生在某些边界帧上，也一并清掉攻击组件里尚未被 GA 消费的本地闪反授权缓存。
			// 这样可以避免“窗口已因起跳失效，但本地缓存还允许后续错误放行”的状态分叉。
			if (UHeroAttackComponent* HeroAttackComponent = HeroCombatComponent->GetOwningHeroAttackComponent())
			{
				HeroAttackComponent->SetPendingDodgeCounterExecutionAuthorization(false);
			}
		}

		ControlledHeroCharacter->Jump();
	}
}

void AActionPlayerController::Input_StopJumping()
{
	if (ControlledHeroCharacter.Get())
	{
		ControlledHeroCharacter->StopJumping();
	}
}

void AActionPlayerController::Input_Move(const FInputActionValue& ActionValue)
{
	if (!IsControlledHeroStartupReady(ControlledHeroCharacter))
	{
		return;
	}

	if (ControlledHeroCharacter.Get())
	{
		SetHasMoveInput(true);
		ControlledHeroCharacter->SetHasMoveInput(HasMoveInput());
	}

	const FVector2D InputAxisVector = ActionValue.Get<FVector2D>();

	// 只取控制器朝向的 Yaw，用来把平面输入转换为世界空间移动方向。
	const FRotator ControllerRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(ControllerRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ControllerRotation).GetUnitAxis(EAxis::Y);

	// 把二维输入映射到世界空间，并缓存成朝向，供闪避转向与动画表现复用。
	const FVector MoveDirection = ForwardDirection * InputAxisVector.Y + RightDirection * InputAxisVector.X;
	if (!MoveDirection.IsNearlyZero())
	{
		InputDirection = MoveDirection.Rotation();
	}

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void AActionPlayerController::Input_StopMove()
{
	if (!IsControlledHeroStartupReady(ControlledHeroCharacter))
	{
		InputDirection = FRotator::ZeroRotator;
		SetHasMoveInput(false);
		return;
	}

	InputDirection = FRotator::ZeroRotator;

	if (ControlledHeroCharacter.Get())
	{
		SetHasMoveInput(false);
		ControlledHeroCharacter->SetHasMoveInput(HasMoveInput());
	}

	// 疾跑停止输入后回退到普通跑步状态，避免移动参数残留。
	if (GetMoveState() == EMoveState::FastRun)
	{
		SetMoveState(EMoveState::Run);
	}

	UpdateMovementData();
}

void AActionPlayerController::Input_Look(const FInputActionValue& ActionValue)
{
	if (!IsControlledHeroStartupReady(ControlledHeroCharacter))
	{
		return;
	}

	if (!GetPawn())
	{
		UE_LOG(ActionRPG, Warning, TEXT("AActionPlayerController::Input_Look - No Pawn found!"));
		return;
	}

	const FVector2D LookAxisVector = ActionValue.Get<FVector2D>();
	const UHeroTargetingComponent* HeroTargetingComponent = ControlledHeroCharacter->GetHeroTargetingComponent();
	const bool bTargetLockActive = HeroTargetingComponent && HeroTargetingComponent->IsTargetLockActive();

	if (!bTargetLockActive && LookAxisVector.X != 0.f)
	{
		GetPawn()->AddControllerYawInput(LookAxisVector.X);
	}

	if (LookAxisVector.Y != 0.f)
	{
		GetPawn()->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AActionPlayerController::Input_Walk()
{
	if (!IsControlledHeroStartupReady(ControlledHeroCharacter))
	{
		return;
	}

	if (GetMoveState() == EMoveState::Walk)
	{
		SetMoveState(EMoveState::Run);
	}
	else
	{
		SetMoveState(EMoveState::Walk);
	}

	UpdateMovementData();
}

void AActionPlayerController::UpdateMovementData()
{
	if (!ControlledHeroCharacter.Get())
	{
		return;
	}

	// 移动速度优先读取属性系统中的 MoveSpeed，这样 Buff、Debuff 或成长值都能统一驱动手感。
	float BaseMoveSpeed = 600.f;
	if (const UActionAttributeSetBase* AttributeSet = ControlledHeroCharacter->GetActionAttributeSet())
	{
		BaseMoveSpeed = FMath::Max(AttributeSet->GetMoveSpeed(), 0.f);
	}

	if (GetMoveState() == EMoveState::Walk)
	{
		ControlledHeroCharacter->GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * 0.3f;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingFrictionFactor = 0.8f;
		ControlledHeroCharacter->GetCharacterMovement()->GroundFriction = 4.8f;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingDecelerationWalking = 480.f;
		ControlledHeroCharacter->GetCharacterMovement()->MaxAcceleration = 320.f;
	}

	if (GetMoveState() == EMoveState::Run)
	{
		ControlledHeroCharacter->GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingFrictionFactor = 0.8f;
		ControlledHeroCharacter->GetCharacterMovement()->GroundFriction = 5.f;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingDecelerationWalking = 500.f;
		ControlledHeroCharacter->GetCharacterMovement()->MaxAcceleration = 2048.f;
	}

	if (GetMoveState() == EMoveState::FastRun)
	{
		ControlledHeroCharacter->GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * 1.5f;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingFrictionFactor = 0.7f;
		ControlledHeroCharacter->GetCharacterMovement()->GroundFriction = 5.f;
		ControlledHeroCharacter->GetCharacterMovement()->BrakingDecelerationWalking = 500.f;
		ControlledHeroCharacter->GetCharacterMovement()->MaxAcceleration = 2048.f;
	}
}

void AActionPlayerController::ResetRuntimeStateForHeroStartup()
{
	// 启动链重新进入锁定态时，控制器侧不应继续保留旧的移动输入方向和冲刺状态。
	SetHasMoveInput(false);
	InputDirection = FRotator::ZeroRotator;
	MoveState = EMoveState::Run;
	bIsMotionWrapping = false;

	if (ControlledHeroCharacter.Get())
	{
		ControlledHeroCharacter->SetHasMoveInput(false);
	}
}

void AActionPlayerController::ActionDebug_PrintHeroCombatAbilityRelationshipAudit()
{
	if (!ControlledHeroCharacter.IsValid())
	{
		Debug::Print(TEXT("[GA Audit] 当前控制器没有有效的 Hero 角色。"), FColor::Red, 4.0f);
		return;
	}

	ControlledHeroCharacter->PrintGrantedHeroCombatAbilityRelationshipAudit();
}

void AActionPlayerController::ActionDebug_PrintHeroCombatAbilityDebug(const FString& InputTagName)
{
	if (!ControlledHeroCharacter.IsValid())
	{
		Debug::Print(TEXT("[GA Debug] 当前控制器没有有效的 Hero 角色。"), FColor::Red, 4.0f);
		return;
	}

	const FGameplayTag InputTag =
		UGameplayTagsManager::Get().RequestGameplayTag(FName(*InputTagName), false);
	if (!InputTag.IsValid())
	{
		Debug::Print(
			FString::Printf(TEXT("[GA Debug] 输入标签无效：%s"), *InputTagName),
			FColor::Yellow,
			4.0f);
		return;
	}

	ControlledHeroCharacter->PrintHeroCombatAbilityDebugByInputTag(InputTag);
}
