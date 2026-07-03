// 文件说明：实现玩家英雄角色相关逻辑。

#include "ActionHeroCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"
#include "AnimInstance/Hero/ActionHeroLinkedAnimLayer.h"
#include "Components/Character/HeroAssemblyComponent.h"
#include "Components/Camera/HeroCameraComponent.h"
#include "Components/Combat/HeroAttackComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatInputComponent.h"
#include "Components/Combat/HeroCombatFeedbackComponent.h"
#include "Components/Combat/HeroDefenseComponent.h"
#include "Components/Combat/HeroExecutionCoordinatorComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "Components/Combat/HeroTargetingComponent.h"
#include "Components/Combat/HeroWeaponSwitchComponent.h"
#include "Components/Equipment/HeroEquipmentComponent.h"
#include "Components/Equipment/HeroLoadoutContextComponent.h"
#include "Components/Equipment/HeroLoadoutEffectComponent.h"
#include "Components/Equipment/HeroLoadoutRuntimeComponent.h"
#include "Components/Equipment/HeroLoadoutStateComponent.h"
#include "Components/Attribute/HeroAttributeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DataAssets/Loadout/DataAsset_HeroLoadoutData.h"
#include "Debug/ActionDebugHelper.h"
#include "GameBase/ActionPlayerController.h"
#include "GameBase/ActionPlayerState.h"
#include "GameBase/ActionHUDBase.h"
#include "Items/Weapons/HeroWeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionHeroCharacter, Log, All);

namespace ActionHeroCharacterSocketDefaults
{
	const FName CharacterBodySocketName(TEXT("Socket_Hit_Body"));
	const FName LeftFistSocketName(TEXT("Socket_Hit_LeftFist"));
	const FName RightFistSocketName(TEXT("Socket_Hit_RightFist"));
	const FName LeftFootSocketName(TEXT("Socket_Hit_LeftFoot"));
	const FName RightFootSocketName(TEXT("Socket_Hit_RightFoot"));
	const FName LeftElbowSocketName(TEXT("Socket_Hit_LeftElbow"));
	const FName RightElbowSocketName(TEXT("Socket_Hit_RightElbow"));
	const FName LeftKneeSocketName(TEXT("Socket_Hit_LeftKnee"));
	const FName RightKneeSocketName(TEXT("Socket_Hit_RightKnee"));
}

AActionHeroCharacter::AActionHeroCharacter()
{
	DefaultAttributeInitData.AttackPower = 1;

	// 初始化角色基础碰撞体尺寸。
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 第三人称动作游戏下，角色朝向由移动逻辑驱动，而不是直接跟随控制器旋转。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 初始化角色默认移动参数。
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// 创建第三人称镜头组件。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 创建独立的相机逻辑组件。
	// 从这里开始，Character 只保留相机实体本身，不再负责镜头状态切换与参数细节。
	HeroCameraComponent = CreateDefaultSubobject<UHeroCameraComponent>(TEXT("HeroCameraComponent"));

	// 创建角色战斗与装备核心组件。
	HeroCombatComponent = CreateDefaultSubobject<UHeroCombatComponent>(TEXT("HeroCombatComponent"));
	HeroAssemblyComponent = CreateDefaultSubobject<UHeroAssemblyComponent>(TEXT("HeroAssemblyComponent"));
	HeroCombatInputComponent = CreateDefaultSubobject<UHeroCombatInputComponent>(TEXT("HeroCombatInputComponent"));
	HeroEquipmentComponent = CreateDefaultSubobject<UHeroEquipmentComponent>(TEXT("HeroEquipmentComponent"));
	HeroLoadoutRuntimeComponent = CreateDefaultSubobject<UHeroLoadoutRuntimeComponent>(TEXT("HeroLoadoutRuntimeComponent"));
	HeroLoadoutContextComponent = CreateDefaultSubobject<UHeroLoadoutContextComponent>(TEXT("HeroLoadoutContextComponent"));
	HeroLoadoutStateComponent = CreateDefaultSubobject<UHeroLoadoutStateComponent>(TEXT("HeroLoadoutStateComponent"));
	HeroLoadoutEffectComponent = CreateDefaultSubobject<UHeroLoadoutEffectComponent>(TEXT("HeroLoadoutEffectComponent"));
	HeroAttackComponent = CreateDefaultSubobject<UHeroAttackComponent>(TEXT("HeroAttackComponent"));
	HeroCombatFeedbackComponent = CreateDefaultSubobject<UHeroCombatFeedbackComponent>(TEXT("HeroCombatFeedbackComponent"));
	HeroDefenseComponent = CreateDefaultSubobject<UHeroDefenseComponent>(TEXT("HeroDefenseComponent"));
	HeroWeaponSwitchComponent = CreateDefaultSubobject<UHeroWeaponSwitchComponent>(TEXT("HeroWeaponSwitchComponent"));
	HeroExecutionCoordinatorComponent = CreateDefaultSubobject<UHeroExecutionCoordinatorComponent>(TEXT("HeroExecutionCoordinatorComponent"));
	HeroHitSourceComponent = CreateDefaultSubobject<UHeroHitSourceComponent>(TEXT("HeroHitSourceComponent"));
	HeroTargetingComponent = CreateDefaultSubobject<UHeroTargetingComponent>(TEXT("HeroTargetingComponent"));
	HeroAttributeComponent = CreateDefaultSubobject<UHeroAttributeComponent>(TEXT("HeroAttributeComponent"));

	// 身体命中体在角色构造阶段就常驻创建，并直接挂到固定 Socket 名上。
	// 这样蓝图只需要在骨骼上创建同名 Socket，再调整尺寸和相对位移，不需要再手动补组件。
	auto CreateBodyHitBox = [this](const TCHAR* InComponentName, const FName InSocketName) -> UBoxComponent*
	{
		UBoxComponent* HitBox = CreateDefaultSubobject<UBoxComponent>(InComponentName);
		HitBox->SetupAttachment(GetMesh(), InSocketName);
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		HitBox->SetGenerateOverlapEvents(true);
		HitBox->SetHiddenInGame(true);
		return HitBox;
	};

	CharacterBodyHitBox = CreateBodyHitBox(TEXT("CharacterBody"), ActionHeroCharacterSocketDefaults::CharacterBodySocketName);
	LeftFistHitBox = CreateBodyHitBox(TEXT("LeftFist"), ActionHeroCharacterSocketDefaults::LeftFistSocketName);
	RightFistHitBox = CreateBodyHitBox(TEXT("RightFist"), ActionHeroCharacterSocketDefaults::RightFistSocketName);
	LeftFootHitBox = CreateBodyHitBox(TEXT("LeftFoot"), ActionHeroCharacterSocketDefaults::LeftFootSocketName);
	RightFootHitBox = CreateBodyHitBox(TEXT("RightFoot"), ActionHeroCharacterSocketDefaults::RightFootSocketName);
	LeftElbowHitBox = CreateBodyHitBox(TEXT("LeftElbow"), ActionHeroCharacterSocketDefaults::LeftElbowSocketName);
	RightElbowHitBox = CreateBodyHitBox(TEXT("RightElbow"), ActionHeroCharacterSocketDefaults::RightElbowSocketName);
	LeftKneeHitBox = CreateBodyHitBox(TEXT("LeftKnee"), ActionHeroCharacterSocketDefaults::LeftKneeSocketName);
	RightKneeHitBox = CreateBodyHitBox(TEXT("RightKnee"), ActionHeroCharacterSocketDefaults::RightKneeSocketName);
}

void AActionHeroCharacter::BuildNormalizedWeaponLoadoutDefinitions(TArray<FHeroWeaponLoadoutDefinition>& OutDefinitions) const
{
	// 优先使用角色负载配置资产里已经整理好的固定武器槽配置。
	if (HeroLoadoutData)
	{
		HeroLoadoutData->BuildNormalizedWeaponLoadoutDefinitions(OutDefinitions);
		return;
	}

	// 若角色未配置负载资产，则在代码侧补一份最小可用的固定武器槽默认结构，
	// 保证后续装备组件仍然能拿到完整的空手槽 / 近战槽 / 远程槽 / 混合槽顺序。
	OutDefinitions.Reset();

	auto AddEmptyFixedLoadoutDefinition = [&OutDefinitions](
		const EHeroWeaponLoadoutSlot InLoadoutSlot,
		const bool bInEquipOnSpawn)
	{
		FHeroWeaponLoadoutDefinition LoadoutDefinition;
		LoadoutDefinition.LoadoutSlot = InLoadoutSlot;
		LoadoutDefinition.AllowedWeaponCategory = FHeroWeaponLoadoutDefinition::ResolveRequiredWeaponCategory(InLoadoutSlot);
		LoadoutDefinition.bEquipOnSpawn = bInEquipOnSpawn;
		OutDefinitions.Add(LoadoutDefinition);
	};

	AddEmptyFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::Unarmed, true);
	AddEmptyFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::MeleeWeapon, false);
	AddEmptyFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::RangedWeapon, false);
	AddEmptyFixedLoadoutDefinition(EHeroWeaponLoadoutSlot::HybridWeapon, false);
}

void AActionHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 把角色上真实存在的 CameraBoom / FollowCamera 注册给相机逻辑组件，
	// 这样后续锁定目标、处决镜头、远程瞄准镜头都可以继续沿这条链路扩展。
	if (HeroCameraComponent)
	{
		HeroCameraComponent->InitializeCameraRig(CameraBoom, FollowCamera);
	}
}

void AActionHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 服务器正式接管 Pawn 后，初始化 ASC 并建立战斗系统。
	InitAbilitySystem();
	InitializeHeroSystems();
}

void AActionHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 客户端收到 PlayerState 后，补齐 ASC 与战斗系统引用。
	InitAbilitySystem();
	InitializeHeroSystems();
}

void AActionHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AActionHeroCharacter::InitAbilitySystem()
{
	if (bHeroASCInitialized) return;

	// ASC 与 AttributeSet 都挂在 PlayerState 上，因此先从 PlayerState 取回引用。
	if (AActionPlayerState* PS = Cast<AActionPlayerState>(GetPlayerState()))
	{
		ActionAbilitySystemComponent = PS->GetHeroAbilitySystemComponent();
		ActionAttributeSet = PS->GetHeroAttributeSet();

		// 重新绑定 AvatarActor 与 OwnerActor，保证后续能力查询使用的是当前角色实例。
		if (ActionAbilitySystemComponent)
		{
			ActionAbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// Hero 当前也补一条显式默认属性初始化。
			// 这样在没有独立属性 GE / 成长配置时，战斗链仍能拿到一份明确基础数值。
			InitializeDefaultAttributes();

			// AttributeSet 里有一些基于 ASC 的初始化逻辑，例如属性变更委托的绑定，必须在 ASC 初始化后才调用。
			ActionAttributeSet->InitBindAttributeChangeDelegate();
			HeroAttributeComponent->InitializeWithAbilitySystem(ActionAbilitySystemComponent);

			// HUD 初始化桥已经迁到角色装配桥组件，角色这里只负责在 ASC 就绪后触发。
			if (HeroAssemblyComponent)
			{
				HeroAssemblyComponent->InitPlayerHUD();
			}

			bHeroASCInitialized = true;
		}
	}
}

void AActionHeroCharacter::InitLoadoutData()
{
	if (bLoadoutDataApplied || !HeroLoadoutData || !ActionAbilitySystemComponent)
	{
		return;
	}

	// 先授予“给到就自动激活”的能力。
	for (const FActionAbilitySet& OnGivenAbilitySet : HeroLoadoutData->ActivateOnGivenAbilities)
	{
		if (OnGivenAbilitySet.IsValid())
		{
			ActionAbilitySystemComponent->GrantAbility(OnGivenAbilitySet.AbilityToGrant, OnGivenAbilitySet.InputTag);
		}
	}

	// 再授予响应战斗事件的反应型能力。
	for (const FActionAbilitySet& ReactiveAbilitySet : HeroLoadoutData->ReactiveAbilities)
	{
		if (ReactiveAbilitySet.IsValid())
		{
			ActionAbilitySystemComponent->GrantAbility(ReactiveAbilitySet.AbilityToGrant, ReactiveAbilitySet.InputTag);
		}
	}

	// 最后授予玩家主动输入触发的常驻能力。
	for (const FActionAbilitySet& InputAbilitySet : HeroLoadoutData->PersistentInputAbilities)
	{
		if (InputAbilitySet.IsValid())
		{
			ActionAbilitySystemComponent->GrantAbility(InputAbilitySet.AbilityToGrant, InputAbilitySet.InputTag);
		}
	}

	bLoadoutDataApplied = true;
}

void AActionHeroCharacter::InitializeHeroSystems()
{
	if (!ActionAbilitySystemComponent || !HeroCombatComponent || !HeroEquipmentComponent)
	{
		// 这里要求 ASC、战斗组件、装备组件三者都已就位。
		// 缺任意一个都说明当前角色还不具备完整战斗初始化条件，应等待下一次生命周期回调再重试。
		return;
	}

	// 先确保角色负载数据里的通用能力已经授予完成。
	InitLoadoutData();

	TArray<FHeroWeaponLoadoutDefinition> NormalizedLoadoutDefinitions;
	BuildNormalizedWeaponLoadoutDefinitions(NormalizedLoadoutDefinitions);

	// 当前版本下，空手与武器的战斗动画都统一来自 WeaponDefinition。
	// 因此初始化前先把连段上限清到 0，等装备组件同步完当前槽位后再由武器定义覆盖。
	HeroCombatComponent->UpdateComboMaxIndex(0);

	// 空手槽现在同样必须绑定 WeaponDefinition。
	// 若这里为空，角色虽然仍能初始化成功，但所有空手战斗动画查询都会返回空，需要尽快在数据里补齐。
	const FHeroWeaponLoadoutDefinition* UnarmedLoadoutDefinition =
		NormalizedLoadoutDefinitions.FindByPredicate([](const FHeroWeaponLoadoutDefinition& InDefinition)
		{
			return InDefinition.LoadoutSlot == EHeroWeaponLoadoutSlot::Unarmed;
		});

	if (!UnarmedLoadoutDefinition || UnarmedLoadoutDefinition->DefaultWeaponDefinition.IsNull())
	{
		UE_LOG(LogActionHeroCharacter, Warning,
			TEXT("角色默认固定武器槽配置中未为空手槽绑定 WeaponDefinition。当前框架下，空手战斗动画与战斗表现都应来自空手 WeaponDefinition。"));
	}

	// 把固定武器槽配置交给战斗组件入口，让战斗侧与装备侧在同一时机同步初始化。
	if (HeroAssemblyComponent)
	{
		HeroAssemblyComponent->PrepareForHeroSystemsStartup();
	}
	HeroCombatComponent->InitializeWeaponLoadout(NormalizedLoadoutDefinitions);

	if (HeroLoadoutStateComponent && HeroLoadoutStateComponent->IsWeaponLoadoutStartupReady() && HeroAssemblyComponent)
	{
		HeroAssemblyComponent->HandleWeaponLoadoutStartupReady();
	}
	else if (HeroLoadoutStateComponent && HeroLoadoutStateComponent->HasWeaponLoadoutStartupFailed() && HeroAssemblyComponent)
	{
		HeroAssemblyComponent->HandleWeaponLoadoutStartupFailed(
			EHeroWeaponLoadoutSlot::Invalid,
			HeroLoadoutStateComponent->GetWeaponLoadoutStartupFailureReason());
	}

	// 属性与能力准备好后，立即刷新一遍控制器上的移动参数。
	if (AActionPlayerController* HeroController = Cast<AActionPlayerController>(GetController()))
	{
		HeroController->UpdateMovementData();
	}

	// 输入映射只需要在本地控制端绑定一次。
	if (!bCombatInputConfigured && IsLocallyControlled())
	{
		// 输入绑定明确收口在战斗组件里完成：
		// 1. Character 不直接绑 Pressed / Held / Released；
		// 2. Character 只负责判定“现在是否已经允许绑定本地输入”；
		// 3. 真正的标签状态机与战斗分发继续留在 HeroCombatComponent。
		HeroCombatComponent->HandleCombatInputConfig();
		bCombatInputConfigured = true;
	}
}

void AActionHeroCharacter::SetHasMoveInput(bool bInHasMoveInput)
{
	bHasMoveInput = bInHasMoveInput;
}

bool AActionHeroCharacter::IsHeroSystemsStartupReady() const
{
	return HeroAssemblyComponent && HeroAssemblyComponent->IsHeroSystemsStartupReady();
}

bool AActionHeroCharacter::IsHeroSystemsStartupInProgress() const
{
	return HeroAssemblyComponent && HeroAssemblyComponent->IsHeroSystemsStartupInProgress();
}

bool AActionHeroCharacter::HasHeroSystemsStartupFailed() const
{
	return HeroAssemblyComponent && HeroAssemblyComponent->HasHeroSystemsStartupFailed();
}

FString AActionHeroCharacter::GetHeroSystemsStartupFailureReason() const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->GetHeroSystemsStartupFailureReason()
		: FString();
}

EHeroWeaponLoadoutStartupState AActionHeroCharacter::GetHeroSystemsStartupState() const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->GetHeroSystemsStartupState()
		: EHeroWeaponLoadoutStartupState::None;
}

float AActionHeroCharacter::GetHeroSystemsStartupProgressRatio() const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->GetHeroSystemsStartupProgressRatio()
		: 0.f;
}

int32 AActionHeroCharacter::GetHeroSystemsStartupPendingSlotCount() const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->GetHeroSystemsStartupPendingSlotCount()
		: 0;
}

int32 AActionHeroCharacter::GetHeroSystemsStartupTotalSlotCount() const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->GetHeroSystemsStartupTotalSlotCount()
		: 0;
}

bool AActionHeroCharacter::BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const
{
	return HeroAssemblyComponent
		? HeroAssemblyComponent->BuildHeroLoadoutUISnapshot(OutSnapshot)
		: false;
}

bool AActionHeroCharacter::RetryHeroSystemsStartup()
{
	return HeroAssemblyComponent && HeroAssemblyComponent->RetryHeroSystemsStartup();
}

bool AActionHeroCharacter::ReloadCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(World, true));
	if (CurrentLevelName.IsNone())
	{
		return false;
	}

	// 重新打开当前关卡，让角色、组件和启动链从干净状态重新构建。
	UGameplayStatics::OpenLevel(this, CurrentLevelName);
	return true;
}

void AActionHeroCharacter::PrintGrantedHeroCombatAbilityRelationshipAudit() const
{
	if (!ActionAbilitySystemComponent)
	{
		Debug::Print(TEXT("[GA Audit] 角色当前没有有效的 ActionAbilitySystemComponent。"), FColor::Red, 4.0f);
		return;
	}

	ActionAbilitySystemComponent->PrintHeroCombatAbilityRelationshipAudit();
}

bool AActionHeroCharacter::PrintHeroCombatAbilityDebugByInputTag(const FGameplayTag AbilityInputTag) const
{
	if (!ActionAbilitySystemComponent)
	{
		Debug::Print(TEXT("[GA Debug] 角色当前没有有效的 ActionAbilitySystemComponent。"), FColor::Red, 4.0f);
		return false;
	}

	return ActionAbilitySystemComponent->PrintHeroCombatAbilityDebugByInputTag(AbilityInputTag);
}

bool AActionHeroCharacter::ShouldUseWeaponLinkedLayer() const
{
	return HeroCombatComponent != nullptr
		&& HeroCombatComponent->HasEquippedWeaponLinkedLayerPresentation();
}

TSubclassOf<UActionHeroLinkedAnimLayer> AActionHeroCharacter::GetCurrentWeaponLinkedAnimLayerClass() const
{
	return HeroCombatComponent != nullptr
		? HeroCombatComponent->GetCurrentWeaponLinkedAnimLayerClass()
		: nullptr;
}

void AActionHeroCharacter::ApplyWeaponActorPresentation(AHeroWeaponBase* InWeapon, const bool bIsEquipped) const
{
	if (HeroAssemblyComponent)
	{
		HeroAssemblyComponent->ApplyWeaponActorPresentation(InWeapon, bIsEquipped);
	}
}

void AActionHeroCharacter::RefreshWeaponAnimLayer(const TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass)
{
	if (HeroAssemblyComponent)
	{
		HeroAssemblyComponent->RefreshWeaponAnimLayer(InLinkedAnimLayerClass);
	}
}
