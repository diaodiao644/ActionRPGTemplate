// 文件说明：声明 WeaponBase 相关类型与接口。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActionType/ActionCollisionTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "WeaponBase.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UActionCollisionRuntimeComponent;
class AActionCharacterBase;

UENUM()
enum class EActionWeaponPresentationState : uint8
{
	Holstered,
	EquippedPresentation,
	AttackDetection
};

USTRUCT()
struct FWeaponHitRegistrationKey
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName SourceId = NAME_None;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	bool operator==(const FWeaponHitRegistrationKey& Other) const
	{
		return SourceId == Other.SourceId && TargetActor == Other.TargetActor;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWeaponHitRegistrationKey& InKey)
{
	return HashCombine(GetTypeHash(InKey.SourceId), GetTypeHash(InKey.TargetActor));
}

UCLASS()
class ACTIONRPG_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void Tick(float DeltaSeconds) override;

public:
	void ApplyWeaponPresentationState(EActionWeaponPresentationState InPresentationState);

	EActionWeaponPresentationState GetCurrentWeaponPresentationState() const
	{
		return CurrentWeaponPresentationState;
	}

	// 装备态正式只保留表现，不保留 Actor / Mesh 世界碰撞。
	// 真正需要开启碰撞的时机，只允许来自攻击命中窗口链。
	void ApplyEquippedPresentationCollisionState();

	// 攻击检测控制接口：通常由 AnimNotifyState 在攻击有效帧内调用。
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	void BeginAttackDetection();

	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	void BeginAttackDetectionForHitSources(const TArray<FName>& InHitSourceIds);

	void BeginAttackDetectionForHitSources(
		const FActionHitWindowRuntimeConfig& InHitWindowRuntimeConfig,
		const TArray<FName>& InHitSourceIds);

	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	void EndAttackDetection();

	UFUNCTION(BlueprintPure, Category = "Action|Weapon|Hit")
	bool IsAttackDetectionEnabled() const { return bAttackDetectionEnabled; }

protected:
	// 生命周期接口。
	virtual void BeginPlay() override;

	// 命中窗口去重辅助：
	// 优先使用角色级命中窗口组件做统一去重；若当前没有接入该组件，则回退到本地挥击去重。
	bool TryRegisterHitTargetForCurrentAttack(FName InSourceId, AActor* OtherActor);
	bool TryRegisterSingleHitTargetForCurrentAttack(FName InSourceId, AActor* OtherActor);
	bool TryBeginRepeatedHitContact(FName InSourceId, FName InSourceComponentName, AActor* OtherActor);

	// 构建基础命中载荷。
	// 基础武器类只负责补齐施加者、来源与冲击方向，不负责额外填充伤害数值。
	virtual FActionDamagePayload BuildDamagePayload(AActor* OtherActor) const;
	virtual FActionDamagePayload BuildDamagePayloadForHitSource(
		AActor* OtherActor,
		FName InSourceId,
		const UPrimitiveComponent* InSourceComponent) const;

	UFUNCTION()
	void OnWeaponHitComponentBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnWeaponHitComponentEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ClearHitActorsThisSwing();
	void CacheWeaponHitComponents();
	void RefreshRepeatedHitTickState();
	void EnsureWeaponCollisionSlotsRegistered();
	void ApplyAttackDetectionCollisionState();
	void UpdateOwnerCollisionIgnoreState(bool bShouldIgnoreOwner);
	bool TryResolveWeaponHitSourceIdByComponent(
		const UPrimitiveComponent* InHitComponent,
		FName& OutSourceId) const;
	bool TryResolveRepeatedWeaponHit(
		FActionActiveHitContactState& InOutContactState,
		const TCHAR* InDebugResolveReason);
	void RemoveRepeatedWeaponHitContact(FName InSourceId, AActor* InTargetActor);
	void RewardInstigatorSpecialWeaponSwitchEnergy(
		const FActionDamagePayload& DamagePayload,
		const FActionHitResolveResult& ResolveResult) const;

protected:
	// 武器组件。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UStaticMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UBoxComponent* WeaponCollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UActionCollisionRuntimeComponent* ActionCollisionRuntimeComponent = nullptr;

protected:
	// 当前这轮攻击检测是否开启。
	bool bAttackDetectionEnabled = false;

	// 当前这一轮挥击真正启用的武器命中源列表。
	TSet<FName> ActiveWeaponHitSourceIds;

	// 当前武器命中窗口的完整运行时配置。
	FActionHitWindowRuntimeConfig ActiveHitWindowRuntimeConfig;

	// 武器 Actor 上按命中源 Id 缓存到的真实碰撞组件。
	TMap<FName, TWeakObjectPtr<UPrimitiveComponent>> WeaponHitComponentsBySourceId;

	// 一轮挥击内同一命中源只命中同一目标一次，避免连续重叠导致重复结算。
	TSet<FWeaponHitRegistrationKey> HitActorsThisSwing;

	// 当前武器命中窗口里仍处于接触中的重复结算状态表。
	TMap<FWeaponHitRegistrationKey, FActionActiveHitContactState> ActiveRepeatedWeaponHitContacts;

	// 当前武器命中窗口对各命中源持有的碰撞 override 句柄。
	TMap<FName, FActionCollisionOverrideHandle> ActiveWeaponCollisionOverrideHandles;

	EActionWeaponPresentationState CurrentWeaponPresentationState = EActionWeaponPresentationState::Holstered;
};
