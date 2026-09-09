// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 팀 번호 규약.
 *
 * 로비의 팀 선택, PlayerState의 Team, 스폰 지점의 Team이 모두 이 값을 쓴다.
 * 리터럴 0과 1이 코드 여기저기에 흩어지면 나중에 팀이 늘거나 순서가 바뀔 때
 * 어느 0이 민트를 뜻했는지 알 수 없게 되므로, 비교는 전부 이 상수로 한다.
 *
 * 네임스페이스로 감싼 이유는 이 헤더가 넓게 포함되기 때문이다. 전역에 두면
 * 유니티 빌드에서 다른 파일의 지역 변수와 이름이 겹쳐 C4459로 막힌다.
 */
namespace Teams
{
	inline constexpr int32 Mint = 0;
	inline constexpr int32 Choco = 1;
	inline constexpr int32 Count = 2;

	/** 팀이 정해지지 않음. 어느 팀이든 쓸 수 있는 중립 스폰 지점이 이 값을 갖는다. */
	inline constexpr int32 None = INDEX_NONE;

	/** 엔진 전역의 IsValid(UObject*)와 헷갈리지 않도록 이름을 달리 둔다. */
	inline bool IsValidId(int32 TeamId)
	{
		return TeamId >= 0 && TeamId < Count;
	}

	/**
	 * 팀 색. 로비 목록과 디버그 표시가 같은 값을 쓴다.
	 * 화면 두 곳에 같은 팀이 다른 색으로 나오는 일이 없도록 여기 한 곳에만 둔다.
	 */
	inline const FColor MintColor = FColor(62, 180, 137);
	inline const FColor ChocoColor = FColor(210, 105, 30);

	inline FColor GetDisplayColor(int32 TeamId)
	{
		switch (TeamId)
		{
		case Mint:  return MintColor;
		case Choco: return ChocoColor;
		default:    return FColor::Silver;
		}
	}

	/** 화면과 로그에 쓰는 팀 이름. 승패 문구는 부르는 쪽이 만든다. */
	inline const TCHAR* GetDisplayName(int32 TeamId)
	{
		switch (TeamId)
		{
		case Mint:  return TEXT("민트");
		case Choco: return TEXT("초코");
		default:    return TEXT("없음");
		}
	}
}
