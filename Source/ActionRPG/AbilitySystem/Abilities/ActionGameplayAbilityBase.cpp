// 文件说明：实现通用 Gameplay Ability 基类的共享逻辑。

#include "AbilitySystem/Abilities/ActionGameplayAbilityBase.h"

#include "ActionGameplayTags.h"
#include "AbilitySystem/ActionAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Effects/ActionGE_GenericCooldown.h"
#include "Components/Combat/ActionCombatReactComponent.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionGameplayAbilityBase, Log, All);

namespace
{
	FString AbilityBaseBoolToDebugText(const bool bValue)
	{
		return bValue ? TEXT("\u662f") : TEXT("\u5426");
	}
}

UActionGameplayAbilityBase::UActionGameplayAbilityBase()
	: Super()
{
	// 基类默认按“本地预测 + Actor 实例化”配置，覆盖绝大多数战斗 Ability 的通用需求。
	// 具体 GA 只需要在自己的构造函数里补关系、输入和战斗规则，不必每次重写这组 GAS 基线。
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
	bReplicateInputDirectly = false;
	bServerRespectsRemoteAbilityCancellation = false;

	// 冷却也统一走一份通用 GE，具体时长和标签由当前 Ability 的静态配置决定。
	// 这样组件侧补冷却和普通 Ability 冷却提交都能复用同一套配置口径。
	AbilityCooldownGameplayEffectClass = UActionGE_GenericCooldown::StaticClass();
}

bool UActionGameplayAbilityBase::ApplyConfiguredCooldownExternally(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!Handle.IsValid())
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：Handle 无效。Ability=%s"),
			*GetAbilityDebugName());
		return false;
	}

	if (!ActorInfo)
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：ActorInfo 无效。Ability=%s"),
			*GetAbilityDebugName());
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：ASC 无效。Ability=%s"),
			*GetAbilityDebugName());
		return false;
	}

	if (!HasAbilityCooldownConfig(Handle, ActorInfo))
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：未配置有效冷却。Ability=%s"),
			*GetAbilityDebugName());
		return false;
	}

	FGameplayTagContainer ResolvedCooldownTags;
	float ResolvedCooldownDuration = 0.f;
	ResolveAbilityCooldownConfig(Handle, ActorInfo, ResolvedCooldownTags, ResolvedCooldownDuration);
	if (AbilityCooldownGameplayEffectClass == nullptr
		|| ResolvedCooldownTags.Num() == 0
		|| ResolvedCooldownDuration <= 0.f)
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：冷却配置无效。Ability=%s GE=%s Tags=%d Duration=%.2f"),
			*GetAbilityDebugName(),
			*GetNameSafe(AbilityCooldownGameplayEffectClass),
			ResolvedCooldownTags.Num(),
			ResolvedCooldownDuration);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	if (ActorInfo->AvatarActor.IsValid())
	{
		EffectContext.AddSourceObject(ActorInfo->AvatarActor.Get());
	}

	FGameplayEffectSpecHandle CooldownSpecHandle =
		AbilitySystemComponent->MakeOutgoingSpec(
			AbilityCooldownGameplayEffectClass,
			GetAbilityLevel(Handle, ActorInfo),
			EffectContext);
	if (!CooldownSpecHandle.IsValid() || !CooldownSpecHandle.Data.IsValid())
	{
		UE_LOG(
			LogActionGameplayAbilityBase,
			Warning,
			TEXT("[Cooldown] 外部补冷却失败：GE 规格创建失败。Ability=%s GE=%s"),
			*GetAbilityDebugName(),
			*GetNameSafe(AbilityCooldownGameplayEffectClass));
		return false;
	}

	CooldownSpecHandle.Data->DynamicGrantedTags.AppendTags(ResolvedCooldownTags);
	CooldownSpecHandle.Data->SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Cooldown_Duration,
		FMath::Max(0.f, ResolvedCooldownDuration));

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CooldownSpecHandle.Data.Get());
	return true;
}

UActionAbilitySystemComponent* UActionGameplayAbilityBase::GetActionAbilitySystemComponentFromActorInfo() const
{
	if (CurrentActorInfo)
	{
		return Cast<UActionAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get());
	}

	return nullptr;
}

bool UActionGameplayAbilityBase::UsesAbilityRelationshipSystem() const
{
	return bUseAbilityRelationshipSystem;
}

int32 UActionGameplayAbilityBase::GetAbilityPriority() const
{
	return AbilityPriority;
}

bool UActionGameplayAbilityBase::CanBeInterruptedByHigherPriority() const
{
	return bCanBeInterruptedByHigherPriority;
}

bool UActionGameplayAbilityBase::CanBeInterruptedBySamePriority() const
{
	return bCanBeInterruptedBySamePriority;
}

bool UActionGameplayAbilityBase::CanInterruptLowerPriorityAbilities() const
{
	return bCanInterruptLowerPriorityAbilities;
}

bool UActionGameplayAbilityBase::CanInterruptSamePriorityAbilities() const
{
	return bCanInterruptSamePriorityAbilities;
}

bool UActionGameplayAbilityBase::AllowsActivationDuringRecoveryCancelWindow() const
{
	return CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow;
}

FGameplayTag UActionGameplayAbilityBase::GetPrimaryInputTagForCombatReact() const
{
	return FGameplayTag();
}

bool UActionGameplayAbilityBase::IsActivationAllowedByCombatReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	// 这里处理的是“当前受击运行态命中后，这条 Ability 还能不能激活”的动态权限。
	// 静态配置决定有哪些规则参与判断，真正的放行与阻断则依赖当前 CombatReact 组件状态。
	// 这层只补受击相关门槛，不替代 GAS 自带的 CanActivate 判定，也不替代 ASC 关系系统裁决。
	if (!CombatReactAbilityRule.bUseCombatReactRule || !CombatReactComponent)
	{
		return true;
	}

	if (!CombatReactComponent->IsCombatReactActive())
	{
		return true;
	}

	if (CombatReactAbilityRule.bBlockDuringPrimaryReactPhase
		&& CombatReactComponent->GetCurrentReactPhase() == EActionCombatReactPhase::Reacting)
	{
		return false;
	}

	if (CombatReactAbilityRule.bBlockDuringAirborneUncontrolledPhase
		&& CombatReactComponent->IsAirborneUncontrolledActive())
	{
		return false;
	}

	if (CombatReactAbilityRule.bBlockDuringRecoveryPhase
		&& CombatReactComponent->IsRecoveryPhaseActive())
	{
		// 恢复取消窗口的放行必须回到正式输入标签。
		// 没有输入标签时，就无法和 HeroCombatComponent 的白名单窗口对齐，因此不能在这里靠 Ability 身份直接放行。
		if (AllowsActivationDuringRecoveryCancelWindow())
		{
			const FGameplayTag AbilityInputTag = GetPrimaryInputTagForCombatReact();
			if (AbilityInputTag.IsValid()
				&& CombatReactComponent->CanActivateAbilityDuringRecoveryCancelWindow(AbilityInputTag))
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

bool UActionGameplayAbilityBase::DoesCombatReactRuleMatchCurrentState(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (!CombatReactAbilityRule.bUseCombatReactRule || !CombatReactComponent)
	{
		return false;
	}

	if (!CombatReactComponent->IsCombatReactActive())
	{
		return false;
	}

	return IsActivationAllowedByCombatReact(CombatReactComponent);
}

bool UActionGameplayAbilityBase::DoesCombatReactInterruptOverrideMatchCurrentState(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	(void)CombatReactComponent;
	return false;
}

bool UActionGameplayAbilityBase::IsProtectedByCombatReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	(void)CombatReactComponent;
	return false;
}

bool UActionGameplayAbilityBase::CanInterruptLowerPriorityAbilitiesInCurrentReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	// 平时读取的是静态打断权限；
	// 只有当受击规则明确要求“命中某种恢复态后覆盖打断权限”时，才临时切到动态覆写结果。
	if (DoesCombatReactInterruptOverrideMatchCurrentState(CombatReactComponent))
	{
		return CanInterruptLowerPriorityAbilities();
	}

	return CanInterruptLowerPriorityAbilities();
}

bool UActionGameplayAbilityBase::CanInterruptSamePriorityAbilitiesInCurrentReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (DoesCombatReactInterruptOverrideMatchCurrentState(CombatReactComponent))
	{
		return CanInterruptSamePriorityAbilities();
	}

	return CanInterruptSamePriorityAbilities();
}

bool UActionGameplayAbilityBase::CanBeInterruptedByHigherPriorityInCurrentReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (DoesCombatReactInterruptOverrideMatchCurrentState(CombatReactComponent))
	{
		return CanBeInterruptedByHigherPriority();
	}

	return CanBeInterruptedByHigherPriority();
}

bool UActionGameplayAbilityBase::CanBeInterruptedBySamePriorityInCurrentReact(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (DoesCombatReactInterruptOverrideMatchCurrentState(CombatReactComponent))
	{
		return CanBeInterruptedBySamePriority();
	}

	return CanBeInterruptedBySamePriority();
}

const FGameplayTagContainer& UActionGameplayAbilityBase::GetAbilityIdentityTags() const
{
	return AbilityTags;
}

const FGameplayTagContainer& UActionGameplayAbilityBase::GetCancelAbilitiesWithTagsForRelationship() const
{
	return CancelAbilitiesWithTag;
}

const FGameplayTagContainer& UActionGameplayAbilityBase::GetActivationBlockedTagsForRelationship() const
{
	return ActivationBlockedTags;
}

bool UActionGameplayAbilityBase::CanCancelAbilitiesWithTags(const FGameplayTagContainer& InAbilityTags) const
{
	return CancelAbilitiesWithTag.Num() > 0 && CancelAbilitiesWithTag.HasAny(InAbilityTags);
}

bool UActionGameplayAbilityBase::IsBlockedByActivationOwnedTags(const FGameplayTagContainer& InActivationOwnedTags) const
{
	return ActivationBlockedTags.Num() > 0 && ActivationBlockedTags.HasAny(InActivationOwnedTags);
}

const FGameplayTagContainer& UActionGameplayAbilityBase::GetActivationOwnedTagsForRelationship() const
{
	return ActivationOwnedTags;
}

bool UActionGameplayAbilityBase::ValidateRelationshipActivationPreconditions(FString& OutFailureReason)
{
	// 这是一层给子类追加“运行时依赖是否齐全”的补充入口。
	// 它不替代 GAS 的激活检查，也不替代 ASC 的关系系统；
	// 只负责把那些必须在关系取消前先知道的子类级前置条件，统一收成可读失败原因。
	OutFailureReason.Reset();
	return true;
}

FString UActionGameplayAbilityBase::GetAbilityDebugName() const
{
	if (AbilityTags.Num() > 0)
	{
		return AbilityTags.First().ToString();
	}

	return GetNameSafe(GetClass());
}

FString UActionGameplayAbilityBase::DescribeCombatReactRule() const
{
	if (!CombatReactAbilityRule.bUseCombatReactRule)
	{
		return TEXT("\u53d7\u51fb\u89c4\u5219\uff1a\u672a\u542f\u7528\u3002");
	}

	return FString::Printf(
		TEXT("\u53d7\u51fb\u89c4\u5219\uff1a\u542f\u7528=%s\uff0c\u4e3b\u53d7\u51fb\u963b\u6b62=%s\uff0c\u7a7a\u4e2d\u5931\u63a7\u963b\u6b62=%s\uff0c\u6062\u590d\u963b\u6b62=%s\uff0c\u6062\u590d\u53d6\u6d88\u7a97\u53e3\u653e\u884c=%s\u3002"),
		*AbilityBaseBoolToDebugText(CombatReactAbilityRule.bUseCombatReactRule),
		*AbilityBaseBoolToDebugText(CombatReactAbilityRule.bBlockDuringPrimaryReactPhase),
		*AbilityBaseBoolToDebugText(CombatReactAbilityRule.bBlockDuringAirborneUncontrolledPhase),
		*AbilityBaseBoolToDebugText(CombatReactAbilityRule.bBlockDuringRecoveryPhase),
		*AbilityBaseBoolToDebugText(CombatReactAbilityRule.bAllowActivationDuringRecoveryCancelWindow));
}

FString UActionGameplayAbilityBase::DescribeCombatReactActivationDecision(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	if (!CombatReactAbilityRule.bUseCombatReactRule)
	{
		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u5141\u8bb8\uff0c\u539f\u56e0=\u672a\u542f\u7528\u53d7\u51fb\u89c4\u5219\u3002");
	}

	if (!CombatReactComponent)
	{
		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u5141\u8bb8\uff0c\u539f\u56e0=\u672a\u627e\u5230\u53d7\u51fb\u7ec4\u4ef6\u3002");
	}

	if (!CombatReactComponent->IsCombatReactActive())
	{
		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u5141\u8bb8\uff0c\u539f\u56e0=\u5f53\u524d\u4e0d\u5728\u53d7\u51fb\u72b6\u6001\u3002");
	}

	if (CombatReactAbilityRule.bBlockDuringPrimaryReactPhase
		&& CombatReactComponent->GetCurrentReactPhase() == EActionCombatReactPhase::Reacting)
	{
		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u7981\u6b62\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u4e3b\u53d7\u51fb\u9636\u6bb5\u3002");
	}

	if (CombatReactAbilityRule.bBlockDuringAirborneUncontrolledPhase
		&& CombatReactComponent->IsAirborneUncontrolledActive())
	{
		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u7981\u6b62\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u7a7a\u4e2d\u5931\u63a7\u9636\u6bb5\u3002");
	}

	if (CombatReactAbilityRule.bBlockDuringRecoveryPhase
		&& CombatReactComponent->IsRecoveryPhaseActive())
	{
		if (AllowsActivationDuringRecoveryCancelWindow())
		{
			const FGameplayTag AbilityInputTag = GetPrimaryInputTagForCombatReact();
			if (!AbilityInputTag.IsValid())
			{
				return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u7981\u6b62\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u6062\u590d\u9636\u6bb5\uff0c\u4f46\u8fd9\u6761 Ability \u6ca1\u6709\u6b63\u5f0f\u8f93\u5165\u6807\u7b7e\uff0c\u65e0\u6cd5\u53c2\u4e0e\u6062\u590d\u5c3e\u6bb5\u53d6\u6d88\u7a97\u53e3\u653e\u884c\u3002");
			}

			if (CombatReactComponent->CanActivateAbilityDuringRecoveryCancelWindow(AbilityInputTag))
			{
				return FString::Printf(
					TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u5141\u8bb8\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u6062\u590d\u9636\u6bb5\uff0c\u4e14\u6062\u590d\u5c3e\u6bb5\u53d6\u6d88\u7a97\u53e3\u5df2\u5bf9\u767d\u540d\u5355\u8f93\u5165 %s \u653e\u884c\u3002"),
					*AbilityInputTag.ToString());
			}

			return FString::Printf(
				TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u7981\u6b62\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u6062\u590d\u9636\u6bb5\uff0c\u4f46\u6062\u590d\u5c3e\u6bb5\u53d6\u6d88\u7a97\u53e3\u5c1a\u672a\u5bf9\u767d\u540d\u5355\u8f93\u5165 %s \u653e\u884c\u3002"),
				*AbilityInputTag.ToString());
		}

		return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u7981\u6b62\uff0c\u539f\u56e0=\u5f53\u524d\u5904\u4e8e\u6062\u590d\u9636\u6bb5\u3002");
	}

	return TEXT("\u6fc0\u6d3b\u5224\u65ad\uff1a\u5141\u8bb8\uff0c\u539f\u56e0=\u5f53\u524d\u53d7\u51fb\u9636\u6bb5\u672a\u547d\u4e2d\u963b\u6b62\u6761\u4ef6\u3002");
}

FString UActionGameplayAbilityBase::DescribeCombatReactInterruptDecision(
	const UActionCombatReactComponent* CombatReactComponent) const
{
	const bool bRuleMatched = DoesCombatReactRuleMatchCurrentState(CombatReactComponent);
	const bool bOverridePermissions =
		DoesCombatReactInterruptOverrideMatchCurrentState(CombatReactComponent);

	return FString::Printf(
		TEXT("\u6253\u65ad\u5224\u65ad\uff1a\u89c4\u5219\u547d\u4e2d=%s\uff0c\u8986\u76d6\u6743\u9650\u547d\u4e2d=%s\uff0c\u53ef\u6253\u65ad\u4f4e\u4f18\u5148\u7ea7=%s\uff0c\u53ef\u6253\u65ad\u540c\u4f18\u5148\u7ea7=%s\uff0c\u53ef\u88ab\u66f4\u9ad8\u4f18\u5148\u7ea7\u6253\u65ad=%s\uff0c\u53ef\u88ab\u540c\u4f18\u5148\u7ea7\u6253\u65ad=%s\u3002"),
		*AbilityBaseBoolToDebugText(bRuleMatched),
		*AbilityBaseBoolToDebugText(bOverridePermissions),
		*AbilityBaseBoolToDebugText(CanInterruptLowerPriorityAbilitiesInCurrentReact(CombatReactComponent)),
		*AbilityBaseBoolToDebugText(CanInterruptSamePriorityAbilitiesInCurrentReact(CombatReactComponent)),
		*AbilityBaseBoolToDebugText(CanBeInterruptedByHigherPriorityInCurrentReact(CombatReactComponent)),
		*AbilityBaseBoolToDebugText(CanBeInterruptedBySamePriorityInCurrentReact(CombatReactComponent)));
}

void UActionGameplayAbilityBase::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (ActivationPolicy == EActionAbilityActivationPolicy::OnGiven && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		// OnGiven 适合“授予后应立刻起手”的能力，例如被动壳或初始化流程能力。
		// 这里直接让 ASC 尝试激活，但最终是否成功仍要继续经过 GAS 和上层关系系统判定。
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

void UActionGameplayAbilityBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (ActivationPolicy == EActionAbilityActivationPolicy::OnGiven && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		// OnGiven 型能力通常只服务当前这一次“授予即执行”的生命周期。
		// 因此结束后直接从 ASC 清掉授予，避免它留在可再次激活列表里造成重复触发。
		ActorInfo->AbilitySystemComponent->ClearAbility(Handle);
	}
}

const FGameplayTagContainer* UActionGameplayAbilityBase::GetCooldownTags() const
{
	const FGameplayTagContainer* ParentCooldownTags = Super::GetCooldownTags();
	FGameplayTagContainer ResolvedCooldownTags;
	float ResolvedCooldownDuration = 0.f;
	ResolveAbilityCooldownConfig(
		CurrentSpecHandle,
		CurrentActorInfo,
		ResolvedCooldownTags,
		ResolvedCooldownDuration);
	if (ResolvedCooldownTags.Num() == 0)
	{
		return ParentCooldownTags;
	}

	// 这里总是先复制父类标签，再叠加当前 Ability 的冷却标签。
	// 原因是父类返回的容器不归当前实例所有，直接改它会污染共享状态；
	// 用缓存拼出最终结果，既保留父类语义，也让子类配置保持只读合并。
	CachedCooldownTags.Reset();
	if (ParentCooldownTags)
	{
		CachedCooldownTags.AppendTags(*ParentCooldownTags);
	}

	CachedCooldownTags.AppendTags(ResolvedCooldownTags);
	return &CachedCooldownTags;
}

bool UActionGameplayAbilityBase::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!HasAbilityCooldownConfig(Handle, ActorInfo))
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent)
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	FGameplayTagContainer ResolvedCooldownTags;
	float ResolvedCooldownDuration = 0.f;
	ResolveAbilityCooldownConfig(Handle, ActorInfo, ResolvedCooldownTags, ResolvedCooldownDuration);
	if (ResolvedCooldownTags.Num() == 0)
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	if (!AbilitySystemComponent->HasAnyMatchingGameplayTags(ResolvedCooldownTags))
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		OptionalRelevantTags->AppendTags(ResolvedCooldownTags);
	}

	return false;
}

void UActionGameplayAbilityBase::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);

	FGameplayTagContainer ResolvedCooldownTags;
	float ResolvedCooldownDuration = 0.f;
	ResolveAbilityCooldownConfig(Handle, ActorInfo, ResolvedCooldownTags, ResolvedCooldownDuration);
	if (AbilityCooldownGameplayEffectClass == nullptr
		|| ResolvedCooldownTags.Num() == 0
		|| ResolvedCooldownDuration <= 0.f)
	{
		return;
	}

	// 基类负责把“冷却标签 + 冷却时长”翻译成一份通用冷却 GE。
	// 具体 GA 只维护自己的静态配置，不直接关心 GameplayEffectSpec 的拼装细节。
	FGameplayEffectSpecHandle CooldownSpecHandle =
		MakeOutgoingGameplayEffectSpec(AbilityCooldownGameplayEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpecHandle.IsValid())
	{
		return;
	}

	CooldownSpecHandle.Data->DynamicGrantedTags.AppendTags(ResolvedCooldownTags);
	CooldownSpecHandle.Data->SetSetByCallerMagnitude(
		ActionGameplayTags::SetByCaller_Cooldown_Duration,
		FMath::Max(0.f, ResolvedCooldownDuration));
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpecHandle);
}

void UActionGameplayAbilityBase::ResolveAbilityCooldownConfig(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer& OutCooldownTags,
	float& OutCooldownDuration) const
{
	(void)Handle;
	(void)ActorInfo;

	OutCooldownTags.Reset();
	OutCooldownTags.AppendTags(AbilityCooldownTags);
	OutCooldownDuration = AbilityCooldownDuration;
}

bool UActionGameplayAbilityBase::HasAbilityCooldownConfig(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayTagContainer ResolvedCooldownTags;
	float ResolvedCooldownDuration = 0.f;
	ResolveAbilityCooldownConfig(Handle, ActorInfo, ResolvedCooldownTags, ResolvedCooldownDuration);
	return AbilityCooldownGameplayEffectClass != nullptr
		&& ResolvedCooldownTags.Num() > 0
		&& ResolvedCooldownDuration > 0.f;
}
