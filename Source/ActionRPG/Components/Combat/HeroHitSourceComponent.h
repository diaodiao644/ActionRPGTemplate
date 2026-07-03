#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCollisionTypes.h"
#include "ActionType/ActionHitTypes.h"
#include "ActionType/ActionHitSourceTypes.h"
#include "Components/ActorComponent.h"
#include "HeroHitSourceComponent.generated.h"

class AActor;
class UActionCollisionRuntimeComponent;
class UPrimitiveComponent;

USTRUCT()
struct FActionHitSourceRegistrationKey
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName SourceId = NAME_None;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	bool operator==(const FActionHitSourceRegistrationKey& Other) const
	{
		return SourceId == Other.SourceId && TargetActor == Other.TargetActor;
	}
};

FORCEINLINE uint32 GetTypeHash(const FActionHitSourceRegistrationKey& InKey)
{
	return HashCombine(GetTypeHash(InKey.SourceId), GetTypeHash(InKey.TargetActor));
}

/**
 * 英雄命中来源组件。
 * 当前这一版承担三件正式职责：
 * 1. 统一管理“本次攻击命中窗口是否开启”；
 * 2. 统一管理“当前窗口启用了哪些命中源”；
 * 3. 统一管理“同一窗口内某个命中源是否已经命中过某个目标”。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroHitSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroHitSourceComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

public:
	/** 开启一个新的命中窗口，并返回这次窗口的运行时句柄。 */
	int32 BeginHitWindow(const FActionHitWindowRuntimeConfig& InWindowRuntimeConfig);

	/** 开启一个新的命中窗口，并返回这次窗口的运行时句柄。 */
	int32 BeginHitWindow(FName InWindowName, bool bInUseWeaponCollisionDetection);
	int32 BeginHitWindow(FName InWindowName, bool bInUseWeaponCollisionDetection, const TArray<FName>& InEnabledHitSourceIds);
	int32 BeginHitWindow(
		FName InWindowName,
		bool bInUseWeaponCollisionDetection,
		const TArray<FName>& InEnabledHitSourceIds,
		const TArray<FName>& InEnabledHitSourceGroupIds);

	/** 按句柄关闭当前命中窗口。若句柄不匹配，则忽略这次关闭请求。 */
	void EndHitWindow(int32 InWindowHandle);

	/** 清空整个命中窗口运行时状态。 */
	void ResetHitWindowRuntime();

	/** 当前是否存在激活中的命中窗口。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit")
	bool IsHitWindowActive() const { return ActiveHitWindowHandle != INDEX_NONE; }

	/** 读取当前激活中的命中窗口名。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit")
	FName GetActiveHitWindowName() const { return ActiveHitWindowName; }

	/** 当前命中窗口是否允许继续驱动武器碰撞检测。 */
	bool ShouldUseWeaponCollisionDetection() const { return bUseWeaponCollisionDetection; }

	/** 读取当前窗口完整配置，供武器命中体等外部链路对齐运行时策略。 */
	const FActionHitWindowRuntimeConfig& GetActiveHitWindowRuntimeConfig() const { return ActiveHitWindowRuntimeConfig; }

	/** 当前是否启用命中窗口屏幕调试。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit|Debug")
	bool IsHitWindowScreenDebugEnabled() const { return bEnableHitWindowScreenDebug; }

	/** 汇总当前命中窗口调试信息。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit|Debug")
	FString DescribeCurrentHitWindowDebug() const;

	/** 把当前命中窗口调试信息打印到屏幕。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Hit|Debug")
	void PrintCurrentHitWindowDebug() const;

	/** 输出一条命中窗口运行时事件调试。 */
	void PrintHitWindowEventDebug(const FString& InMessage, const FColor& InColor, float InDuration = 1.5f) const;

	/** 某个命中源当前是否处于激活状态。 */
	bool IsHitSourceActive(FName InSourceId) const;

	/** 读取当前命中窗口下已经激活的命中源列表。 */
	void GetActiveHitSourceIds(TArray<FName>& OutHitSourceIds) const;

	/** 按来源类型读取当前窗口下已经激活的命中源列表。 */
	void GetActiveHitSourceIdsByType(EActionHitSourceType InSourceType, TArray<FName>& OutHitSourceIds) const;

	/** 读取当前命中窗口下已经激活的命中源定义。 */
	void GetActiveHitSourceDefinitions(TArray<FActionHitSourceDefinition>& OutHitSourceDefinitions) const;

	/** 按来源类型读取当前窗口里第一个激活中的命中源定义。 */
	bool TryGetFirstActiveHitSourceByType(
		EActionHitSourceType InSourceType,
		FActionHitSourceDefinition& OutDefinition) const;

	/** 判断当前命中窗口下，这个目标是否允许登记为新命中。 */
	bool CanRegisterHit(AActor* InTargetActor) const;
	bool CanRegisterHitBySource(FName InSourceId, AActor* InTargetActor) const;

	/** 尝试把目标登记为当前命中窗口中的一次有效命中。 */
	bool TryRegisterHit(AActor* InTargetActor);
	bool TryRegisterHitBySource(FName InSourceId, AActor* InTargetActor);
	bool TryRegisterSingleHitBySource(FName InSourceId, AActor* InTargetActor);
	bool TryBeginRepeatedHitContact(FName InSourceId, FName InSourceComponentName, AActor* InTargetActor);

	/** 注册或更新一个命中源定义。 */
	bool RegisterOrUpdateHitSource(const FActionHitSourceDefinition& InDefinition);

	/** 注销一个命中源定义。 */
	void UnregisterHitSource(FName InSourceId);

	/** 查询一个已注册命中源定义。 */
	bool TryGetRegisteredHitSource(FName InSourceId, FActionHitSourceDefinition& OutDefinition) const;

	/** 把已注册定义写入正式命中来源信息。 */
	bool TryFillHitSourceInfoFromRegistration(FName InSourceId, FActionHitSourceInfo& InOutHitSourceInfo) const;

	/** 按组件名解析一个已注册命中源。 */
	bool TryResolveRegisteredHitSourceIdByComponentName(
		FName InComponentName,
		EActionHitSourceType InRequiredSourceType,
		FName& OutSourceId) const;

	/** 注册或更新一个命中源组定义。 */
	bool RegisterOrUpdateHitSourceGroup(const FActionHitSourceGroupDefinition& InDefinition);

	/** 查询一个已注册命中源组。 */
	bool TryGetRegisteredHitSourceGroup(FName InGroupId, FActionHitSourceGroupDefinition& OutDefinition) const;

private:
	/** 根据本次窗口配置刷新激活命中源集合。 */
	void RefreshActiveHitSources(const TArray<FName>& InEnabledHitSourceIds);
	void RefreshActiveHitSources(
		const TArray<FName>& InEnabledHitSourceIds,
		const TArray<FName>& InEnabledHitSourceGroupIds);
	void RegisterBuiltInHitSources();

	/** 扫描角色身上的身体命中体，并绑定统一重叠回调。 */
	void CacheAndBindOwnedBodyHitComponents();
	UActionCollisionRuntimeComponent* GetOwningCollisionRuntimeComponent() const;

	/** 根据当前命中窗口是否启用对应来源，同步身体命中体碰撞开关。 */
	void UpdateOwnedBodyHitComponentCollisionState();
	void RefreshRepeatedHitTickState();

	/** 身体命中体统一重叠回调。 */
	UFUNCTION()
	void HandleOwnedBodyHitComponentBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOwnedBodyHitComponentEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	/** 按当前攻击段与指定命中源组装正式伤害载荷。 */
	bool TryBuildBodyHitDamagePayload(
		AActor* InTargetActor,
		FName InSourceId,
		FName InSourceComponentName,
		FActionDamagePayload& OutDamagePayload) const;
	bool TryResolveRepeatedBodyHit(
		FActionActiveHitContactState& InOutContactState,
		const TCHAR* InDebugResolveReason);
	void RemoveRepeatedBodyHitContact(FName InSourceId, AActor* InTargetActor);

	/** 身体命中成功后统一奖励特殊切武能量。 */
	void RewardSpecialWeaponSwitchEnergyOnResolvedHit(
		const FActionDamagePayload& InDamagePayload,
		const FActionHitResolveResult& InResolveResult) const;

	/** 是否启用命中窗口屏幕调试。默认关闭，避免常规战斗时刷屏。 */
	UPROPERTY(EditAnywhere, Category = "Action|Combat|Hit|Debug")
	bool bEnableHitWindowScreenDebug = false;

	/** 当前激活中的命中窗口句柄。没有窗口时为 INDEX_NONE。 */
	int32 ActiveHitWindowHandle = INDEX_NONE;

	/** 当前激活中的命中窗口名。 */
	FName ActiveHitWindowName = NAME_None;

	/** 当前窗口是否允许继续使用武器碰撞盒作为命中来源。 */
	bool bUseWeaponCollisionDetection = false;

	/** 当前命中窗口的完整运行时配置。 */
	FActionHitWindowRuntimeConfig ActiveHitWindowRuntimeConfig;

	/** 全局递增的窗口句柄计数器。 */
	int32 NextHitWindowHandle = 1;

	/** 当前命中窗口里已激活的命中源集合。 */
	TSet<FName> ActiveHitSourcesInCurrentWindow;

	/** 当前命中窗口里已经正式登记过的“命中源 + 目标”组合。 */
	TSet<FActionHitSourceRegistrationKey> RegisteredHitsInCurrentWindow;

	/** 当前命中窗口里仍保持接触、并允许按固定间隔重复结算的身体来源状态表。 */
	TMap<FActionHitSourceRegistrationKey, FActionActiveHitContactState> ActiveRepeatedBodyHitContacts;

	/** 当前角色侧已经注册的命中源定义表。 */
	TMap<FName, FActionHitSourceDefinition> RegisteredHitSources;

	/** 当前角色侧已经注册的命中源组定义表。 */
	TMap<FName, FActionHitSourceGroupDefinition> RegisteredHitSourceGroups;

	/** 角色自身身体命中体的运行时组件表。键为命中源 Id。 */
	TMap<FName, TWeakObjectPtr<UPrimitiveComponent>> OwnedBodyHitComponentsBySourceId;
	TMap<FName, FActionCollisionOverrideHandle> ActiveOwnedBodyCollisionOverrideHandles;
};
