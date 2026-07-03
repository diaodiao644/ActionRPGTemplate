// 文件说明：实现英雄角色相机逻辑组件，用于集中处理镜头模式解析、状态优先级与参数过渡。

#include "Components/Camera/HeroCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "GameBase/ActionPlayerController.h"
#include "GameFramework/SpringArmComponent.h"

UHeroCameraComponent::UHeroCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// 探索状态下，镜头稍远、稍高，给移动和观察留更多空间。
	ExplorationCameraConfig.TargetArmLength = 420.f;
	ExplorationCameraConfig.TargetOffset = FVector(0.f, 0.f, 55.f);
	ExplorationCameraConfig.FieldOfView = 90.f;
	ExplorationCameraConfig.BlendSpeed = 8.f;
	ExplorationCameraConfig.bEnableCameraLag = true;
	ExplorationCameraConfig.CameraLagSpeed = 12.f;

	// 进入战斗后，镜头稍微拉近，保证角色动作更清晰。
	CombatCameraConfig.TargetArmLength = 360.f;
	CombatCameraConfig.TargetOffset = FVector(0.f, 0.f, 60.f);
	CombatCameraConfig.FieldOfView = 86.f;
	CombatCameraConfig.BlendSpeed = 10.f;
	CombatCameraConfig.bEnableCameraLag = true;
	CombatCameraConfig.CameraLagSpeed = 14.f;

	// 锁定目标时镜头略微再拉近一点，并减弱参数波动，保证目标关系更稳定。
	TargetLockCameraConfig.TargetArmLength = 335.f;
	TargetLockCameraConfig.TargetOffset = FVector(0.f, 0.f, 62.f);
	TargetLockCameraConfig.FieldOfView = 84.f;
	TargetLockCameraConfig.BlendSpeed = 12.f;
	TargetLockCameraConfig.bEnableCameraLag = true;
	TargetLockCameraConfig.CameraLagSpeed = 16.f;

	// 防御状态下，进一步微拉近镜头，突出临场压迫感。
	DefenseCameraConfig.TargetArmLength = 340.f;
	DefenseCameraConfig.TargetOffset = FVector(0.f, 0.f, 62.f);
	DefenseCameraConfig.FieldOfView = 84.f;
	DefenseCameraConfig.BlendSpeed = 12.f;
	DefenseCameraConfig.bEnableCameraLag = true;
	DefenseCameraConfig.CameraLagSpeed = 16.f;

	// 闪避期间把镜头稍拉远一点，避免高速位移时角色跑出画面。
	DodgeCameraConfig.TargetArmLength = 390.f;
	DodgeCameraConfig.TargetOffset = FVector(0.f, 0.f, 58.f);
	DodgeCameraConfig.FieldOfView = 92.f;
	DodgeCameraConfig.BlendSpeed = 14.f;
	DodgeCameraConfig.bEnableCameraLag = true;
	DodgeCameraConfig.CameraLagSpeed = 18.f;

	// 特殊切武表现期优先强调演出，因此稍拉近并减小视野。
	SpecialWeaponSwitchCameraConfig.TargetArmLength = 320.f;
	SpecialWeaponSwitchCameraConfig.TargetOffset = FVector(0.f, 0.f, 65.f);
	SpecialWeaponSwitchCameraConfig.FieldOfView = 82.f;
	SpecialWeaponSwitchCameraConfig.BlendSpeed = 16.f;
	SpecialWeaponSwitchCameraConfig.bEnableCameraLag = true;
	SpecialWeaponSwitchCameraConfig.CameraLagSpeed = 20.f;
}

void UHeroCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwnerDependencies();
	ForceRefreshCameraMode();
}

void UHeroCameraComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasValidCameraRig())
	{
		// 若镜头实体引用暂时还没准备好，就继续尝试补缓存，但本帧不再推进镜头参数。
		CacheOwnerDependencies();
		return;
	}

	CurrentCameraMode = ResolveDesiredCameraMode();
	BlendToCameraModeConfig(GetCameraModeConfig(CurrentCameraMode), DeltaTime);

	if (CurrentCameraMode == EHeroCameraMode::TargetLock
		&& CachedHeroTargetingComponent.IsValid()
		&& CachedHeroTargetingComponent->IsTargetLockActive()
		&& CachedHeroController.IsValid())
	{
		AActor* LockedTargetActor = CachedHeroTargetingComponent->GetLockedTargetActor();
		AActionHeroCharacter* HeroCharacter = CachedHeroCharacter.Get();
		if (IsValid(LockedTargetActor) && HeroCharacter != nullptr)
		{
			FVector ToTarget = LockedTargetActor->GetActorLocation() - HeroCharacter->GetActorLocation();
			ToTarget.Z = 0.f;
			if (!ToTarget.IsNearlyZero())
			{
				const FRotator CurrentControlRotation = CachedHeroController->GetControlRotation();
				const FRotator DesiredControlRotation(0.f, ToTarget.GetSafeNormal().Rotation().Yaw, CurrentControlRotation.Roll);
				const FRotator NewControlRotation = FMath::RInterpTo(
					CurrentControlRotation,
					FRotator(CurrentControlRotation.Pitch, DesiredControlRotation.Yaw, CurrentControlRotation.Roll),
					DeltaTime,
					FMath::Max(CachedHeroTargetingComponent->GetTargetLockConfig().ControllerYawInterpSpeed, 0.f));
				CachedHeroController->SetControlRotation(NewControlRotation);
			}
		}
	}
}

void UHeroCameraComponent::InitializeCameraRig(USpringArmComponent* InCameraBoom, UCameraComponent* InFollowCamera)
{
	CameraBoom = InCameraBoom;
	FollowCamera = InFollowCamera;

	// 初始化完成后立刻按当前状态同步一次镜头，避免开场第一帧还停留在硬编码默认值上。
	ForceRefreshCameraMode();
}

void UHeroCameraComponent::ForceRefreshCameraMode()
{
	CacheOwnerDependencies();
	if (!HasValidCameraRig())
	{
		return;
	}

	CurrentCameraMode = ResolveDesiredCameraMode();
	ApplyCameraModeConfigImmediately(GetCameraModeConfig(CurrentCameraMode));
}

void UHeroCameraComponent::CacheOwnerDependencies()
{
	if (!CachedHeroCharacter.IsValid())
	{
		CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	}

	if (CachedHeroCharacter.IsValid() && !CachedHeroCombatComponent.IsValid())
	{
		CachedHeroCombatComponent = CachedHeroCharacter->GetHeroCombatComponent();
	}

	if (CachedHeroCharacter.IsValid() && !CachedHeroTargetingComponent.IsValid())
	{
		CachedHeroTargetingComponent = CachedHeroCharacter->GetHeroTargetingComponent();
	}

	if (CachedHeroCharacter.IsValid() && !CachedHeroController.IsValid())
	{
		CachedHeroController = Cast<AActionPlayerController>(CachedHeroCharacter->GetController());
	}
}

bool UHeroCameraComponent::HasValidCameraRig() const
{
	return IsValid(CameraBoom) && IsValid(FollowCamera);
}

EHeroCameraMode UHeroCameraComponent::ResolveDesiredCameraMode() const
{
	if (CachedHeroCharacter.IsValid())
	{
		if (const UHeroAssemblyComponent* HeroAssemblyComponent = CachedHeroCharacter->GetHeroAssemblyComponent())
		{
			if (!HeroAssemblyComponent->IsHeroSystemsStartupReady())
			{
				// 启动预热未完成前，镜头统一回到探索模式。
				// 这样即使某些战斗状态在失败重试期间短暂残留，也不会把加载阶段镜头切进战斗演出。
				return EHeroCameraMode::Exploration;
			}
		}
	}

	if (CachedHeroCombatComponent.IsValid())
	{
		// 特殊切武表现优先级最高，因为这类镜头通常带有明确的演出目的。
		if (CachedHeroCombatComponent->IsSpecialWeaponSwitchPresentationActive())
		{
			return EHeroCameraMode::SpecialWeaponSwitch;
		}

		if (CachedHeroTargetingComponent.IsValid() && CachedHeroTargetingComponent->IsTargetLockActive())
		{
			return EHeroCameraMode::TargetLock;
		}

		// 闪避和防御本身就是短时强状态，优先压过普通战斗镜头。
		if (CachedHeroCombatComponent->IsDodgeActive())
		{
			return EHeroCameraMode::Dodge;
		}

		if (CachedHeroCombatComponent->IsDefenseActive())
		{
			return EHeroCameraMode::Defense;
		}

		// 只要已经进入战斗模式，就使用战斗镜头。
		if (CachedHeroCombatComponent->GetCombatMode() != EHeroCombatMode::Idle)
		{
			return EHeroCameraMode::Combat;
		}
	}

	// 目前冲刺镜头还没有单独拆模式，后续如果要做跑图镜头或处决专属镜头，可以从这里继续分叉。
	return EHeroCameraMode::Exploration;
}

const FHeroCameraModeConfig& UHeroCameraComponent::GetCameraModeConfig(const EHeroCameraMode InCameraMode) const
{
	switch (InCameraMode)
	{
	case EHeroCameraMode::Combat:
		return CombatCameraConfig;

	case EHeroCameraMode::TargetLock:
		return TargetLockCameraConfig;

	case EHeroCameraMode::Defense:
		return DefenseCameraConfig;

	case EHeroCameraMode::Dodge:
		return DodgeCameraConfig;

	case EHeroCameraMode::SpecialWeaponSwitch:
		return SpecialWeaponSwitchCameraConfig;

	case EHeroCameraMode::Exploration:
	default:
		break;
	}

	return ExplorationCameraConfig;
}

void UHeroCameraComponent::ApplyCameraModeConfigImmediately(const FHeroCameraModeConfig& InConfig)
{
	if (!HasValidCameraRig())
	{
		return;
	}

	// 即时应用用于开场初始化、蓝图热重载或强制刷新时，避免参数渐变导致镜头状态错位。
	CameraBoom->TargetArmLength = InConfig.TargetArmLength;
	CameraBoom->SocketOffset = InConfig.SocketOffset;
	CameraBoom->TargetOffset = InConfig.TargetOffset;
	CameraBoom->bEnableCameraLag = InConfig.bEnableCameraLag;
	CameraBoom->CameraLagSpeed = InConfig.CameraLagSpeed;
	CameraBoom->bEnableCameraRotationLag = InConfig.bEnableCameraRotationLag;
	CameraBoom->CameraRotationLagSpeed = InConfig.CameraRotationLagSpeed;
	FollowCamera->SetFieldOfView(InConfig.FieldOfView);
}

void UHeroCameraComponent::BlendToCameraModeConfig(const FHeroCameraModeConfig& InConfig, const float DeltaTime)
{
	if (!HasValidCameraRig())
	{
		return;
	}

	const float InterpSpeed = FMath::Max(InConfig.BlendSpeed, 0.f);

	// 这里统一用插值推进镜头参数，避免战斗状态切换时镜头“瞬移”。
	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		InConfig.TargetArmLength,
		DeltaTime,
		InterpSpeed);

	CameraBoom->SocketOffset = FMath::VInterpTo(
		CameraBoom->SocketOffset,
		InConfig.SocketOffset,
		DeltaTime,
		InterpSpeed);

	CameraBoom->TargetOffset = FMath::VInterpTo(
		CameraBoom->TargetOffset,
		InConfig.TargetOffset,
		DeltaTime,
		InterpSpeed);

	const float CurrentFieldOfView = FollowCamera->FieldOfView;
	FollowCamera->SetFieldOfView(FMath::FInterpTo(
		CurrentFieldOfView,
		InConfig.FieldOfView,
		DeltaTime,
		InterpSpeed));

	// 延迟开关本身不适合做插值，因此直接按模式切换。
	CameraBoom->bEnableCameraLag = InConfig.bEnableCameraLag;
	CameraBoom->CameraLagSpeed = InConfig.CameraLagSpeed;
	CameraBoom->bEnableCameraRotationLag = InConfig.bEnableCameraRotationLag;
	CameraBoom->CameraRotationLagSpeed = InConfig.CameraRotationLagSpeed;
}
