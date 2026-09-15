#pragma once

#include "CoreMinimal.h"

class UMaterialParameterCollection;
class UWorld;

/** 한 팀의 색과 광택. MPC_TeamLook 이 담은 값 그대로(리니어)다. */
struct FTeamLook
{
	/** 알베도. */
	FLinearColor Color = FLinearColor::White;

	/** Substrate SSS MFP. */
	FLinearColor Subsurface = FLinearColor::White;

	float Roughness = 0.5f;

	/** UE 규약 0..1 (F0 = 0.08 * Specular). */
	float Specular = 0.5f;

	float Metallic = 0.0f;

	float WetCoat = 0.0f;
};

/**
 * 팀 색의 단일 출처. UPaintSettings::TeamLookCollection(MPC_TeamLook)을 읽는다.
 * 머티리얼은 같은 컬렉션을 MF_TeamLook 으로 직접 읽으므로 화면(UI, 데칼, 이펙트)과
 * 셰이더가 같은 값을 쓴다. 컬렉션이 없으면 내장 기본값으로 두 팀을 구분한다.
 */
namespace TeamLook
{
	/**
	 * World 를 주면 그 월드의 컬렉션 인스턴스(런타임 SetVectorParameterValue 포함)를,
	 * 없으면 에셋 기본값을 읽는다. 팀이 아닌 id 는 중립 회색.
	 */
	MINTCHOCO_API FTeamLook Get(int32 TeamId, const UWorld* World = nullptr);

	MINTCHOCO_API FLinearColor GetColor(int32 TeamId, const UWorld* World = nullptr);

	/** Slate 글자와 디버그 문자열용 sRGB. 팀이 아니면 Silver. */
	MINTCHOCO_API FColor GetDisplayColor(int32 TeamId, const UWorld* World = nullptr);

	/** 컬렉션을 직접 지정해 읽는다. nullptr 이면 내장 기본값. 테스트와 툴용. */
	MINTCHOCO_API FTeamLook GetFrom(const UMaterialParameterCollection* Collection, int32 TeamId, const UWorld* World = nullptr);

	/** MPC 항목 이름: "MintColor", "ChocoSubsurface", "MintSurface" ... 팀이 아니면 NAME_None. */
	MINTCHOCO_API FName ColorParameterName(int32 TeamId);
	MINTCHOCO_API FName SubsurfaceParameterName(int32 TeamId);
	MINTCHOCO_API FName SurfaceParameterName(int32 TeamId);

	/** 팀 색을 쓰는 모든 마스터 머티리얼이 선언하는 스칼라 파라미터. MI 와 MID 가 팀 id 를 넣는다. */
	inline const FName TeamIdParameter(TEXT("TeamId"));

	/** 팀 색을 받는 나이아가라 시스템의 유저 파라미터: 착탄, 파열, 총구 화염, 차징 홀드. */
	inline const FName NiagaraTintParameter(TEXT("User.TintColor"));
}
