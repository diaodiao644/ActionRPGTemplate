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
	void HandleCombatInputPressed(const FGameplayTag& InputTag);
	void HandleCombatInputHeld(const FGameplayTag& InputTag);
	void HandleCombatInputReleased(const FGameplayTag& InputTag);

	void QueueBufferedInput(
		const FGameplayTag& InputTag,
		EActionInputEvent InputEvent,
		const FGameplayTag& ResolvedAttackRequestTag = FGameplayTag(),
		int32 BufferedInputOrder = 0);
	void ClearBufferedInput();
	void ClearBufferedWeaponSwitchInputIfAny();
	bool PeekBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput) const;
	bool ConsumeBufferedInputSnapshot(FActionBufferedInput& OutBufferedInput);
	bool ConsumeBufferedInput();
	bool HasValidBufferedInput() const;

	void ClearDeferredCombatInputRecoveryRequests();
	void RequestConsumeBufferedInputOnNextTick();
	void RequestReplayHeldInputsAfterCombatReact();
	bool RecoverCombatInputAfterCombatReact();
	bool RecoverCombatInputAfterWeaponSwitch();

	int32 GenerateNextCombatIntentOrder();
	bool IsCombatInputPressedOrHeld(const FGameplayTag& InputTag) const;
	EActionInputButtonState GetInputButtonStateByTag(const FGameplayTag& InputTag) const;
	bool TryGetInputRuntimeStateEntry(const FGameplayTag& InputTag, FActionInputRuntimeStateEntry& OutInputState) const;
	void MarkInputAsHeldByTag(const FGameplayTag& InputTag);
	void MarkInputConsumedByTag(const FGameplayTag& InputTag);
	bool IsInputConsumedByTag(const FGameplayTag& InputTag) const;
	void SetLatchedAttackRequestTagByInputTag(const FGameplayTag& InputTag, const FGameplayTag& InAttackRequestTag);
	FGameplayTag GetLatchedAttackRequestTagByInputTag(const FGameplayTag& InputTag) const;
	void ClearInputStateByTag(const FGameplayTag& InputTag);

	void ResetCombatInputRecoveryRuntime();
	void ResetRuntimeStateForHeroStartup();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnBufferedInputExpired();

	void HandleDeferredBufferedInputConsume();
	void HandleDeferredReplayHeldInputsAfterCombatReact();
	void ReplayHeldInputsAfterCombatReact();
	void ReplayHeldInputsAfterWeaponSwitch();
	void ReplayHeldInputsByPriority(const TArray<FGameplayTag>& HeldReplayPriority);
	bool TryReplayHeldInputByPriorityTag(const FGameplayTag& HeldInputTag);

protected:
	AActionHeroCharacter* GetOwningHeroCharacter() const;
	UHeroCombatComponent* GetOwningHeroCombatComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CombatConfig|Runtime")
	FHeroCombatInputRuntimeState InputRuntimeState;

private:
	bool bDeferredBufferedInputConsumeRequested = false;
	bool bDeferredReplayHeldInputsAfterCombatReactRequested = false;
	int32 NextCombatIntentOrder = 1;

	mutable TWeakObjectPtr<AActionHeroCharacter> CachedHeroCharacter;
	mutable TWeakObjectPtr<UHeroCombatComponent> CachedHeroCombatComponent;
};
