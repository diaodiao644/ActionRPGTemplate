// 文件说明：实现英雄角色装配桥组件，负责启动链协调、HUD 初始化桥和武器表现桥。

#include "Components/Character/HeroAssemblyComponent.h"

#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Camera/HeroCameraComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "GameBase/ActionHUDBase.h"
#include "GameBase/ActionPlayerController.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Items/Weapons/HeroWeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogHeroAssemblyComponent, Log, All);

UHeroAssemblyComponent::UHeroAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UHeroAssemblyComponent::IsHeroSystemsStartupInProgress() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent && LoadoutStateComponent->IsWeaponLoadoutStartupInProgress();
}

EHeroWeaponLoadoutStartupState UHeroAssemblyComponent::GetHeroSystemsStartupState() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent
		? LoadoutStateComponent->GetWeaponLoadoutStartupState()
		: EHeroWeaponLoadoutStartupState::None;
}

float UHeroAssemblyComponent::GetHeroSystemsStartupProgressRatio() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent
		? LoadoutStateComponent->GetWeaponLoadoutStartupProgressRatio()
		: 0.f;
}

int32 UHeroAssemblyComponent::GetHeroSystemsStartupPendingSlotCount() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent
		? LoadoutStateComponent->GetWeaponLoadoutStartupPendingSlotCount()
		: 0;
}

int32 UHeroAssemblyComponent::GetHeroSystemsStartupTotalSlotCount() const
{
	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	return LoadoutStateComponent
		? LoadoutStateComponent->GetWeaponLoadoutStartupTotalSlotCount()
		: 0;
}

void UHeroAssemblyComponent::PrepareForHeroSystemsStartup()
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	if (UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
		LoadoutStateComponent && !LoadoutStateComponent->IsWeaponLoadoutStartupReady())
	{
		bHeroSystemsStartupReady = false;
		bHeroSystemsStartupFailed = false;
		HeroSystemsStartupFailureReason.Reset();
		SetHeroStartupInputEnabled(false);
		BindWeaponLoadoutStartupDelegates();
	}
}

void UHeroAssemblyComponent::HandleWeaponLoadoutStartupReady()
{
	if (bHeroSystemsStartupReady && !bHeroSystemsStartupFailed)
	{
		return;
	}

	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	bHeroSystemsStartupReady = true;
	bHeroSystemsStartupFailed = false;
	HeroSystemsStartupFailureReason.Reset();
	SetHeroStartupInputEnabled(true);

	if (AActionPlayerController* HeroController = Cast<AActionPlayerController>(OwnerHeroCharacter->GetController()))
	{
		HeroController->UpdateMovementData();
	}

	UnbindWeaponLoadoutStartupDelegates();
	UE_LOG(LogHeroAssemblyComponent, Log, TEXT("角色固定武器槽纯预热完成，已解除输入锁定。"));
	OwnerHeroCharacter->OnHeroSystemsStartupReady.Broadcast();
}

void UHeroAssemblyComponent::HandleWeaponLoadoutStartupFailed(
	const EHeroWeaponLoadoutSlot InLoadoutSlot,
	const FString& InFailureReason)
{
	if (bHeroSystemsStartupFailed && HeroSystemsStartupFailureReason == InFailureReason)
	{
		return;
	}

	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	bHeroSystemsStartupReady = false;
	bHeroSystemsStartupFailed = true;
	HeroSystemsStartupFailureReason = InFailureReason;
	SetHeroStartupInputEnabled(false);

	UnbindWeaponLoadoutStartupDelegates();
	UE_LOG(
		LogHeroAssemblyComponent,
		Warning,
		TEXT("角色固定武器槽纯预热失败，继续保持输入锁定。槽位=%d，原因=%s"),
		static_cast<int32>(InLoadoutSlot),
		*InFailureReason);
	OwnerHeroCharacter->OnHeroSystemsStartupFailed.Broadcast(InLoadoutSlot, InFailureReason);
}

bool UHeroAssemblyComponent::RetryHeroSystemsStartup()
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return false;
	}

	UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	if (!LoadoutStateComponent)
	{
		return false;
	}

	bHeroSystemsStartupReady = false;
	bHeroSystemsStartupFailed = false;
	HeroSystemsStartupFailureReason.Reset();
	SetHeroStartupInputEnabled(false);
	BindWeaponLoadoutStartupDelegates();

	const bool bRetryStarted = LoadoutStateComponent->RetryWeaponLoadoutStartup();
	if (!bRetryStarted && LoadoutStateComponent->HasWeaponLoadoutStartupFailed())
	{
		HandleWeaponLoadoutStartupFailed(
			EHeroWeaponLoadoutSlot::Invalid,
			LoadoutStateComponent->GetWeaponLoadoutStartupFailureReason());
	}

	return bRetryStarted;
}

void UHeroAssemblyComponent::InitPlayerHUD() const
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	AActionPlayerController* HeroController = OwnerHeroCharacter->GetController<AActionPlayerController>();
	if (!HeroController)
	{
		return;
	}

	AActionHUDBase* HeroHUD = Cast<AActionHUDBase>(HeroController->GetHUD());
	if (!HeroHUD)
	{
		return;
	}

	HeroHUD->InitPlayerHUD(OwnerHeroCharacter);
}

void UHeroAssemblyComponent::UninitPlayerHUD() const
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	AActionPlayerController* HeroController = OwnerHeroCharacter->GetController<AActionPlayerController>();
	if (!HeroController)
	{
		return;
	}

	if (AActionHUDBase* HeroHUD = Cast<AActionHUDBase>(HeroController->GetHUD()))
	{
		HeroHUD->UninitPlayerHUD();
	}
}

bool UHeroAssemblyComponent::BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const
{
	OutSnapshot = FHeroWeaponLoadoutUISnapshot();

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return false;
	}

	const UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	if (LoadoutStateComponent)
	{
		LoadoutStateComponent->BuildEquipmentLoadoutUISnapshot(OutSnapshot);
	}

	OutSnapshot.bWeaponSwitchBlockedByCooldown =
		OwnerHeroCharacter->GetHeroCombatComponent()
			? OwnerHeroCharacter->GetHeroCombatComponent()->IsWeaponSwitchBlockedByCooldownForUI()
			: false;

	return OwnerHeroCharacter->FindComponentByClass<UHeroEquipmentComponent>() != nullptr
		&& LoadoutStateComponent != nullptr;
}

bool UHeroAssemblyComponent::ApplyWeaponActorPresentation(AHeroWeaponBase* InWeapon, const bool bIsEquipped) const
{
	if (!bIsEquipped)
	{
		// 非当前槽位武器的正式语义已经固定：
		// 永远挂到对应 HolsterSocket，并保持 Hidden + NoCollision。
		// 因此这里不再区分“待命可见展示”等其它旧分支。
		return ApplyWeaponSocketPresentation(InWeapon, false, true);
	}

	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	const UHeroCombatComponent* HeroCombatComponent = OwnerHeroCharacter
		? OwnerHeroCharacter->GetHeroCombatComponent()
		: nullptr;
	// 当前已装备武器始终是“可见武器”，只是挂 Holster 还是挂手上要看 Combat 表现态。
	// 也就是说，装备资格和 socket 表现被故意拆成了两层，不再是一枚 bIsEquipped 直接包办全部语义。
	// 当前已装备武器始终保持可见；真正是挂 Holster 还是 WeaponSocket，由 Combat 表现态决定。
	return ApplyWeaponSocketPresentation(
		InWeapon,
		HeroCombatComponent && HeroCombatComponent->ShouldCurrentEquippedWeaponUseWeaponSocketPresentation(),
		false);
}

bool UHeroAssemblyComponent::ApplyCurrentEquippedWeaponSocketPresentation(
	AHeroWeaponBase* InWeapon,
	const bool bAttachToWeaponSocket) const
{
	return ApplyWeaponSocketPresentation(InWeapon, bAttachToWeaponSocket, false);
}

bool UHeroAssemblyComponent::RefreshCurrentEquippedWeaponSocketPresentation() const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	const UHeroCombatComponent* HeroCombatComponent = OwnerHeroCharacter
		? OwnerHeroCharacter->GetHeroCombatComponent()
		: nullptr;
	if (!HeroCombatComponent)
	{
		return false;
	}

	AHeroWeaponBase* CurrentEquippedWeapon = HeroCombatComponent->GetCurrentEquippedWeapon();
	if (!CurrentEquippedWeapon)
	{
		return true;
	}

	// 这里必须重新向 HeroCombatComponent 查询“当前正式应挂在哪个 socket”，
	// 而不是假设调用方自己还记得上一帧是 Holster 还是 WeaponSocket。
	// 这样 Combat 过渡 Notify、蒙太奇结束兜底和异常恢复都能收束到同一条刷新链。
	return ApplyCurrentEquippedWeaponSocketPresentation(
		CurrentEquippedWeapon,
		HeroCombatComponent->ShouldCurrentEquippedWeaponUseWeaponSocketPresentation());
}

bool UHeroAssemblyComponent::ApplyWeaponSocketPresentation(
	AHeroWeaponBase* InWeapon,
	const bool bAttachToWeaponSocket,
	const bool bShouldHideActor) const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter || !InWeapon || !OwnerHeroCharacter->GetMesh())
	{
		return false;
	}

	const FGameplayTag WeaponSubtypeTag = InWeapon->GetWeaponSubtypeTag();
	const FName AttachSocketName = bAttachToWeaponSocket
		? OwnerHeroCharacter->GetWeaponSocketBySubtypeTag(WeaponSubtypeTag)
		: OwnerHeroCharacter->GetHolsteredWeaponSocketBySubtypeTag(WeaponSubtypeTag);
	if (AttachSocketName.IsNone())
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Warning,
			TEXT("武器表现附加失败：角色未配置对应武器小类别挂点。手持态=%d，隐藏=%d，武器Tag=%s，WeaponSubtypeTag=%s。"),
			bAttachToWeaponSocket ? 1 : 0,
			bShouldHideActor ? 1 : 0,
			*InWeapon->GetWeaponTag().ToString(),
			*WeaponSubtypeTag.ToString());
		return false;
	}

	const FAttachmentTransformRules AttachmentRules(
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		true);

	// 这里的顺序固定是：
	// 1. 先确定当前应挂哪一个 socket；
	// 2. 再把武器表现态切到 EquippedPresentation 或 Holstered；
	// 3. 最后统一处理可见性并重新附挂到角色 Mesh。
	// 这样 Character 层只负责“表现桥”，不会反向接管武器自己的正式运行态。
	// 装配桥只负责“挂到哪个 socket、显不显示”。
	// 非当前槽位武器会回 Holster 并隐藏；当前已装备武器则在 Holster / WeaponSocket 之间切换并保持可见。
	// 碰撞正式状态由武器自身的表现态运行时统一接管。
	InWeapon->ApplyWeaponPresentationState(
		bAttachToWeaponSocket
			? EActionWeaponPresentationState::EquippedPresentation
			: EActionWeaponPresentationState::Holstered);
	InWeapon->SetActorHiddenInGame(bShouldHideActor);
	InWeapon->AttachToComponent(
		OwnerHeroCharacter->GetMesh(),
		AttachmentRules,
		AttachSocketName);
	return true;
}

bool UHeroAssemblyComponent::ApplyCurrentWeaponVisualState(
	AHeroWeaponBase* InWeapon,
	const UDataAsset_WeaponDefinition* InWeaponDefinition,
	const bool bIsEquipped)
{
	if (InWeapon && !ApplyWeaponActorPresentation(InWeapon, bIsEquipped))
	{
		return false;
	}

	RefreshWeaponAnimLayer(InWeaponDefinition ? InWeaponDefinition->GetLinkedAnimLayerClass() : nullptr);
	const TSubclassOf<UActionHeroLinkedAnimLayer> TargetLinkedAnimLayerClass =
		InWeaponDefinition ? InWeaponDefinition->GetLinkedAnimLayerClass() : nullptr;
	const bool bLinkedLayerApplied = ValidateCurrentWeaponLinkedLayerApplied(TargetLinkedAnimLayerClass);
	const UHeroCombatComponent* HeroCombatComponent = GetOwningHeroCharacter()
		? GetOwningHeroCharacter()->GetHeroCombatComponent()
		: nullptr;
	// linked layer 是否应启用只认 HeroCombatComponent 的正式状态源；这里不再自行推导武器 Tag 口径。
	const bool bShouldUseWeaponLinkedLayer = HeroCombatComponent
		&& HeroCombatComponent->HasEquippedWeaponLinkedLayerPresentation();
	if (bShouldUseWeaponLinkedLayer)
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Log,
			TEXT("weapon_linked_layer_runtime_applied weapon_tag=%s linked_layer=%s should_use=%s linked_layer_applied=%s is_equipped=%s"),
			InWeaponDefinition ? *InWeaponDefinition->WeaponTag.ToString() : TEXT("None"),
			*GetNameSafe(TargetLinkedAnimLayerClass),
			bShouldUseWeaponLinkedLayer ? TEXT("true") : TEXT("false"),
			bLinkedLayerApplied ? TEXT("true") : TEXT("false"),
			bIsEquipped ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Log,
			TEXT("weapon_linked_layer_runtime_disabled weapon_tag=%s linked_layer=%s should_use=false linked_layer_applied=%s is_equipped=%s"),
			InWeaponDefinition ? *InWeaponDefinition->WeaponTag.ToString() : TEXT("None"),
			*GetNameSafe(TargetLinkedAnimLayerClass),
			bLinkedLayerApplied ? TEXT("true") : TEXT("false"),
			bIsEquipped ? TEXT("true") : TEXT("false"));
	}

	return bLinkedLayerApplied;
}

void UHeroAssemblyComponent::RefreshWeaponAnimLayer(const TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	USkeletalMeshComponent* HeroMesh = OwnerHeroCharacter ? OwnerHeroCharacter->GetMesh() : nullptr;
	if (!OwnerHeroCharacter || !HeroMesh)
	{
		UE_LOG(LogHeroAssemblyComponent, Warning, TEXT("武器动画层刷新失败：角色或 Mesh 无效。"));
		return;
	}

	if (!HeroMesh->GetAnimInstance())
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Warning,
			TEXT("武器动画层刷新失败：AnimInstance 无效。目标层=%s"),
			*GetNameSafe(InLinkedAnimLayerClass));
		return;
	}

	if (CurrentWeaponLinkedAnimLayer == InLinkedAnimLayerClass)
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Verbose,
			TEXT("武器动画层刷新检测到同层重挂。Layer=%s"),
			*GetNameSafe(InLinkedAnimLayerClass));
	}

	if (CurrentWeaponLinkedAnimLayer)
	{
		HeroMesh->UnlinkAnimClassLayers(CurrentWeaponLinkedAnimLayer);
		CurrentWeaponLinkedAnimLayer = nullptr;
	}

	// linked layer 的正式开关状态源不在这里。
	// 装配桥只做“当前目标层是什么，就按目标层去 link / unlink / 校验”，
	// 是否应该启用武器层仍只认 HeroCombatComponent 的镜像结果。
	if (!InLinkedAnimLayerClass)
	{
		UE_LOG(LogHeroAssemblyComponent, Log, TEXT("武器动画层已卸载，角色回退到基础动画层。"));
		return;
	}

	HeroMesh->LinkAnimClassLayers(InLinkedAnimLayerClass);
	CurrentWeaponLinkedAnimLayer = InLinkedAnimLayerClass;
	if (ValidateCurrentWeaponLinkedLayerApplied(InLinkedAnimLayerClass))
	{
		UE_LOG(LogHeroAssemblyComponent, Log, TEXT("武器动画层已刷新并接管成功。Layer=%s"), *GetNameSafe(InLinkedAnimLayerClass));
	}
}

UAnimInstance* UHeroAssemblyComponent::GetCurrentWeaponLinkedLayerInstance() const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	USkeletalMeshComponent* HeroMesh = OwnerHeroCharacter ? OwnerHeroCharacter->GetMesh() : nullptr;
	if (!HeroMesh || !CurrentWeaponLinkedAnimLayer)
	{
		return nullptr;
	}

	return HeroMesh->GetLinkedAnimLayerInstanceByClass(CurrentWeaponLinkedAnimLayer);
}

bool UHeroAssemblyComponent::ValidateCurrentWeaponLinkedLayerApplied(
	const TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass) const
{
	const AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	USkeletalMeshComponent* HeroMesh = OwnerHeroCharacter ? OwnerHeroCharacter->GetMesh() : nullptr;
	if (!HeroMesh)
	{
		UE_LOG(LogHeroAssemblyComponent, Warning, TEXT("武器动画层校验失败：角色 Mesh 无效。目标层=%s"), *GetNameSafe(InLinkedAnimLayerClass));
		return false;
	}

	if (!InLinkedAnimLayerClass)
	{
		return GetCurrentWeaponLinkedLayerInstance() == nullptr;
	}

	if (!HeroMesh->GetAnimInstance())
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Warning,
			TEXT("武器动画层校验失败：主 AnimInstance 无效。目标层=%s"),
			*GetNameSafe(InLinkedAnimLayerClass));
		return false;
	}

	UAnimInstance* LinkedLayerInstance = HeroMesh->GetLinkedAnimLayerInstanceByClass(InLinkedAnimLayerClass);
	if (!LinkedLayerInstance)
	{
		UE_LOG(
			LogHeroAssemblyComponent,
			Warning,
			TEXT("武器动画层未真正接管：主 ABP 没有接住该 LinkedLayer，或运行时未实例化。主Anim=%s，目标层=%s"),
			*GetNameSafe(HeroMesh->GetAnimInstance()->GetClass()),
			*GetNameSafe(InLinkedAnimLayerClass));
		return false;
	}

	UE_LOG(
		LogHeroAssemblyComponent,
		Verbose,
		TEXT("武器动画层运行时校验成功。主Anim=%s，目标层=%s，实例=%s"),
		*GetNameSafe(HeroMesh->GetAnimInstance()->GetClass()),
		*GetNameSafe(InLinkedAnimLayerClass),
		*GetNameSafe(LinkedLayerInstance->GetClass()));
	return true;
}

void UHeroAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindWeaponLoadoutStartupDelegates();
	UninitPlayerHUD();
	CurrentWeaponLinkedAnimLayer = nullptr;
	CachedHeroCharacter.Reset();

	Super::EndPlay(EndPlayReason);
}

AActionHeroCharacter* UHeroAssemblyComponent::GetOwningHeroCharacter() const
{
	if (CachedHeroCharacter.IsValid())
	{
		return CachedHeroCharacter.Get();
	}

	CachedHeroCharacter = Cast<AActionHeroCharacter>(GetOwner());
	return CachedHeroCharacter.Get();
}

UHeroLoadoutStateComponent* UHeroAssemblyComponent::GetOwningHeroLoadoutStateComponent() const
{
	return GetOwner()
		? GetOwner()->FindComponentByClass<UHeroLoadoutStateComponent>()
		: nullptr;
}

void UHeroAssemblyComponent::BindWeaponLoadoutStartupDelegates()
{
	UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	if (!LoadoutStateComponent)
	{
		return;
	}

	UnbindWeaponLoadoutStartupDelegates();

	EquipmentStartupReadyHandle = LoadoutStateComponent->OnWeaponLoadoutStartupReady().AddUObject(
		this,
		&ThisClass::HandleWeaponLoadoutStartupReady);
	EquipmentStartupFailedHandle = LoadoutStateComponent->OnWeaponLoadoutStartupFailed().AddUObject(
		this,
		&ThisClass::HandleWeaponLoadoutStartupFailed);
}

void UHeroAssemblyComponent::UnbindWeaponLoadoutStartupDelegates()
{
	UHeroLoadoutStateComponent* LoadoutStateComponent = GetOwningHeroLoadoutStateComponent();
	if (!LoadoutStateComponent)
	{
		EquipmentStartupReadyHandle.Reset();
		EquipmentStartupFailedHandle.Reset();
		return;
	}

	if (EquipmentStartupReadyHandle.IsValid())
	{
		LoadoutStateComponent->OnWeaponLoadoutStartupReady().Remove(EquipmentStartupReadyHandle);
		EquipmentStartupReadyHandle.Reset();
	}

	if (EquipmentStartupFailedHandle.IsValid())
	{
		LoadoutStateComponent->OnWeaponLoadoutStartupFailed().Remove(EquipmentStartupFailedHandle);
		EquipmentStartupFailedHandle.Reset();
	}
}

void UHeroAssemblyComponent::SetHeroStartupInputEnabled(const bool bEnabled)
{
	AActionHeroCharacter* OwnerHeroCharacter = GetOwningHeroCharacter();
	if (!OwnerHeroCharacter)
	{
		return;
	}

	if (AController* OwnerController = OwnerHeroCharacter->GetController())
	{
		OwnerController->SetIgnoreMoveInput(!bEnabled);
		OwnerController->SetIgnoreLookInput(!bEnabled);
	}

	if (UHeroCombatComponent* HeroCombatComponent = OwnerHeroCharacter->GetHeroCombatComponent();
		HeroCombatComponent && !bEnabled)
	{
		HeroCombatComponent->ResetRuntimeStateForHeroStartup();
	}

	if (AActionPlayerController* HeroController = Cast<AActionPlayerController>(OwnerHeroCharacter->GetController()))
	{
		if (!bEnabled)
		{
			HeroController->ResetRuntimeStateForHeroStartup();
		}
	}

	if (UCharacterMovementComponent* MovementComponent = OwnerHeroCharacter->GetCharacterMovement())
	{
		if (bEnabled)
		{
			MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
		}
		else
		{
			MovementComponent->DisableMovement();
		}
	}

	if (UHeroCameraComponent* HeroCameraComponent = OwnerHeroCharacter->GetHeroCameraComponent())
	{
		HeroCameraComponent->ForceRefreshCameraMode();
	}
}
