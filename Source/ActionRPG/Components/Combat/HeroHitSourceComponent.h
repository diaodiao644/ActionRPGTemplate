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
 * 当前这一版承担五件正式职责：
 * 1. 统一管理“本次攻击命中窗口是否开启”；
 * 2. 统一管理“当前窗口启用了哪些命中源”；
 * 3. 统一管理“同一窗口内某个命中源是否已经命中过某个目标”；
 * 4. 统一管理“持续接触可重复结算”的身体来源接触状态；
 * 5. 统一维护角色侧命中源与命中源组静态注册表。
 * 它是角色身体命中窗口、激活命中源、窗口内去重和重复接触结算的正式宿主，
 * 不是单次命中结果快照，也不是武器长期配置入口。
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
	/** 开启一个新的命中窗口，并返回这次窗口的运行时句柄。它只建立当前窗口 runtime，不等于攻击正式状态本体。 */
	int32 BeginHitWindow(const FActionHitWindowRuntimeConfig& InWindowRuntimeConfig);

	/** 用最小配置开启一个新的命中窗口，并返回这次窗口的运行时句柄。 */
	int32 BeginHitWindow(FName InWindowName, bool bInUseWeaponCollisionDetection);
	/** 开启命中窗口，并显式指定本地启用的命中源列表。 */
	int32 BeginHitWindow(FName InWindowName, bool bInUseWeaponCollisionDetection, const TArray<FName>& InEnabledHitSourceIds);
	/** 开启命中窗口，并显式指定本地启用的命中源和命中源组列表。 */
	int32 BeginHitWindow(
		FName InWindowName,
		bool bInUseWeaponCollisionDetection,
		const TArray<FName>& InEnabledHitSourceIds,
		const TArray<FName>& InEnabledHitSourceGroupIds);

	/** 按句柄关闭当前命中窗口。若句柄不匹配，则忽略这次关闭请求。 */
	void EndHitWindow(int32 InWindowHandle);

	/** 清空整个命中窗口运行时状态。它只重置窗口、激活集和接触表，不替代外层攻击或受击收尾。 */
	void ResetHitWindowRuntime();

	/** 当前是否存在激活中的命中窗口。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit", meta = (ToolTip = "当前是否存在激活中的命中窗口。它只回答窗口壳是否开启，不等于这次攻击已经真的命中过目标。"))
	bool IsHitWindowActive() const { return ActiveHitWindowHandle != INDEX_NONE; }

	/** 读取当前激活中的命中窗口名。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit", meta = (ToolTip = "读取当前激活中的命中窗口名。它只服务调试、日志和外部链路对齐，不是新的攻击状态源。"))
	FName GetActiveHitWindowName() const { return ActiveHitWindowName; }

	/** 当前命中窗口是否允许继续驱动武器碰撞检测。 */
	bool ShouldUseWeaponCollisionDetection() const { return bUseWeaponCollisionDetection; }

	/** 读取当前窗口完整配置，供武器命中体等外部链路对齐运行时策略。 */
	const FActionHitWindowRuntimeConfig& GetActiveHitWindowRuntimeConfig() const { return ActiveHitWindowRuntimeConfig; }

	/** 当前是否启用命中窗口屏幕调试。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit|Debug", meta = (ToolTip = "当前是否启用命中窗口屏幕调试。它只影响调试输出，不改变命中窗口或去重策略本身。"))
	bool IsHitWindowScreenDebugEnabled() const { return bEnableHitWindowScreenDebug; }

	/** 汇总当前命中窗口调试信息。 */
	UFUNCTION(BlueprintPure, Category = "Action|Combat|Hit|Debug", meta = (ToolTip = "汇总当前命中窗口、激活命中源和去重状态的调试摘要。它只服务观察和排错。"))
	FString DescribeCurrentHitWindowDebug() const;

	/** 把当前命中窗口调试信息打印到屏幕。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat|Hit|Debug", meta = (ToolTip = "把当前命中窗口调试信息打印到屏幕。它只服务排错，不推进任何正式命中状态。"))
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
	/** 判断指定命中源对当前目标是否仍允许登记一次新命中。 */
	bool CanRegisterHitBySource(FName InSourceId, AActor* InTargetActor) const;

	/** 尝试把目标登记为当前命中窗口中的一次有效命中。 */
	bool TryRegisterHit(AActor* InTargetActor);
	/** 尝试把一次指定命中源的命中登记到当前窗口。 */
	bool TryRegisterHitBySource(FName InSourceId, AActor* InTargetActor);
	/** 在单次命中策略下尝试登记一次指定命中源的命中。 */
	bool TryRegisterSingleHitBySource(FName InSourceId, AActor* InTargetActor);
	/** 在持续接触策略下开始追踪一条“来源 + 目标”的重复命中接触状态。 */
	bool TryBeginRepeatedHitContact(FName InSourceId, FName InSourceComponentName, AActor* InTargetActor);

	/** 注册或更新一个命中源定义。 */
	bool RegisterOrUpdateHitSource(const FActionHitSourceDefinition& InDefinition);

	/** 注销一个命中源定义。 */
	void UnregisterHitSource(FName InSourceId);

	/** 查询一个已注册命中源定义。 */
	bool TryGetRegisteredHitSource(FName InSourceId, FActionHitSourceDefinition& OutDefinition) const;

	/** 把已注册定义写入正式命中来源信息。它只把静态定义桥接进 payload，不替代窗口开关本身。 */
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
	/** 根据本次窗口配置刷新激活命中源集合，并展开命中源组。 */
	void RefreshActiveHitSources(
		const TArray<FName>& InEnabledHitSourceIds,
		const TArray<FName>& InEnabledHitSourceGroupIds);
	/** 注册当前项目内建的身体命中源定义。它写入的是角色侧静态注册表，不表示当前窗口已经启用这些来源。 */
	void RegisterBuiltInHitSources();

	/** 扫描角色身上的身体命中体，并绑定统一重叠回调。 */
	void CacheAndBindOwnedBodyHitComponents();
	UActionCollisionRuntimeComponent* GetOwningCollisionRuntimeComponent() const;

	/** 根据当前命中窗口是否启用对应来源，同步身体命中体碰撞开关。 */
	void UpdateOwnedBodyHitComponentCollisionState();
	/** 根据当前窗口策略决定是否需要继续 Tick 持续接触命中。 */
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
	/** 移除一条已经结束接触的持续命中状态。 */
	void RemoveRepeatedBodyHitContact(FName InSourceId, AActor* InTargetActor);

	/** 身体命中成功后统一奖励特殊切武能量。 */
	void RewardSpecialWeaponSwitchEnergyOnResolvedHit(
		const FActionDamagePayload& InDamagePayload,
		const FActionHitResolveResult& InResolveResult) const;

	/** 是否启用命中窗口屏幕调试。默认关闭，避免常规战斗时刷屏。 */
	UPROPERTY(EditAnywhere, Category = "Action|Combat|Hit|Debug", meta = (ToolTip = "是否启用命中窗口屏幕调试。默认关闭，避免常规战斗时持续刷屏。"))
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

	/** 当前角色侧已经注册的命中源定义表。它是静态注册表，不等于当前窗口激活集。 */
	TMap<FName, FActionHitSourceDefinition> RegisteredHitSources;

	/** 当前角色侧已经注册的命中源组定义表。它同样只描述可复用分组入口。 */
	TMap<FName, FActionHitSourceGroupDefinition> RegisteredHitSourceGroups;

	/** 角色自身身体命中体的运行时组件表。键为命中源 Id。 */
	TMap<FName, TWeakObjectPtr<UPrimitiveComponent>> OwnedBodyHitComponentsBySourceId;
	/** 当前仍生效的身体命中体碰撞覆写句柄。它只服务当前窗口局部碰撞桥接，不是长期角色碰撞状态。 */
	TMap<FName, FActionCollisionOverrideHandle> ActiveOwnedBodyCollisionOverrideHandles;
};
