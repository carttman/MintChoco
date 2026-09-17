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
		// 거리는 로그에도 쓰므로 한 번만 구한다. 제곱 비교보다 근을 한 번 더 뽑지만, 이 루프는
		// 아이템이 터질 때만 돌고 그때 알아야 하는 것은 "얼마나 가까웠나"다.
		const double Distance = FVector::Dist(Unit->GetActorLocation(), Origin);
		if (Distance > Radius + Slack)
		{
			continue;
		}

		// 밀기가 먼저다. 스턴이 걸린 뒤에도 공중 속도는 유지되므로 순서가 결과를 바꾸지는 않지만,
		// 슈퍼아머 판정은 둘 다 각자 한다.
		if (bKnockback)
		{
			Unit->Knockback(Origin);
		}
		const bool bStunned = Unit->TryApplyStun();
		if (bStunned)
		{
			++Affected;
		}

		// 합계만으로는 "상대가 나를 맞히지 않았는데 걸렸다"를 가릴 수 없다. 누가 누구를,
		// 얼마나 가까이에서 건드렸는지가 여기서만 남는다.
		UE_LOG(LogMintChoco, Verbose, TEXT("[스턴][광역] %s(%s팀) ← %s(%s팀), 거리 %.0f / 반경 %.0f(+여유 %.0f) → %s."),
			*GetNameSafe(Unit), Teams::GetDisplayName(Unit->GetTeam()),
			*GetNameSafe(Instigator), Teams::GetDisplayName(InstigatorTeam),
			Distance, Radius, Slack, bStunned ? TEXT("적용") : TEXT("거절"));
	}

	UE_LOG(LogMintChoco, Verbose, TEXT("광역 효과: %s 팀, 반경 %.0f, %d명 스턴."), Teams::GetDisplayName(InstigatorTeam), Radius, Affected);
	return Affected;
}
