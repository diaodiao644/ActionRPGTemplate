#include "AnimNotify/AnimNotify_CombatWeaponPresentationSwitch.h"

#include "Characters/ActionHeroCharacter.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_CombatWeaponPresentationSwitch::UAnimNotify_CombatWeaponPresentationSwitch()
	: Super()
{
}

FString UAnimNotify_CombatWeaponPresentationSwitch::GetNotifyName_Implementation() const
{
	return bAttachToWeaponSocket
		? TEXT("CombatWeaponToHand")
		: TEXT("CombatWeaponToHolster");
}

void UAnimNotify_CombatWeaponPresentationSwitch::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActionHeroCharacter* OwnerHeroCharacter = Cast<AActionHeroCharacter>(MeshComp->GetOwner());
	if (!OwnerHeroCharacter || !OwnerHeroCharacter->GetHeroCombatComponent())
	{
		return;
	}

	// 这枚 Notify 只把“这一帧当前已装备武器该挂在哪个 socket”交给 HeroCombatComponent。
	// CombatMode 写入、移动锁和 linked layer 开关都不在这里维护；
	// 若 Notify 缺失或未命中，正式兜底会回到 HeroCombatComponent 的蒙太奇结束收尾。
	OwnerHeroCharacter->GetHeroCombatComponent()->NotifyCombatWeaponPresentationSwitchFrame(bAttachToWeaponSocket);
}
