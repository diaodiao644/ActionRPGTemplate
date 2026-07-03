
#include "Components/Attribute/EnemyAttributeComponent.h"

#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/ActionAttributeSetBase.h"

UEnemyAttributeComponent::UEnemyAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UEnemyAttributeComponent::InitializeWithAbilitySystem(UActionAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	if (!Owner || !InASC) return;

	OwnerASC = InASC;

	OwnerAttributeSet = OwnerASC->GetSet<UActionAttributeSetBase>();
	if (!OwnerAttributeSet.IsValid()) return;

	// 绑定属性变化的回调函数，监听属性变化事件
	OwnerAttributeSet->OnAttributeChangeDelegate.AddUObject(this, &UEnemyAttributeComponent::OnRep_EnemyAttributeChange);

}

void UEnemyAttributeComponent::UnInitializeWithAbilitySystem()
{
	if (OwnerAttributeSet.IsValid())
	{
		//解绑属性变化的委托
		OwnerAttributeSet->OnAttributeChangeDelegate.RemoveAll(this);
	}

	OnEnemyAttributeChangeDelegate.Clear();

	OwnerAttributeSet = nullptr;
	OwnerASC = nullptr;
}

void UEnemyAttributeComponent::ForceRefreshAttributeChange()
{
	if (OwnerAttributeSet.IsValid())
	{
		OnEnemyAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetMaxHealthAttribute(), OwnerAttributeSet->GetMaxHealth(), OwnerAttributeSet->GetMaxHealth());
		OnEnemyAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetHealthAttribute(), OwnerAttributeSet->GetHealth(), OwnerAttributeSet->GetHealth());

		OnEnemyAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetMaxPoiseAttribute(), OwnerAttributeSet->GetMaxPoise(), OwnerAttributeSet->GetMaxPoise());
		OnEnemyAttributeChangeDelegate.Broadcast(OwnerAttributeSet->GetPoiseAttribute(), OwnerAttributeSet->GetPoise(), OwnerAttributeSet->GetPoise());
	}
}

void UEnemyAttributeComponent::OnRep_EnemyAttributeChange(const FOnAttributeChangeData& Data)
{
	OnEnemyAttributeChangeDelegate.Broadcast(Data.Attribute, Data.OldValue, Data.NewValue);
}

void UEnemyAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEnemyAttributeComponent::DestroyComponent(bool bPromoteChildren)
{
	UnInitializeWithAbilitySystem();
	Super::DestroyComponent(bPromoteChildren);
}

