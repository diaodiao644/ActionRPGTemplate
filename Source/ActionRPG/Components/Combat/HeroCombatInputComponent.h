#pragma once

#include "CoreMinimal.h"
#include "ActionType/ActionCombatRuntimeTypes.h"
#include "Components/PawnExtensionComponentBase.h"
#include "HeroCombatInputComponent.generated.h"

class AActionHeroCharacter;
class UHeroCombatComponent;

/**
 * 英雄战斗输入运行态组件。
 * 负责收口输入状态、缓冲输入、Held 回放与输入恢复链的正式运行时状态。
 * 当前粒度已经稳定，后续默认冻结，不再继续细拆恢复链或输入状态镜像。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONRPG_API UHeroCombatInputComponent : public UPawnExtensionComponentBase
{
	GENERATED_BODY()

public:
	UHeroCombatInputComponent();

public:
	/** 写入一次 Pressed 输入阶段到正式输入运行态。它只记录和推进输入状态，不直接分发业务行为。 */
	void HandleCombatInputPressed(const FGameplayTag& InputTag);
	/** 写入一次 Held 输入阶段到正式输入运行态，并为后续 Held 回放保留最新状态。 */
	void HandleCombatInputHeld(const FGameplayTag& InputTag);
	/** 写入一次 Released 输入阶段到正式输入运行态，并触发必要的生命周期清理。 */
	void HandleCombatInputReleased(const FGameplayTag& InputTag);

	/** 写入一条新的缓冲输入快照。它是输入恢复链的正式缓存入口，不建议外层另存第二份缓冲状态。 */
	void QueueBufferedInput(
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		const FGameplayTag& ResolvedAttackRequestTag = FGameplayTag(),
		int32 BufferedInputOrder = 0);
	/** 清空当前缓冲输入快照。 */
	void ClearBufferedInput();
	/** 仅当当前缓冲输入标签命中指定标签时，才清掉这条正式缓冲快照。 */
	void ClearBufferedInputIfMatchesTag(const FGameplayTag& InputTag);
	/** 只清理当前缓冲中的切武输入，避免它误串到后续普通恢复链。 */
	void ClearBufferedWeaponSwitchInputIfAny();
	/** 只读查看当前缓冲输入快照，不消耗这份正式缓存。 */
	bool PeekBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput) const;
	/** 取走并消费当前缓冲输入快照。 */
	bool ConsumeBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput);
	/** 直接按当前恢复门禁消费一条缓冲输入。 */
	bool ConsumeBufferedInput();
	/** 查询当前是否存在一条正式有效的缓冲输入。 */
	bool HasValidBufferedInput() const;

	/** 清空延迟输入恢复请求，避免旧恢复任务串到下一条链。 */
	void ClearDeferredCombatInputRecoveryRequests();
	/** 请求在下一帧尝试消费缓冲输入，避开本帧收尾冲突。 */
	void RequestConsumeBufferedInputOnNextTick();
	/** 请求在受击恢复后按正式优先级回放 Held 输入。 */
	void RequestReplayHeldInputsAfterCombatReact();
	/** 处理一次受击恢复后的正式输入恢复。 */
	bool RecoverCombatInputAfterCombatReact();
	/** 处理一次切武表现期后的正式输入恢复。 */
	bool RecoverCombatInputAfterWeaponSwitch();

	/** 生成下一条输入意图顺序号，供缓冲和回放链保持稳定顺序。 */
	int32 GenerateNextCombatIntentOrder();
	/** 查询某个输入当前是否仍处于 Pressed 或 Held 态。 */
	bool IsCombatInputPressedOrHeld(const FGameplayTag& InputTag) const;
	/** 读取指定输入标签当前的按钮状态快照。 */
	EActionInputButtonState GetInputButtonStateByTag(const FGameplayTag& InputTag) const;
	/** 读取指定输入标签对应的完整运行态条目。 */
	bool TryGetInputRuntimeStateEntry(const FGameplayTag& InputTag, FActionInputRuntimeStateEntry& OutInputState) const;
	/** 把指定输入标签标记为 Held，供后续回放和长按语义读取。 */
	void MarkInputAsHeldByTag(const FGameplayTag& InputTag);
	/** 把指定输入标签标记为已正式消费，避免同一轮输入重复落地。 */
	void MarkInputConsumedByTag(const FGameplayTag& InputTag);
	/** 查询指定输入标签当前是否已经被正式消费。 */
	bool IsInputConsumedByTag(const FGameplayTag& InputTag) const;
	/** 给某个输入标签挂上一份锁存的攻击请求标签，供延迟消费或回放时继续沿用。 */
	void SetLatchedAttackRequestTagByInputTag(const FGameplayTag& InputTag, const FGameplayTag& InAttackRequestTag);
	/** 读取某个输入标签当前锁存的攻击请求标签。 */
	FGameplayTag GetLatchedAttackRequestTagByInputTag(const FGameplayTag& InputTag) const;
	/** 清空指定输入标签的整条本地运行态。 */
	void ClearInputStateByTag(const FGameplayTag& InputTag);

	/** 重置输入恢复链专用运行时，不回退其它正式状态源。 */
	void ResetCombatInputRecoveryRuntime();
	/** Hero 启动链重置时回零输入运行态、缓冲和恢复请求。 */
	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 缓冲输入过期后的统一收尾入口，只负责清理正式缓存，不解释业务资格。 */
	UFUNCTION()
	void OnBufferedInputExpired();

	/** 下一帧延迟消费缓冲输入的内部 helper，用来避开本帧收尾冲突。 */
	void HandleDeferredBufferedInputConsume();
	/** 下一帧延迟触发受击恢复 Held 回放的内部 helper。 */
	void HandleDeferredReplayHeldInputsAfterCombatReact();
	/** 受击恢复后按正式优先级回放仍保持按住的输入。 */
	void ReplayHeldInputsAfterCombatReact();
	/** 切武表现期恢复后按正式优先级回放仍保持按住的输入。 */
	void ReplayHeldInputsAfterWeaponSwitch();
	/** 按给定优先级表顺序回放 Held 输入。它只消费输入运行态，不替代总控资格判断。 */
	void ReplayHeldInputsByPriority(const TArray<FGameplayTag>& HeldReplayPriority);
	/** 尝试回放某个优先级标签对应的 Held 输入。 */
	bool TryReplayHeldInputByPriorityTag(const FGameplayTag& HeldInputTag);

protected:
	/** 解析拥有者 Hero。它只是稳定宿主入口，不形成新的角色状态源。 */
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	/** 解析拥有者战斗总控。输入组件只持有正式输入运行态，业务分发仍回到总控。 */
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;

protected:
	/** 输入正式运行态容器。它是按钮阶段、缓冲输入和 Held 回放链的唯一正式状态源。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FHeroCombatInputRuntimeState InputRuntimeState;

private:
	/** 下一帧是否需要尝试消费一条缓冲输入。 */
	bool bDeferredBufferedInputConsumeRequested = false;
	/** 下一帧是否需要在受击恢复后回放 Held 输入。 */
	bool bDeferredReplayHeldInputsAfterCombatReactRequested = false;
	/** 递增输入意图顺序号，用于在同帧多输入时保持稳定先后。 */
	int32 NextCombatIntentOrder = 1;

	/** 拥有者 Hero 的弱引用缓存。 */
	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	/** 拥有者战斗总控的弱引用缓存。 */
	mutable TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;
};
