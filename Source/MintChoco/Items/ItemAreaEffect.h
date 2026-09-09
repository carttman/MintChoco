#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/**
 * 광역 아이템(꿀풍선, 히어로 랜딩, 꿀벌)이 반경 안의 플레이어에게 거는 것. 서버 전용.
 *
 * 규칙은 하나다: 사용자 본인과 같은 팀은 빼고, 상대만 (선택적으로) 밀어낸 뒤 스턴한다.
 * 페인트는 여기서 다루지 않는다. 그것은 APaintBurst가 뿌리는 탄의 몫이고, 탄은 팀을 가리지 않는다.
 */
struct MINTCHOCO_API FItemAreaEffect
{
	/** 상대인지. 팀이 없는 사용자(샘플 맵)는 본인만 빼고 전부 상대다. */
	static bool ShouldAffect(int32 VictimTeam, int32 InstigatorTeam, bool bIsInstigator);

	/**
	 * 반경 안(캡슐 중심 기준, 캡슐 반지름만큼 여유)의 상대 유닛을 처리한다. 영향 받은 수를 돌려준다.
	 * bKnockback이면 Origin에서 멀어지는 쪽으로 민 뒤 스턴한다. 슈퍼아머는 둘 다 무시한다.
	 */
	static int32 Apply(UWorld& World, const FVector& Origin, float Radius, int32 InstigatorTeam, const AActor* Instigator, bool bKnockback);
};
