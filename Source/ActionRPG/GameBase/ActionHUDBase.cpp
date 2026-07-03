// 文件说明：实现 ActionHUDBase 相关逻辑。


#include "GameBase/ActionHUDBase.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Attribute/HeroAttributeComponent.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroCombatFeedbackComponent.h"
#include "UMG/MVVM/MVVM_HeroAttribute.h"
#include "UMG/MVVM/MVVM_HeroLoadout.h"

AActionHUDBase::AActionHUDBase()
	:Super()
{
}

void AActionHUDBase::InitPlayerHUD(AActionHeroCharacter* InHeroCharacter)
{
	if (BoundCombatFeedbackComponent)
	{
		BoundCombatFeedbackComponent->OnCombatFeedbackHUDDisplayEvent.RemoveDynamic(
			this,
			&ThisClass::HandleCombatFeedbackHUDDisplayEvent);
		BoundCombatFeedbackComponent = nullptr;
	}

	BoundHeroCharacter = InHeroCharacter;
	InitMVVM(InHeroCharacter);

	BoundCombatFeedbackComponent = InHeroCharacter
		? InHeroCharacter->GetHeroCombatFeedbackComponent()
		: nullptr;
	if (BoundCombatFeedbackComponent)
	{
		BoundCombatFeedbackComponent->OnCombatFeedbackHUDDisplayEvent.AddDynamic(
			this,
			&ThisClass::HandleCombatFeedbackHUDDisplayEvent);
	}

	k2_CreateWidgetForPlayer();
}

void AActionHUDBase::UninitPlayerHUD()
{
	if (BoundCombatFeedbackComponent)
	{
		BoundCombatFeedbackComponent->OnCombatFeedbackHUDDisplayEvent.RemoveDynamic(
			this,
			&ThisClass::HandleCombatFeedbackHUDDisplayEvent);
		BoundCombatFeedbackComponent = nullptr;
	}

	k2_DestroyWidgetForPlayer();

	if (HeroAttributeViewModel)
	{
		HeroAttributeViewModel->UninitializeMVVM();
	}
	if (HeroLoadoutViewModel)
	{
		HeroLoadoutViewModel->UninitializeMVVM();
	}
	HeroAttributeViewModel = nullptr;
	HeroLoadoutViewModel = nullptr;
	BoundHeroCharacter = nullptr;
}

void AActionHUDBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitPlayerHUD();
	Super::EndPlay(EndPlayReason);
}

bool AActionHUDBase::RequestEquipWeaponByLoadoutSlot(const EHeroWeaponLoadoutSlot InLoadoutSlot) const
{
	if (!BoundHeroCharacter.IsValid())
	{
		return false;
	}

	if (UHeroCombatComponent* HeroCombatComponent = BoundHeroCharacter->GetHeroCombatComponent())
	{
		return HeroCombatComponent->EquipWeaponByLoadoutSlot(InLoadoutSlot);
	}

	return false;
}

void AActionHUDBase::InitMVVM(AActionHeroCharacter* InHeroCharacter)
{
	HeroAttributeViewModel = NewObject<UMVVM_HeroAttribute>(this, UMVVM_HeroAttribute::StaticClass());
	HeroLoadoutViewModel = NewObject<UMVVM_HeroLoadout>(this, UMVVM_HeroLoadout::StaticClass());

	if (!InHeroCharacter)
	{
		return;
	}

	if (UHeroAttributeComponent* InHeroAttributeCmp = InHeroCharacter->GetHeroAttributeComponent())
	{
		HeroAttributeViewModel->InitMVVMWithHeroAttributeSet(InHeroAttributeCmp);
		// 绑定完成后立刻强刷一次当前属性值，确保新建的 ViewModel 能立即拿到初始数据。
		InHeroAttributeCmp->ForceRefreshAttributeChange();
	}

	HeroLoadoutViewModel->InitMVVMWithHeroCharacter(InHeroCharacter);
}

void AActionHUDBase::HandleCombatFeedbackHUDDisplayEvent(FActionCombatFeedbackEvent CombatFeedbackEvent)
{
	if (!CombatFeedbackEvent.bShouldShowDamageNumber)
	{
		return;
	}

	K2_OnCombatFeedbackEvent(CombatFeedbackEvent);
}
