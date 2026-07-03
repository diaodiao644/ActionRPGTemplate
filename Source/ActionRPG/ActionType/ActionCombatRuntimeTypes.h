// 文件说明：声明战斗输入、能力窗口与防御窗口等共享运行时状态结构。
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionCombatRuntimeTypes.generated.h"

/** 输入标签当前处于哪一种公共按钮状态语义，不直接代表外层战斗资格是否成立。 */
UENUM()
enum class EActionInputButtonState : uint8
{
	None,
	Pressed,
	Held
};

/** 唯一缓冲输入里记录的触发事件类型，只描述这次暂存输入原本来自 Pressed / Held / Released 哪一段。 */
UENUM()
enum class EActionInputEvent : uint8
{
	Pressed,
	Held,
	Released
};

/** 角色当前战斗表现模式的公共语义枚举，正式状态源仍由 HeroCombatComponent 持有。 */
UENUM()
enum class EHeroCombatMode : uint8
{
	Idle,
	Combo,
	Defense
};

/**
 * 唯一缓冲输入壳。
 * 它只记录“哪次输入被暂存、何时过期、解析到了哪个攻击请求”，
 * 不替代 HeroCombatComponent 的正式输入裁决，也不表达输入已经重新正式落地。
 */
USTRUCT(BlueprintType)
struct FActionBufferedInput
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag InputTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EActionInputEvent TriggerEvent = EActionInputEvent::Pressed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag ResolvedAttackRequestTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ExpireWorldTime = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BufferedWorldTime = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 BufferedInputOrder = 0;

public:
	/** 只校验当前缓冲壳自身是否仍可消费，不在这里推进外层输入资格判断。 */
	bool IsValid(float CurrentWorldTime) const
	{
		return InputTag.IsValid() && CurrentWorldTime <= ExpireWorldTime;
	}

	/** 把唯一缓冲输入壳整体清回空态，不负责额外收尾逐键输入镜像。 */
	void Reset()
	{
		InputTag = FGameplayTag();
		TriggerEvent = EActionInputEvent::Pressed;
		ResolvedAttackRequestTag = FGameplayTag();
		ExpireWorldTime = 0.f;
		BufferedWorldTime = 0.f;
		BufferedInputOrder = 0;
	}
};

/** 单个输入标签的组件侧镜像条目，只承载按键状态、消费标记、按下时长和锁存攻击请求。 */
USTRUCT(BlueprintType)
struct FActionInputRuntimeStateEntry
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EActionInputButtonState ButtonState = EActionInputButtonState::None;

	UPROPERTY()
	bool bIsConsumed = false;

	UPROPERTY()
	float PressedTime = 0.f;

	UPROPERTY()
	FGameplayTag LatchedAttackRequestTag;
};

/**
 * 英雄战斗输入的组件侧 runtime 壳。
 * 它统一维护逐键输入镜像和唯一缓冲输入，但正式输入状态源仍收口在 HeroCombatComponent。
 */
USTRUCT(BlueprintType)
struct FHeroCombatInputRuntimeState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FGameplayTag, FActionInputRuntimeStateEntry> InputRuntimeStateEntries;

	UPROPERTY()
	FActionBufferedInput BufferedInput;

	FTimerHandle BufferedInputTimerHandle;

public:
	/** 为某个输入标签写入新的 Pressed 镜像起点，不在这里做攻击资格裁决。 */
	void BeginPressedInputByTag(const FGameplayTag& InTag)
	{
		if (!InTag.IsValid())
		{
			return;
		}

		FActionInputRuntimeStateEntry& InputState = InputRuntimeStateEntries.FindOrAdd(InTag);
		InputState.ButtonState = EActionInputButtonState::Pressed;
		InputState.bIsConsumed = false;
		InputState.PressedTime = 0.f;
		InputState.LatchedAttackRequestTag = FGameplayTag();
	}

	/** 把现有镜像从 Pressed 推进到 Held，只更新本层按钮状态镜像。 */
	void MarkInputAsHeldByTag(const FGameplayTag& InTag)
	{
		if (FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag))
		{
			InputState->ButtonState = EActionInputButtonState::Held;
		}
	}

	/** 删除单个输入标签的镜像条目，通常用于正式松开或强制收尾后的本层清理。 */
	void RemoveInputStateByTag(const FGameplayTag& InTag)
	{
		if (!InTag.IsValid())
		{
			return;
		}

		InputRuntimeStateEntries.Remove(InTag);
	}

	/** 标记这次输入已被正式消费，避免同一轮输入重复落地。 */
	void MarkInputConsumedByTag(const FGameplayTag& InTag)
	{
		if (FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag))
		{
			InputState->bIsConsumed = true;
		}
	}

	/** 只读取消费镜像，不独立回答“当前是否仍允许继续攻击”。 */
	bool IsInputConsumedByTag(const FGameplayTag& InTag) const
	{
		const FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag);
		return InputState ? InputState->bIsConsumed : false;
	}

	/** 记录该输入标签最近一次解析出的攻击请求锁存值，供后续 replay / held 分支读取。 */
	void SetLatchedAttackRequestTagByInputTag(const FGameplayTag& InTag, const FGameplayTag& InAttackRequestTag)
	{
		if (!InTag.IsValid())
		{
			return;
		}

		FActionInputRuntimeStateEntry& InputState = InputRuntimeStateEntries.FindOrAdd(InTag);
		InputState.LatchedAttackRequestTag = InAttackRequestTag;
	}

	/** 只读取当前输入标签锁存的攻击请求镜像，不回查外层攻击配置。 */
	FGameplayTag GetLatchedAttackRequestTagByInputTag(const FGameplayTag& InTag) const
	{
		const FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag);
		return InputState ? InputState->LatchedAttackRequestTag : FGameplayTag();
	}

	/** 累加本次按住时长，服务 Held 判定、表现消费和输入诊断。 */
	float AccumulatePressedTimeByTag(const FGameplayTag& InTag, const float InDeltaTime)
	{
		if (!InTag.IsValid())
		{
			return 0.f;
		}

		FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag);
		if (!InputState)
		{
			return 0.f;
		}

		InputState->PressedTime += FMath::Max(0.f, InDeltaTime);
		return InputState->PressedTime;
	}

	/** 清空单键按住时长镜像，不影响已消费标记和锁存请求。 */
	void ClearPressedTimeByTag(const FGameplayTag& InTag)
	{
		if (!InTag.IsValid())
		{
			return;
		}

		if (FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag))
		{
			InputState->PressedTime = 0.f;
		}
	}

	/** 写入唯一缓冲输入壳，表达“本次输入先暂存，等待窗口或恢复链稍后再正式重放”。 */
	void SetBufferedInput(
		const FGameplayTag& InInputTag,
		const EActionInputEvent InInputEvent,
		const float InBufferedWorldTime,
		const float InExpireWorldTime,
		const FGameplayTag& InResolvedAttackRequestTag = FGameplayTag(),
		const int32 InBufferedInputOrder = 0)
	{
		BufferedInput.InputTag = InInputTag;
		BufferedInput.TriggerEvent = InInputEvent;
		BufferedInput.ResolvedAttackRequestTag = InResolvedAttackRequestTag;
		BufferedInput.BufferedWorldTime = InBufferedWorldTime;
		BufferedInput.ExpireWorldTime = InExpireWorldTime;
		BufferedInput.BufferedInputOrder = InBufferedInputOrder;
	}

	/** 只清空唯一缓冲输入壳，不触碰逐键输入镜像。 */
	void ClearBufferedInput()
	{
		BufferedInput.Reset();
	}

	/** 只判断缓冲壳在当前世界时间是否仍有效。 */
	bool HasValidBufferedInput(const float CurrentWorldTime) const
	{
		return BufferedInput.IsValid(CurrentWorldTime);
	}

	/** 从唯一缓冲输入壳里正式取走一份暂存输入；是否真的重新落地仍由外层重新判定。 */
	bool ConsumeBufferedInput(const float CurrentWorldTime, FActionBufferedInput& OutBufferedInput)
	{
		if (!BufferedInput.IsValid(CurrentWorldTime))
		{
			return false;
		}

		OutBufferedInput = BufferedInput;
		ClearBufferedInput();
		return true;
	}

	/** 只窥视当前缓冲输入内容，供外层做资格判断或调试，不改变缓冲壳本身。 */
	bool PeekBufferedInput(const float CurrentWorldTime, FActionBufferedInput& OutBufferedInput) const
	{
		if (!BufferedInput.IsValid(CurrentWorldTime))
		{
			return false;
		}

		OutBufferedInput = BufferedInput;
		return true;
	}

	/** 只读取单键按钮状态镜像，供 Held / 表现层 / 诊断链消费。 */
	EActionInputButtonState GetInputButtonStateByTag(const FGameplayTag& InTag) const
	{
		if (!InTag.IsValid())
		{
			return EActionInputButtonState::None;
		}

		if (const FActionInputRuntimeStateEntry* InputState = InputRuntimeStateEntries.Find(InTag))
		{
			return InputState->ButtonState;
		}

		return EActionInputButtonState::None;
	}
};

/**
 * Ability 衔接 / 取消窗口的组件侧 runtime 壳。
 * 它只维护窗口镜像与允许输入集合，不承担任何单次 GA 的生命周期宿主职责。
 */
USTRUCT(BlueprintType)
struct FHeroAbilityWindowRuntimeState
{
	GENERATED_BODY()

public:
	bool bAbilityChainWindowActive = false;
	bool bAbilityCancelWindowActive = false;

	UPROPERTY()
	FGameplayTagContainer ChainWindowAllowedInputTags;

	UPROPERTY()
	FGameplayTagContainer CancelWindowAllowedInputTags;

public:
	/** 当前是否仍开放攻击衔接窗口。 */
	bool IsAbilityChainWindowActive() const
	{
		return bAbilityChainWindowActive;
	}

	/** 当前是否仍开放通用取消窗口。 */
	bool IsAbilityCancelWindowActive() const
	{
		return bAbilityCancelWindowActive;
	}

	/** 只回答当前衔接窗口是否接收这个输入标签，不替代外层正式资格判断。 */
	bool AcceptsChainInput(const FGameplayTag& InInputTag) const
	{
		return AcceptsInput(ChainWindowAllowedInputTags, InInputTag);
	}

	/** 只回答当前取消窗口是否接收这个输入标签，不替代外层正式资格判断。 */
	bool AcceptsCancelInput(const FGameplayTag& InInputTag) const
	{
		return AcceptsInput(CancelWindowAllowedInputTags, InInputTag);
	}

	/** 打开攻击衔接窗口并写入这次窗口允许的输入集合。 */
	void OpenAbilityChainWindow(const FGameplayTagContainer& InAllowedInputTags)
	{
		bAbilityChainWindowActive = true;
		ChainWindowAllowedInputTags = InAllowedInputTags;
	}

	/** 关闭攻击衔接窗口并清空它的允许输入集合。 */
	void CloseAbilityChainWindow()
	{
		bAbilityChainWindowActive = false;
		ChainWindowAllowedInputTags.Reset();
	}

	/** 打开通用取消窗口并写入这次窗口允许的输入集合。 */
	void OpenAbilityCancelWindow(const FGameplayTagContainer& InAllowedInputTags)
	{
		bAbilityCancelWindowActive = true;
		CancelWindowAllowedInputTags = InAllowedInputTags;
	}

	/** 关闭通用取消窗口并清空它的允许输入集合。 */
	void CloseAbilityCancelWindow()
	{
		bAbilityCancelWindowActive = false;
		CancelWindowAllowedInputTags.Reset();
	}

	/** 把两类 Ability 窗口镜像整体回收到空态。 */
	void Reset()
	{
		CloseAbilityChainWindow();
		CloseAbilityCancelWindow();
	}

private:
	/** 窗口层的纯读取 helper：空集合表示该窗口不额外限制输入标签。 */
	static bool AcceptsInput(const FGameplayTagContainer& InAllowedInputTags, const FGameplayTag& InInputTag)
	{
		if (!InInputTag.IsValid())
		{
			return false;
		}

		return InAllowedInputTags.IsEmpty() || InAllowedInputTags.HasTagExact(InInputTag);
	}
};

/**
 * 英雄战斗窗口的组件侧 runtime 壳。
 * 它只承载防御、精准格挡、闪避、完美闪避和闪反资格等窗口镜像，
 * 不替代 HeroCombatComponent 的正式战斗状态源，也不单独形成独立状态机。
 */
USTRUCT(BlueprintType)
struct FHeroCombatWindowRuntimeState
{
	GENERATED_BODY()

public:
	bool bDefenseActive = false;
	bool bParryWindowActive = false;
	bool bDodgeActive = false;
	bool bPerfectDodgeWindowActive = false;
	bool bDodgeCounterAvailable = false;

public:
	/** 当前是否处于正式防御窗口。 */
	bool IsDefenseActive() const
	{
		return bDefenseActive;
	}

	/** 当前是否开放精准格挡窗口。 */
	bool IsParryWindowActive() const
	{
		return bParryWindowActive;
	}

	/** 当前是否仍处于闪避表现 / 资格窗口。 */
	bool IsDodgeActive() const
	{
		return bDodgeActive;
	}

	/** 当前是否开放完美闪避判定窗口。 */
	bool IsPerfectDodgeWindowActive() const
	{
		return bPerfectDodgeWindowActive;
	}

	/** 当前是否保留闪反资格。 */
	bool IsDodgeCounterAvailable() const
	{
		return bDodgeCounterAvailable;
	}

	/** 聚合读取当前是否处于防御或闪避硬锁相关窗口，供外层做统一门禁判断。 */
	bool HasCombatLockState() const
	{
		return bDefenseActive || bDodgeActive;
	}

	/** 只写防御窗口镜像，不在这里提交防御行为。 */
	void SetDefenseActive(const bool bInActive)
	{
		bDefenseActive = bInActive;
	}

	/** 只写精准格挡窗口镜像。 */
	void SetParryWindowActive(const bool bInActive)
	{
		bParryWindowActive = bInActive;
	}

	/** 只写闪避窗口镜像。 */
	void SetDodgeActive(const bool bInActive)
	{
		bDodgeActive = bInActive;
	}

	/** 只写完美闪避窗口镜像。 */
	void SetPerfectDodgeWindowActive(const bool bInActive)
	{
		bPerfectDodgeWindowActive = bInActive;
	}

	/** 只写闪反资格镜像。 */
	void SetDodgeCounterAvailable(const bool bInAvailable)
	{
		bDodgeCounterAvailable = bInAvailable;
	}
};

/**
 * Spirit 多段技能的组件侧持久运行时状态。
 * 它描述的是“某个 Spirit 输入标签当前是否还保留续段资格”，
 * 而不是某次 GA 激活内部的瞬时播放状态。
 * 因此 PendingClipIndex、待命态和超时器都跟着 HeroCombatComponent 走，
 * 让同一个 Spirit 输入在 Ability 结束后仍能保留正式续段语义。
 */
USTRUCT(BlueprintType)
struct FHeroSpiritSkillComboRuntimeState
{
	GENERATED_BODY()

public:
	/** 当前等待下一次激活时要读取的技能段索引。未开启持久索引时始终保持为 0。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PendingClipIndex = 0;

	/** 当前技能配置里的总段数快照，用于约束待命索引和判定这条链是否仍有后续段可接。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 SkillClipCount = 0;

	/** 当前是否正处于“等待玩家续段或等待超时断链”的待命态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bWaitingForNextClip = false;

	/** 当前这条 Spirit 连段是否已经正式提交过一次成本，避免中间段续按时重复扣除。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bCostCommittedForCurrentChain = false;

	/** 当前这条 Spirit 连段使用的等待超时秒数。它只服务待命断链，不承担技能表现期时长语义。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ComboChainTimeoutSeconds = 0.f;

	/** 当前等待超时计时器句柄。组件在超时回调里负责提交流却并回收这条持久状态。 */
	FTimerHandle ComboChainTimeoutTimerHandle;

public:
	/** 当前是否仍保留“可以在后续重新按下同一 Spirit 输入继续续段”的待命资格。 */
	bool HasWaitingState() const
	{
		return bWaitingForNextClip;
	}

	/** 把这条 Spirit 输入的持久链状态整体回到全新起手态。 */
	void Reset()
	{
		PendingClipIndex = 0;
		SkillClipCount = 0;
		bWaitingForNextClip = false;
		bCostCommittedForCurrentChain = false;
		ComboChainTimeoutSeconds = 0.f;
		ComboChainTimeoutTimerHandle = FTimerHandle();
	}
};
