#include "AnimNotifyState/AnimNotifyState_HitWindow.h"

#include "Animation/AnimMontage.h"
#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/Combat/HeroHitSourceComponent.h"
#include "DataAssets/Weapons/DataAsset_WeaponDefinition.h"
#include "Debug/ActionDebugHelper.h"
#include "Items/Weapons/HeroWeaponBase.h"

UAnimNotifyState_HitWindow::UAnimNotifyState_HitWindow()
	: Super()
{
}

FString UAnimNotifyState_HitWindow::GetNotifyName_Implementation() const
{
	const FName RuntimeWindowName = ResolveRuntimeHitWindowName();
	if (RuntimeWindowName == NAME_None)
	{
		return TEXT("HitWindow");
	}

	if (const FName TemplateName = ResolveHitWindowTemplateName(); TemplateName != NAME_None)
	{
		return FString::Printf(
			TEXT("HitWindow:%s [%s]"),
			*RuntimeWindowName.ToString(),
			*TemplateName.ToString());
	}

	return FString::Printf(TEXT("HitWindow:%s"), *RuntimeWindowName.ToString());
}

void UAnimNotifyState_HitWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp);
	if (!HeroCombatComponent)
	{
		return;
	}

	// 只允许当前真正仍在运行的战斗蒙太奇打开命中窗口。
	if (!IsCurrentCombatMontageHitWindowNotify(HeroCombatComponent, Animation))
	{
		return;
	}

	FActionHitWindowRuntimeEntry RuntimeEntry;
	RuntimeEntry.Weapon = ResolveCurrentWeapon(MeshComp);

	if (UHeroHitSourceComponent* HitSourceComponent = HeroCombatComponent->GetOwningHeroHitSourceComponent())
	{
		FActionHitWindowRuntimeConfig HitWindowRuntimeConfig = BuildLocalHitWindowRuntimeConfig();

		if (const UDataAsset_WeaponDefinition* WeaponDefinition = HeroCombatComponent->GetCurrentWeaponDefinition())
		{
			FActionHitWindowRuntimeConfig TemplateRuntimeConfig;
			if (TryResolveTemplateHitWindowRuntimeConfig(WeaponDefinition, TemplateRuntimeConfig))
			{
				// 武器模板一旦命中，就说明当前这段窗口希望完全复用武器定义里的正式模板。
				// 因此这里直接用模板结果覆盖通知本地配置，而不是做字段级拼接，
				// 避免资产侧误以为两边会自动合并。
				HitWindowRuntimeConfig = TemplateRuntimeConfig;
			}
		}

		RuntimeEntry.HitWindowHandle = HitSourceComponent->BeginHitWindow(HitWindowRuntimeConfig);

		if (RuntimeEntry.Weapon.IsValid())
		{
			TArray<FName> ActiveWeaponHitSourceIds;
			HitSourceComponent->GetActiveHitSourceIdsByType(
				EActionHitSourceType::WeaponCollision,
				ActiveWeaponHitSourceIds);
			if (ActiveWeaponHitSourceIds.Num() > 0)
			{
				RuntimeEntry.Weapon->BeginAttackDetectionForHitSources(
					HitWindowRuntimeConfig,
					ActiveWeaponHitSourceIds);
				RuntimeEntry.bStartedWeaponDetection = true;
			}
		}
	}

	CachedRuntimeByMesh.Add(MeshComp, RuntimeEntry);
}

void UAnimNotifyState_HitWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp);
	if (!HeroCombatComponent)
	{
		return;
	}

	// 如果旧蒙太奇的 NotifyEnd 晚到，此时不能再去关闭新攻击窗口。
	if (!IsCurrentCombatMontageHitWindowNotify(HeroCombatComponent, Animation))
	{
		CachedRuntimeByMesh.Remove(MeshComp);
		return;
	}

	const FActionHitWindowRuntimeEntry* RuntimeEntry = CachedRuntimeByMesh.Find(MeshComp);
	if (!RuntimeEntry)
	{
		return;
	}

	if (UHeroHitSourceComponent* HitSourceComponent = HeroCombatComponent->GetOwningHeroHitSourceComponent())
	{
		HitSourceComponent->EndHitWindow(RuntimeEntry->HitWindowHandle);
	}

	if (RuntimeEntry->bStartedWeaponDetection && RuntimeEntry->Weapon.IsValid())
	{
		RuntimeEntry->Weapon->EndAttackDetection();
	}

	CachedRuntimeByMesh.Remove(MeshComp);
}

UHeroCombatComponent* UAnimNotifyState_HitWindow::ResolveHeroCombatComponent(USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActionHeroCharacter* HeroCharacter = Cast<AActionHeroCharacter>(MeshComp->GetOwner());
	return HeroCharacter ? HeroCharacter->GetHeroCombatComponent() : nullptr;
}

AHeroWeaponBase* UAnimNotifyState_HitWindow::ResolveCurrentWeapon(USkeletalMeshComponent* MeshComp) const
{
	if (UHeroCombatComponent* HeroCombatComponent = ResolveHeroCombatComponent(MeshComp))
	{
		return HeroCombatComponent->GetCurrentEquippedWeapon();
	}

	return nullptr;
}

bool UAnimNotifyState_HitWindow::IsCurrentCombatMontageHitWindowNotify(
	const UHeroCombatComponent* HeroCombatComponent,
	UAnimSequenceBase* Animation) const
{
	if (!HeroCombatComponent)
	{
		return false;
	}

	UAnimMontage* CurrentRunningMontage = HeroCombatComponent->GetCurrentRunningAnimMontage();
	if (!CurrentRunningMontage)
	{
		return false;
	}

	if (UAnimMontage* NotifyMontage = Cast<UAnimMontage>(Animation))
	{
		return CurrentRunningMontage == NotifyMontage;
	}

	return true;
}

FName UAnimNotifyState_HitWindow::ResolveRuntimeHitWindowName() const
{
	return HitWindowName != NAME_None ? HitWindowName : GetFName();
}

FName UAnimNotifyState_HitWindow::ResolveHitWindowTemplateName() const
{
	return HitWindowTemplateName != NAME_None ? HitWindowTemplateName : HitWindowName;
}

FActionHitWindowRuntimeConfig UAnimNotifyState_HitWindow::BuildLocalHitWindowRuntimeConfig() const
{
	FActionHitWindowRuntimeConfig RuntimeConfig;
	RuntimeConfig.WindowName = ResolveRuntimeHitWindowName();
	RuntimeConfig.bUseWeaponCollisionDetection = bUseWeaponCollisionDetection;
	// 本地通知配置只负责把这枚 Notify 上明确写出来的字段组装成一份运行时窗口配置。
	// 如果稍后命中了武器模板，整份配置会被模板替换，而不是局部覆盖。
	for (const EActionHitSourceId SourceIdEnum : EnabledHitSourceIdEnums)
	{
		if (const FName SourceId = ActionHitSourceDefaults::ResolveHitSourceIdName(SourceIdEnum); SourceId != NAME_None)
		{
			RuntimeConfig.EnabledHitSourceIds.AddUnique(SourceId);
		}
	}

	for (const EActionHitSourceGroupId GroupIdEnum : EnabledHitSourceGroupIdEnums)
	{
		if (const FName GroupId = ActionHitSourceDefaults::ResolveHitSourceGroupIdName(GroupIdEnum); GroupId != NAME_None)
		{
			RuntimeConfig.EnabledHitSourceGroupIds.AddUnique(GroupId);
		}
	}

	RuntimeConfig.ResolvePolicy = ResolvePolicy;
	RuntimeConfig.RepeatResolveInterval = RepeatResolveInterval;
	RuntimeConfig.bOverrideKnockbackStrength = bOverrideKnockbackStrength;
	RuntimeConfig.OverrideKnockbackStrength = FMath::Max(OverrideKnockbackStrength, 0.f);
	return RuntimeConfig;
}

bool UAnimNotifyState_HitWindow::TryResolveTemplateHitWindowRuntimeConfig(
	const UDataAsset_WeaponDefinition* WeaponDefinition,
	FActionHitWindowRuntimeConfig& OutRuntimeConfig) const
{
	OutRuntimeConfig = FActionHitWindowRuntimeConfig();

	if (!WeaponDefinition)
	{
		return false;
	}

	const FName TemplateName = ResolveHitWindowTemplateName();
	if (TemplateName == NAME_None)
	{
		return false;
	}

	// 这里的输出语义是“按模板名解析出一份完整可执行的窗口配置”，
	// 不是把模板结果补丁式叠到本地字段上。
	return WeaponDefinition->TryResolveHitWindowConfigByName(
		TemplateName,
		ResolveRuntimeHitWindowName(),
		OutRuntimeConfig);
}
