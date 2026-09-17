#pragma once

#include "CoreMinimal.h"

/**
 * 제자리에서 둥둥 떠 있는 연출의 계산. 아이템 상자와 풍선이 같은 식을 쓴다.
 *
 * 연출이라 복제하지 않는다. 흔들리는 것은 메시의 상대 위치뿐이고 판정을 맡은 컴포넌트는
 * 제자리에 있으므로, 머신마다 위상이 달라도 맞는 자리는 모든 화면에서 같다. 판정까지
 * 같이 흔들면 서버와 화면이 반대로 움직일 때 오차가 두 배가 된다 — 그래서 흔들지 않는다.
 */
namespace BobMotion
{
	/**
	 * 상하 오프셋(cm): Amplitude × sin(2π·(FrequencyHz·Time + Phase01)).
	 *
	 * Phase01은 한 주기 안의 위치(0~1)다. 같은 순간에 태어난 것들이 한 몸처럼 오르내리지
	 * 않게 하려는 값이라, 0과 1은 같은 결과다. 진폭이나 주파수가 0 이하면 0.
	 */
	inline float Offset(float Time, float Amplitude, float FrequencyHz, float Phase01 = 0.0f)
	{
		if (Amplitude <= 0.0f || FrequencyHz <= 0.0f)
		{
			return 0.0f;
		}
		return Amplitude * FMath::Sin(2.0f * PI * (FrequencyHz * Time + Phase01));
	}

	/**
	 * 위치에서 뽑은 위상(0~1). 레벨에 놓여 같은 프레임에 시작하는 것들에 쓴다: 시각이 아니라
	 * 자리에서 나오므로 모든 머신이 같은 값을 내고, 터졌다 다시 생겨도 위상이 튀지 않는다.
	 *
	 * 해시라 서로 다른 자리가 같은 위상을 받는 일은 있을 수 있다. 연출이 조금 겹칠 뿐이다.
	 */
	inline float PhaseFromVector(const FVector& Location)
	{
		// cm 단위로 내려 섞는다. 배치를 1cm 옮겨도 위상이 달라지지만, 어차피 어느 위상이든
		// 좋은 값이므로 상관없다. 중요한 것은 같은 자리가 언제나 같은 값을 준다는 것뿐이다.
		const uint32 X = static_cast<uint32>(FMath::RoundToInt(Location.X));
		const uint32 Y = static_cast<uint32>(FMath::RoundToInt(Location.Y));
		const uint32 Z = static_cast<uint32>(FMath::RoundToInt(Location.Z));
		const uint32 Hash = HashCombine(HashCombine(GetTypeHash(X), GetTypeHash(Y)), GetTypeHash(Z));

		constexpr uint32 Steps = 1024;
		return static_cast<float>(Hash % Steps) / static_cast<float>(Steps);
	}
}
