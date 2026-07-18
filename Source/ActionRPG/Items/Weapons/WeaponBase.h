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
	/** 武器挂回 Holster 或待机展示位。 */
	Holstered,
	/** 武器处于已装备展示态，但尚未进入正式命中窗口。 */
	EquippedPresentation,
	/** 武器处于攻击命中窗口表现态，允许命中检测链接管碰撞。 */
	AttackDetection
};

USTRUCT()
struct FWeaponHitRegistrationKey
{
	GENERATED_BODY()

public:
	/** 当前命中登记来自哪个武器命中源。 */
	UPROPERTY()
	FName SourceId = NAME_None;

	/** 当前登记命中的目标。 */
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** 同一命中源 + 同一目标视为同一笔窗口内命中登记。 */
	bool operator==(const FWeaponHitRegistrationKey& Other) const
	{
		return SourceId == Other.SourceId && TargetActor == Other.TargetActor;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWeaponHitRegistrationKey& InKey)
{
	return HashCombine(GetTypeHash(InKey.SourceId), GetTypeHash(InKey.TargetActor));
}

/**
 * 基础武器命中检测壳。
 * 它只负责武器局部展示态收口、攻击窗口内碰撞检测、命中登记兜底和基础载荷提交。
 * 角色级正式命中窗口、激活命中源和窗口内去重仍优先回到 `HeroHitSourceComponent`。
 */
UCLASS()
class ACTIONRPG_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void Tick(float DeltaSeconds) override;

public:
	/** 按目标表现阶段切换武器展示态。它只服务显示与碰撞收口，不承担正式装备事务。 */
	void ApplyWeaponPresentationState(EActionWeaponPresentationState InPresentationState);

	/** 读取当前武器表现态。这里回答的是“当前展示/命中窗口状态”，不是装备域正式槽位状态。 */
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

	/** 只为指定命中源列表打开当前攻击命中窗口。它属于单窗口局部运行态，不生成新的正式命中状态源。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Weapon|Hit")
	void BeginAttackDetectionForHitSources(const TArray<FName>& InHitSourceIds);

	/** 带命中窗口运行时配置地打开当前攻击检测。它只开关当前武器局部检测，不创建新的正式命中状态源。 */
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
	/** 统一尝试登记当前窗口内的命中目标。优先交给角色级命中窗口宿主，否则回退到武器本地去重。 */
	bool TryRegisterHitTargetForCurrentAttack(FName InSourceId, AActor* OtherActor);
	/** 单次命中登记路径：同一命中源在一轮挥击内只允许命中同一目标一次。 */
	bool TryRegisterSingleHitTargetForCurrentAttack(FName InSourceId, AActor* OtherActor);
	/** 重复命中登记路径：进入“持续接触可重复结算”局部运行态。 */
	bool TryBeginRepeatedHitContact(FName InSourceId, FName InSourceComponentName, AActor* OtherActor);

	// 构建基础命中载荷。
	// 基础武器类只负责补齐施加者、来源与冲击方向，不负责额外填充伤害数值。
	virtual FActionDamagePayload BuildDamagePayload(AActor* OtherActor) const;
	virtual FActionDamagePayload BuildDamagePayloadForHitSource(
		AActor* OtherActor,
		FName InSourceId,
		const UPrimitiveComponent* InSourceComponent) const;

	/** 武器命中源重叠开始回调。它只处理当前打开窗口内的命中检测与载荷提交。 */
	UFUNCTION()
	void OnWeaponHitComponentBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 武器命中源重叠结束回调。它主要用于重复接触链的局部收尾。 */
	UFUNCTION()
	void OnWeaponHitComponentEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	/** 清理当前一轮挥击中的本地命中去重表。 */
	void ClearHitActorsThisSwing();
	/** 扫描并缓存当前武器 Actor 上可用于命中源解析的真实碰撞组件。 */
	void CacheWeaponHitComponents();
	/** 根据当前是否仍存在重复接触记录，刷新是否继续 Tick 的局部状态。 */
	void RefreshRepeatedHitTickState();
	/** 确保武器命中组件已向 CollisionRuntime 注册正式槽位。 */
	void EnsureWeaponCollisionSlotsRegistered();
	/** 把当前武器切到攻击命中窗口碰撞态。 */
	void ApplyAttackDetectionCollisionState();
	/** 在攻击检测期间临时忽略拥有者碰撞，结束后再恢复。 */
	void UpdateOwnerCollisionIgnoreState(bool bShouldIgnoreOwner);
	/** 根据真实碰撞组件反查它对应的武器命中源 Id。 */
	bool TryResolveWeaponHitSourceIdByComponent(
		const UPrimitiveComponent* InHitComponent,
		FName& OutSourceId) const;
	/** 对仍在接触中的目标执行一次重复命中结算。 */
	bool TryResolveRepeatedWeaponHit(
		FActionActiveHitContactState& InOutContactState,
		const TCHAR* InDebugResolveReason);
	/** 把一条重复命中接触记录从当前窗口局部运行态中移除。 */
	void RemoveRepeatedWeaponHitContact(FName InSourceId, AActor* InTargetActor);
	/** 命中成功后按正式规则奖励攻击方特殊切武能量。它属于命中后的辅助收尾链。 */
	void RewardInstigatorSpecialWeaponSwitchEnergy(
		const FActionDamagePayload& DamagePayload,
		const FActionHitResolveResult& ResolveResult) const;

protected:
	// 武器组件。
	/** 武器主显示网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UStaticMeshComponent* WeaponMesh;

	/** 默认近战碰撞盒。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UBoxComponent* WeaponCollisionBox;

	/** 武器本地碰撞运行态宿主。命中窗口开启时的碰撞 override 都经由这里收口。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
	UActionCollisionRuntimeComponent* ActionCollisionRuntimeComponent = nullptr;

protected:
	// 当前这轮攻击检测是否开启。
	// 它只表达当前武器局部检测态，不代表角色正式攻击状态或装备槽位状态。
	bool bAttackDetectionEnabled = false;

	// 当前这一轮挥击真正启用的武器命中源列表。
	// 它只描述当前命中窗口，不代表武器长期拥有的全部命中源配置。
	TSet<FName> ActiveWeaponHitSourceIds;

	// 当前武器命中窗口的完整运行时配置。
	// 它是单窗口局部运行态，不是资产入口本体。
	FActionHitWindowRuntimeConfig ActiveHitWindowRuntimeConfig;

	// 武器 Actor 上按命中源 Id 缓存到的真实碰撞组件。
	TMap<FName, TWeakObjectPtr<UPrimitiveComponent>> WeaponHitComponentsBySourceId;

	// 一轮挥击内同一命中源只命中同一目标一次，避免连续重叠导致重复结算。
	TSet<FWeaponHitRegistrationKey> HitActorsThisSwing;

	// 当前武器命中窗口里仍处于接触中的重复结算状态表。
	TMap<FWeaponHitRegistrationKey, FActionActiveHitContactState> ActiveRepeatedWeaponHitContacts;

	// 当前武器命中窗口对各命中源持有的碰撞 override 句柄。
	// 窗口收尾时必须统一释放，避免碰撞态泄漏到待机或已装备展示态。
	TMap<FName, FActionCollisionOverrideHandle> ActiveWeaponCollisionOverrideHandles;

	/** 当前武器表现态。 */
	EActionWeaponPresentationState CurrentWeaponPresentationState = EActionWeaponPresentationState::Holstered;
};
