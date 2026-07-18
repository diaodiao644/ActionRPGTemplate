#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ActionType/ActionLoadoutTypes.h"
#include "MVVM_HeroLoadoutSlot.generated.h"

/**
 * 单槽装备域 HUD 只读 ViewModel。
 * 它只消费 `FHeroWeaponLoadoutSlotUISnapshot`，不自持新的装备域正式状态。
 */
UCLASS()
class ACTIONRPG_API UMVVM_HeroLoadoutSlot : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UMVVM_HeroLoadoutSlot();

	/** 把一份单槽 UI 快照拷贝进当前 ViewModel，并刷新对应的 FieldNotify 显示字段。它只做显示刷新，不反向推动装备域正式状态。 */
	void ApplySnapshot(const FHeroWeaponLoadoutSlotUISnapshot& InSnapshot);

	/** 以下 getter 返回的都是单槽 UI 快照结果，不是装备域新的正式状态源。 */
	EHeroWeaponLoadoutSlot GetLoadoutSlot() const { return LoadoutSlot; }
	EHeroWeaponCategory GetAllowedWeaponCategory() const { return AllowedWeaponCategory; }
	FGameplayTag GetWeaponTag() const { return WeaponTag; }
	const FString& GetWeaponLabel() const { return WeaponLabel; }
	EActionWeaponPropertyType GetWeaponPropertyType() const { return WeaponPropertyType; }
	bool GetbHasAssignedWeaponDefinition() const { return bHasAssignedWeaponDefinition; }
	bool GetbRuntimeReady() const { return bRuntimeReady; }
	bool GetbIsEquipped() const { return bIsEquipped; }
	bool GetbSupportsProjectileSwitching() const { return bSupportsProjectileSwitching; }
	FGameplayTag GetSelectedProjectileConfigTag() const { return SelectedProjectileConfigTag; }

protected:
	/** 以下字段都来自 `FHeroWeaponLoadoutSlotUISnapshot` 的展示快照。 */
	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的固定槽位结果。它只服务展示，不是新的装备状态源。"))
	EHeroWeaponLoadoutSlot LoadoutSlot = EHeroWeaponLoadoutSlot::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的允许武器类别结果。"))
	EHeroWeaponCategory AllowedWeaponCategory = EHeroWeaponCategory::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的当前武器标签结果。它只是展示快照，不是新的语义权威源。"))
	FGameplayTag WeaponTag = ActionGameplayTags::Player_Weapon_Unarmed_Default;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的当前武器显示名称。"))
	FString WeaponLabel;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的武器属性类型结果。"))
	EActionWeaponPropertyType WeaponPropertyType = EActionWeaponPropertyType::Mundane;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的“是否已分配 WeaponDefinition”结果。它只服务展示。"))
	bool bHasAssignedWeaponDefinition = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的 runtime ready 展示结果。它只表示当前 UI 快照结果，不是新的正式状态源。"))
	bool bRuntimeReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的“当前是否已正式装备”展示结果。它是快照，不是第二套装备状态。"))
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的“是否支持切换发射物”展示结果。"))
	bool bSupportsProjectileSwitching = false;

	UPROPERTY(BlueprintReadOnly, Category = "MVVM|HeroLoadoutSlot", FieldNotify, Getter, meta = (ToolTip = "单槽 UI 快照里的当前已选发射物配置标签展示结果。它只读自正式 context 状态，不是新的正式状态源。"))
	FGameplayTag SelectedProjectileConfigTag;
};
