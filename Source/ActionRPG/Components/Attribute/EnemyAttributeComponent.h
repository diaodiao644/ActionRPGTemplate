// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AttributeSet.h"

#include "EnemyAttributeComponent.generated.h"

class UActionAbilitySystemComponent;
class UActionAttributeSetBase;
struct FOnAttributeChangeData;

// 定义一个委托，供属性变化时广播，参数包括：属性类，旧值、新值
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEnemyAttributeChange,
	FGameplayAttribute, ChangedAttribute,
	float, OldValue,
	float, NewValue
);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACTIONRPG_API UEnemyAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEnemyAttributeComponent();

	UFUNCTION(BlueprintPure, Category = "Enemy|AttributeCmt")
	static UEnemyAttributeComponent* FindEnemyAttributeCmt(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UEnemyAttributeComponent>() : nullptr); }

	// 初始化函数，传入AbilitySystemComponent指针，绑定属性变化的回调函数等
	void InitializeWithAbilitySystem(UActionAbilitySystemComponent* InASC);
	// 反初始化函数，解绑属性变化的回调函数等清理工作
	void UnInitializeWithAbilitySystem();

	// 强制刷新属性变化
	UFUNCTION(BlueprintCallable)
	void ForceRefreshAttributeChange();

	// 属性变化的回调函数，参数为属性变化的数据结构，包含新旧值、属性变化的原因等信息
	void OnRep_EnemyAttributeChange(const FOnAttributeChangeData& Data);

	// 重写组件的生命周期函数，在组件销毁时解绑委托等清理工作
	virtual void DestroyComponent(bool bPromoteChildren = false) override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(BlueprintAssignable)
	FOnEnemyAttributeChange OnEnemyAttributeChangeDelegate;

protected:
	// 持有一个指向属性集的弱引用，避免循环引用导致内存泄漏
	UPROPERTY()
	TWeakObjectPtr<const UActionAttributeSetBase> OwnerAttributeSet = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UActionAbilitySystemComponent> OwnerASC = nullptr;
};
