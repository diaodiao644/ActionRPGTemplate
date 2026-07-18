// 文件说明：实现 ActionHeroAnimInstance 相关逻辑。


#include "AnimInstance/Hero/ActionHeroAnimInstance.h"
#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "GameBase/ActionPlayerController.h"
UActionHeroAnimInstance::UActionHeroAnimInstance(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
}

FAnimInstanceProxy* UActionHeroAnimInstance::CreateAnimInstanceProxy()
{
	return new FHeroAnimInstanceProxy(this);
}

void UActionHeroAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UActionHeroAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
	if (OwningHeroCharacter)
	{
		OwningHeroController = Cast<AActionPlayerController>(OwningHeroCharacter->GetController());
	}
}

void UActionHeroAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if(GetOwningCharacter())
	{
		OwningHeroCharacter = Cast<AActionHeroCharacter>(GetOwningCharacter());
		if (OwningHeroCharacter)
		{
			LastActorYawDegrees = OwningHeroCharacter->GetActorRotation().Yaw;
		}
	}

}

void UActionHeroAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (OwningHeroCharacter && OwningHeroCharacter->GetHeroCombatComponent())
	{
		if (UHeroCombatComponent* HeroCombatComponent = OwningHeroCharacter->GetHeroCombatComponent())
		{
			// 主 ABP 现在只从 HeroCombatComponent 读取正式镜像：
			// linked layer 是否应启用、当前武器 Tag、当前目标层类都由组件侧决定，
			// AnimInstance 不再自己通过 WeaponTag、速度超时等线索反推正式战斗态。
			bShouldUseWeaponLinkedLayer = HeroCombatComponent->HasEquippedWeaponLinkedLayerPresentation();
			CurrentWeaponTag = HeroCombatComponent->GetCurrentEquippedWeaponTag();
			CurrentWeaponLinkedLayerClass = HeroCombatComponent->GetCurrentWeaponLinkedAnimLayerClass();
			const bool bWeaponSwitchPresentationActive = HeroCombatComponent->IsSpecialWeaponSwitchPresentationActive();
			const bool bCombatModeTransitionActive = HeroCombatComponent->IsCombatModeTransitionPresentationActive();

			if (OwningHeroController)
			{
				bHasMoveInput = OwningHeroController->HasMoveInput();
				if (bHasMoveInput)
				{
					const float ActorYaw = OwningHeroCharacter->GetActorRotation().Yaw;
					const float InputYaw = OwningHeroController->GetInputDirection().Yaw;
					InputYawDeltaDegrees = FMath::FindDeltaAngleDegrees(ActorYaw, InputYaw);
				}
				else
				{
					InputYawDeltaDegrees = 0.f;
				}
			}
			else
			{
				bHasMoveInput = false;
				InputYawDeltaDegrees = 0.f;
			}

			const float CurrentActorYawDegrees = OwningHeroCharacter->GetActorRotation().Yaw;
			ActorYawDeltaDegrees = FMath::FindDeltaAngleDegrees(LastActorYawDegrees, CurrentActorYawDegrees);
			LastActorYawDegrees = CurrentActorYawDegrees;
			// 这组转向量只服务表现消费和诊断：
			// 它们帮助蓝图识别“大转向输入下为什么看起来退回 Idle / 静止 pose”，
			// 但不再参与 Combat -> Idle 正式退出裁决。
			bLargeTurnInputActive =
				bHasMoveInput
				&& GroundSpeed > 5.f
				&& FMath::Abs(InputYawDeltaDegrees) >= 60.f;

			if (bLastLoggedShouldUseWeaponLinkedLayer != bShouldUseWeaponLinkedLayer
				|| LastLoggedWeaponTag != CurrentWeaponTag
				|| LastLoggedWeaponLinkedLayerClass != CurrentWeaponLinkedLayerClass
				|| LastLoggedCombatMode != CombatMode)
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("weapon_linked_layer_runtime_applied weapon_tag=%s linked_layer=%s should_use=%s combat_mode=%d ground_speed=%.2f falling=%s"),
					*CurrentWeaponTag.ToString(),
					*GetNameSafe(CurrentWeaponLinkedLayerClass),
					bShouldUseWeaponLinkedLayer ? TEXT("true") : TEXT("false"),
					static_cast<int32>(CombatMode),
					GroundSpeed,
					bIsFalling ? TEXT("true") : TEXT("false"));

				bLastLoggedShouldUseWeaponLinkedLayer = bShouldUseWeaponLinkedLayer;
				LastLoggedWeaponTag = CurrentWeaponTag;
				LastLoggedWeaponLinkedLayerClass = CurrentWeaponLinkedLayerClass;
				LastLoggedCombatMode = CombatMode;
			}

			const bool bMovingButStillIdleWithWeaponLayer =
				bShouldUseWeaponLinkedLayer
				&& CombatMode == EHeroCombatMode::Idle
				&& !bIsFalling
				&& GroundSpeed > 5.f
				&& !bWeaponSwitchPresentationActive
				&& !bCombatModeTransitionActive;
			if (bMovingButStillIdleWithWeaponLayer)
			{
				if (!bLoggedMovingIdleWhileWeaponLayerActive)
				{
					// 这组日志只在“已脱离切武/CombatModeTransition 交界后仍持续停在 Idle”时输出。
					// 它帮助定位主 ABP / linked layer 的表现消费问题，不替代正式状态源裁决。
					UE_LOG(
						LogTemp,
						Log,
						TEXT("weapon_linked_layer_move_idle_suspect weapon_tag=%s linked_layer=%s should_use=%s combat_mode=%d ground_speed=%.2f move_state=%d weapon_switch_presentation=%s combat_transition=%s"),
						*CurrentWeaponTag.ToString(),
						*GetNameSafe(CurrentWeaponLinkedLayerClass),
						bShouldUseWeaponLinkedLayer ? TEXT("true") : TEXT("false"),
						static_cast<int32>(CombatMode),
						GroundSpeed,
						static_cast<int32>(MoveState),
						bWeaponSwitchPresentationActive ? TEXT("true") : TEXT("false"),
						bCombatModeTransitionActive ? TEXT("true") : TEXT("false"));
					UE_LOG(
						LogTemp,
						Log,
						TEXT("weapon_linked_layer_turn_diagnostic weapon_tag=%s linked_layer=%s has_move_input=%s large_turn=%s input_yaw_delta=%.2f actor_yaw_delta=%.2f combat_mode=%d ground_speed=%.2f move_state=%d weapon_switch_presentation=%s combat_transition=%s"),
						*CurrentWeaponTag.ToString(),
						*GetNameSafe(CurrentWeaponLinkedLayerClass),
						bHasMoveInput ? TEXT("true") : TEXT("false"),
						bLargeTurnInputActive ? TEXT("true") : TEXT("false"),
						InputYawDeltaDegrees,
						ActorYawDeltaDegrees,
						static_cast<int32>(CombatMode),
						GroundSpeed,
						static_cast<int32>(MoveState),
						bWeaponSwitchPresentationActive ? TEXT("true") : TEXT("false"),
						bCombatModeTransitionActive ? TEXT("true") : TEXT("false"));
				}

				bLoggedMovingIdleWhileWeaponLayerActive = true;
			}
			else
			{
				bLoggedMovingIdleWhileWeaponLayerActive = false;
			}

			const bool bShouldLogLargeTurnDiagnostic =
				bShouldUseWeaponLinkedLayer
				&& !bIsFalling
				&& GroundSpeed > 5.f
				&& !bWeaponSwitchPresentationActive
				&& !bCombatModeTransitionActive
				&& bLargeTurnInputActive;
			if (bShouldLogLargeTurnDiagnostic)
			{
				if (!bLoggedLargeTurnWhileWeaponLayerActive)
				{
					UE_LOG(
						LogTemp,
						Log,
						TEXT("weapon_linked_layer_turn_diagnostic weapon_tag=%s linked_layer=%s has_move_input=%s large_turn=%s input_yaw_delta=%.2f actor_yaw_delta=%.2f combat_mode=%d ground_speed=%.2f move_state=%d weapon_switch_presentation=%s combat_transition=%s"),
						*CurrentWeaponTag.ToString(),
						*GetNameSafe(CurrentWeaponLinkedLayerClass),
						bHasMoveInput ? TEXT("true") : TEXT("false"),
						bLargeTurnInputActive ? TEXT("true") : TEXT("false"),
						InputYawDeltaDegrees,
						ActorYawDeltaDegrees,
						static_cast<int32>(CombatMode),
						GroundSpeed,
						static_cast<int32>(MoveState),
						bWeaponSwitchPresentationActive ? TEXT("true") : TEXT("false"),
						bCombatModeTransitionActive ? TEXT("true") : TEXT("false"));
				}

				bLoggedLargeTurnWhileWeaponLayerActive = true;
			}
			else
			{
				bLoggedLargeTurnWhileWeaponLayerActive = false;
			}
		}
	}
	else
	{
		bShouldUseWeaponLinkedLayer = false;
		bHasMoveInput = false;
		InputYawDeltaDegrees = 0.f;
		ActorYawDeltaDegrees = 0.f;
		bLargeTurnInputActive = false;
		CurrentWeaponTag = FGameplayTag();
		CurrentWeaponLinkedLayerClass = nullptr;
		bLoggedMovingIdleWhileWeaponLayerActive = false;
		bLoggedLargeTurnWhileWeaponLayerActive = false;
	}
}

void FHeroAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	if (UActionHeroAnimInstance* ActionAnimInstance = Cast<UActionHeroAnimInstance>(InAnimInstance))
	{
		// Proxy 侧继续只搬运运动学镜像，不额外推导 linked layer 或 Combat/Idle 切换语义。
		if (AActionHeroCharacter* OwningHero = ActionAnimInstance->GetOwningHeroCharacter())
		{
			bIsFalling = OwningHero->GetCharacterMovement()->IsFalling();
		}

		if (AActionPlayerController* OwningController = ActionAnimInstance->GetOwningHeroController())
		{
			MoveState = OwningController->GetMoveState();
		}

	}

}

void FHeroAnimInstanceProxy::Update(float DeltaSeconds)
{
	Super::Update(DeltaSeconds);
}

void FHeroAnimInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	Super::PostUpdate(InAnimInstance);

	if (UActionHeroAnimInstance* ActionAnimInstance = Cast<UActionHeroAnimInstance>(InAnimInstance))
	{
		ActionAnimInstance->bIsFalling = this->bIsFalling;
		ActionAnimInstance->MoveState = this->MoveState;
	}

}
