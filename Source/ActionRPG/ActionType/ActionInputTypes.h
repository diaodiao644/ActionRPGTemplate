#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionInputTypes.generated.h"

class UInputAction;

/**
 * 角色移动状态。
 * 说明：
 * 1. 这里描述的是控制器侧高层移动语义，而不是 CharacterMovement 的底层速度模式；
 * 2. 战斗链主要用它区分普通跑动、冲刺攻击、闪避表现等分支；
 * 3. 后续若继续扩展滑步、锁定绕圈等移动语义，也应优先接在这里统一管理。
 * 4. 它不是正式战斗输入资格，也不表达按钮阶段、缓冲或 Held 回放状态。
 */
UENUM()
enum class EMoveState : uint8
{
	/** 常规移动。它只是控制器侧高层语义，不直接替代 CharacterMovement 的底层模式。 */
	Walk,

	/** 普通奔跑。 */
	Run,

	/** 快速冲刺，通常用于触发冲刺攻击等更激进的战斗分支。它仍然只是高层移动语义。 */
	FastRun
};

/**
 * 输入动作与 GameplayTag 的静态映射关系。
 * 作用：
 * 1. 蓝图只需要配置 InputAction 与 InputTag 的对应关系；
 * 2. 运行时由输入组件统一把原始输入事件翻译成 Tag 驱动的战斗输入；
 * 3. 这样攻击、闪避、防御、切武等逻辑都只依赖统一的输入标签，不直接依赖具体按键资源。
 * 4. 这里描述的是静态资源映射模板，不保存已按下、缓冲、Held 回放或已消费输入状态。
 */
USTRUCT(BlueprintType)
struct FActionInputBinding
{
	GENERATED_BODY()

public:
	/** 输入动作资源。它只是原始设备事件入口，不是正式战斗语义本体。 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ToolTip = "Enhanced Input 的输入动作资源。它只负责原始按键/手柄事件入口，不直接表达战斗语义。若这里留空，这条绑定在运行时不会生效，也不会自动补其它输入状态。"))
	const UInputAction* InputAction = nullptr;

	/**
	 * 该输入动作在战斗框架中的统一语义标签。
	 * 例如可以映射到 Attack、Dodge、Defense、WeaponSwitch 等输入标签。
	 * 它是静态语义入口，不是运行时“已按下 / 已缓冲 / 已消费”的输入状态。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ToolTip = "当前输入动作在战斗框架里的统一语义标签，例如 Attack、Dodge、CombatModeOrDefense、Execution、WeaponSlot.Unarmed~Hybrid 或 SpiritSkill1~4。运行时正式只认这个标签，不直接认具体按键资源；不要把多个完全不同的战斗语义复用成同一个标签。它是静态语义入口，不是已按下或已消费输入状态。"))
	FGameplayTag InputTag;
};
