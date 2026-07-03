// 文件说明：声明 ActionHeroAnimInstance 相关接口。

#pragma once

#include "CoreMinimal.h"
#include "AnimInstance/ActionCharacterAnimInstance.h"
#include "GameplayTagContainer.h"
#include "ActionHeroAnimInstance.generated.h"

class AActionHeroCharacter;
class AActionPlayerController;
class UActionHeroLinkedAnimLayer;

UCLASS()
class ACTIONRPG_API UActionHeroAnimInstance : public UActionCharacterAnimInstance
{
	GENERATED_BODY()

public:
	UActionHeroAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 创建和销毁代理。
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	// 动画实例生命周期函数。
	virtual void NativeBeginPlay() override;
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 读取拥有者英雄角色，仅供表现镜像与调试辅助使用。 */
	AActionHeroCharacter* GetOwningHeroCharacter() { return OwningHeroCharacter; }
	/** 读取拥有者控制器，仅供表现镜像与调试辅助使用。 */
	AActionPlayerController* GetOwningHeroController() { return OwningHeroController; }

public:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bIsFalling;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bHasMoveInput = false;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float InputYawDeltaDegrees = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	float ActorYawDeltaDegrees = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|LocomotionData")
	bool bLargeTurnInputActive = false;

	// 这组字段都只是主 ABP 的只读镜像，不是 linked layer 或 Combat / Idle 切换的正式状态源。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	bool bShouldUseWeaponLinkedLayer = false;

	// 仅用于表现查询与调试镜像，不再作为主 ABP 推导 linked layer 开关的正式状态源。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	FGameplayTag CurrentWeaponTag;

	// 仅用于表现查询与调试镜像；真正的 linked layer 应用与校验由 HeroAssemblyComponent 负责。
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|WeaponLayer")
	TSubclassOf<UActionHeroLinkedAnimLayer> CurrentWeaponLinkedLayerClass;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|References")
	AActionHeroCharacter* OwningHeroCharacter = nullptr;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "AnimData|References")
	AActionPlayerController* OwningHeroController = nullptr;

private:
	bool bLastLoggedShouldUseWeaponLinkedLayer = false;

	bool bLoggedMovingIdleWhileWeaponLayerActive = false;

	bool bLoggedLargeTurnWhileWeaponLayerActive = false;

	FGameplayTag LastLoggedWeaponTag;

	TSubclassOf<UActionHeroLinkedAnimLayer> LastLoggedWeaponLinkedLayerClass;

	EHeroCombatMode LastLoggedCombatMode = EHeroCombatMode::Idle;

	float LastActorYawDegrees = 0.f;
};
