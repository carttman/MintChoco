#include "Items/ItemAreaEffect.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "Game/TeamTypes.h"
#include "Game/Unit.h"
#include "MintChoco.h"

bool FItemAreaEffect::ShouldAffect(int32 VictimTeam, int32 InstigatorTeam, bool bIsInstigator)
{
	if (bIsInstigator)
	{
		return false;
	}
	return !(Teams::IsValidId(InstigatorTeam) && VictimTeam == InstigatorTeam);
}

int32 FItemAreaEffect::Apply(UWorld& World, const FVector& Origin, float Radius, int32 InstigatorTeam, const AActor* Instigator, bool bKnockback)
{
	int32 Affected = 0;
	for (TActorIterator<AUnit> It(&World); It; ++It)
	{
		AUnit* const Unit = *It;
		if (!Unit || !ShouldAffect(Unit->GetTeam(), InstigatorTeam, Unit == Instigator))
		{
			continue;
		}

		const float Slack = Unit->GetCapsuleComponent() ? Unit->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
		if (FVector::DistSquared(Unit->GetActorLocation(), Origin) > FMath::Square(Radius + Slack))
		{
			continue;
		}

		// 밀기가 먼저다. 스턴이 걸린 뒤에도 공중 속도는 유지되므로 순서가 결과를 바꾸지는 않지만,
		// 슈퍼아머 판정은 둘 다 각자 한다.
		if (bKnockback)
		{
			Unit->Knockback(Origin);
		}
		if (Unit->TryApplyStun())
		{
			++Affected;
		}
	}

	UE_LOG(LogMintChoco, Verbose, TEXT("광역 효과: %s 팀, 반경 %.0f, %d명 스턴."), Teams::GetDisplayName(InstigatorTeam), Radius, Affected);
	return Affected;
}
