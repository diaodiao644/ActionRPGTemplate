// 文件说明：声明英雄角色装配桥组件，负责启动链协调、HUD 初始化桥和武器表现桥。

#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "Components/PawnExtensionComponentBase.h"
#include "HeroAssemblyComponent.generated.h"

class AActionHeroCharacter;
class AHeroWeaponBase;
class UActionHeroLinkedAnimLayer;
class UAnimInstance;
class UDataAsset_WeaponDefinition;
class UHeroLoadoutStateComponent;

/**
 * 英雄角色装配桥组件。
 * 负责角色层的启动链协作、HUD 初始化桥以及武器表现桥，
 * 避免这些跨系统装配细节继续堆在 Character 本体里。
 * 当前粒度已经稳定，后续默认冻结，不再新增新的 Character 桥接拆分。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroAssemblyComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroAssemblyComponent();

	bool IsHeroSystemsStartupReady() const { return bHeroSystemsStartupReady; }
	bool IsHeroSystemsStartupInProgress() const;
	bool HasHeroSystemsStartupFailed() const { return bHeroSystemsStartupFailed; }
	const FString& GetHeroSystemsStartupFailureReason() const { return HeroSystemsStartupFailureReason; }
	EHeroWeaponLoadoutStartupState GetHeroSystemsStartupState() const;
	float GetHeroSystemsStartupProgressRatio() const;
	int32 GetHeroSystemsStartupPendingSlotCount() const;
	int32 GetHeroSystemsStartupTotalSlotCount() const;

	void PrepareForHeroSystemsStartup();
	void HandleWeaponLoadoutStartupReady();
	void HandleWeaponLoadoutStartupFailed(EHeroWeaponLoadoutSlot InLoadoutSlot, const FString& InFailureReason);
	bool RetryHeroSystemsStartup();

	void InitPlayerHUD() const;
	void UninitPlayerHUD() const;

	bool BuildHeroLoadoutUISnapshot(FHeroWeaponLoadoutUISnapshot& OutSnapshot) const;
	// 武器表现桥只负责两类结果：
	// 1. 非当前槽位武器：Holster + Hidden + NoCollision
	// 2. 当前已装备武器：根据 Combat 表现态挂到 HolsterSocket 或 WeaponSocket，并保持可见
	bool ApplyWeaponActorPresentation(AHeroWeaponBase* InWeapon, bool bIsEquipped) const;
	/** 只切当前已装备武器的 HolsterSocket / WeaponSocket 挂点，不重新决定“谁是当前装备武器”。 */
	bool ApplyCurrentEquippedWeaponSocketPresentation(AHeroWeaponBase* InWeapon, bool bAttachToWeaponSocket) const;
	/** 按当前角色正式战斗表现态，重新把当前已装备武器落到正确的挂点表现。 */
	bool RefreshCurrentEquippedWeaponSocketPresentation() const;
	bool ApplyCurrentWeaponVisualState(
		AHeroWeaponBase* InWeapon,
		const UDataAsset_WeaponDefinition* InWeaponDefinition,
		bool bIsEquipped);
	/** 按当前正式层类执行 link / unlink，不在这里平行维护 linked layer 开关状态源。 */
	void RefreshWeaponAnimLayer(TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass);
	UAnimInstance* GetCurrentWeaponLinkedLayerInstance() const;
	/** 校验当前 mesh 是否真的接住了目标 linked layer，供运行时桥接链做一致性检查。 */
	bool ValidateCurrentWeaponLinkedLayerApplied(TSubclassOf<UActionHeroLinkedAnimLayer> InLinkedAnimLayerClass) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UHeroLoadoutStateComponent* GetOwningHeroLoadoutStateComponent() const;
	bool ApplyWeaponSocketPresentation(
		AHeroWeaponBase* InWeapon,
		bool bAttachToWeaponSocket,
		bool bShouldHideActor) const;

	void BindWeaponLoadoutStartupDelegates();
	void UnbindWeaponLoadoutStartupDelegates();
	void SetHeroStartupInputEnabled(bool bEnabled);

protected:
	bool bHeroSystemsStartupReady = false;
	bool bHeroSystemsStartupFailed = false;
	FString HeroSystemsStartupFailureReason;

	UPROPERTY(Transient)
	TSubclassOf<UActionHeroLinkedAnimLayer> CurrentWeaponLinkedAnimLayer;

	FDelegateHandle EquipmentStartupReadyHandle;
	FDelegateHandle EquipmentStartupFailedHandle;

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
};
