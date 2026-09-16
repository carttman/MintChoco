#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "Items/ItemProfile.h"

#include "DessertBombardmentProfile.generated.h"

class APaintRain;
class UNiagaraSystem;
class UPaintballProfile;

/**
 * 디저트 폭격: 바라보는 수평 방향으로 맵 끝까지, 가까운 곳부터 순서대로 페인트탄이 떨어진다.
 * 맵의 끝은 도색 가능 표면들의 경계 상자다. 즉발(Duration 0); APaintRain이 이어받는다.
 */
UCLASS(BlueprintType)
class MINTCHOCO_API UDessertBombardmentProfile : public UItemProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TObjectPtr<UPaintballProfile> Paintball;

	/**
	 * 조준 중 발사 경로를 보여주는 미리보기 이펙트. 조준하는 본인 화면에만 생기며,
	 * 매 프레임 사용자 위치와 수평 시선 방향을 따라간다. 비워 두면 표시 없이 조준만 한다.
	 *
	 * 폭격 범위는 컴포넌트 스케일이 아니라 유저 파라미터로 건넨다(스프라이트는 스케일로
	 * 늘어나지 않는다): User.Length, User.Width 가 각각 띠의 길이와 폭(cm)이고, 팀 색은
	 * 다른 이펙트와 같은 User.TintColor다. 시스템이 읽지 않는 파라미터는 조용히 무시된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TObjectPtr<UNiagaraSystem> AimPreviewFX;

	/**
	 * 이펙트를 놓을 자리. 플레이어에서 조준 방향으로 이만큼 앞(cm), 0이면 발밑 그대로.
	 *
	 * 길이와는 무관하다 — 길이를 키워도 이펙트가 멀어지지 않는다. 스프라이트가 제 원점을
	 * 중심으로 그려지므로, 표시가 발밑에서 시작해 앞으로 뻗게 하고 싶으면 그리는 길이의
	 * 절반을 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ForceUnits = "cm"))
	float AimPreviewDistance = 0.0f;

	/**
	 * 이펙트에 건네는 길이(cm). 0이면 폭격이 닿는 끝까지. 실제 폭격 길이보다 길게는 건네지
	 * 않으므로, 맵 끝을 보고 있으면 이 값도 같이 줄어든다.
	 *
	 * User.Length로만 나간다. 이펙트가 그 길이만큼 늘어날지는 나이아가라 쪽 배선에 달렸고,
	 * 스폰 자리는 여기가 아니라 AimPreviewDistance가 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AimPreviewLength = 1000.0f;

	/**
	 * 미리보기로 보여 줄 좌우 폭(cm). 0이면 실제로 떨어지는 폭(Columns × ColumnSpacing).
	 *
	 * 길이와 달리 실제 폭으로 자르지 않는다: 길이를 넘겨 그리면 폭격이 닿지도 않는 앞쪽을
	 * 가리키게 되지만, 폭은 화살표 이미지의 가로세로 비를 맞추는 값이라 조금 좁거나 넓은 편이
	 * 보기 좋을 수 있다. 실제 범위를 그대로 보여 주려면 0으로 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AimPreviewWidth = 0.0f;

	/**
	 * 미리보기를 놓을 높이(cm). 플레이어 발밑이 0이다.
	 *
	 * 바닥을 훑어 지형에 얹지 않고 플레이어를 기준으로만 놓으므로, 표시는 언제나 사용자와 같은
	 * 높이를 따라다닌다. 앞쪽 지형이 높으면 그 안으로 들어갈 수 있고, 그럴 때 이 값을 올린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ForceUnits = "cm"))
	float AimPreviewHeight = 5.0f;

	/**
	 * 미리보기 색의 세기. 팀 색에 곱해서 User.TintColor로 나간다.
	 *
	 * 팀 색은 표면 알베도(전부 1 이하)라 바닥에 깔린 이펙트로는 흐리다. 1보다 크면 HDR 값이
	 * 되어 또렷해지고, 1이면 팀 색 그대로다. 알파는 여기 곱해지지 않는다 — 표시는 언제나 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0"))
	float AimPreviewIntensity = 3.0f;

	/** 비워 두면 APaintRain 그대로. 연출을 붙이려면 서브클래스 BP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TSubclassOf<APaintRain> RainClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "10", ForceUnits = "cm"))
	float RowSpacing = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "1"))
	int32 Columns = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ColumnSpacing = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float RowInterval = 0.05f;

	/**
	 * 좌클릭하고 첫 행이 떨어지기까지의 시간(초). 그동안 경로에 예고 표식이 순차로 놓인다.
	 * 0이면 예고 없이 곧바로 떨어진다.
	 *
	 * 상대가 피할 틈이기도 하다: 길게 잡을수록 맞히기 어렵고 짧을수록 회피가 어렵다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "s"))
	float LeadInSeconds = 1.0f;

	/**
	 * 리드인 동안 경로에 놓이는 예고 표식. 비워 두면 표식 없이 기다리기만 한다.
	 *
	 * 행마다 하나씩 가운데 열 자리에, 아래로 트레이스해 지형에 얹는다. 조준 미리보기
	 * (AimPreviewFX, 본인 화면 전용)와 달리 이건 모든 플레이어가 본다 — 피하라는 표시다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment")
	TObjectPtr<UNiagaraSystem> TelegraphFX;

	/**
	 * 표식을 몇 행마다 하나씩 놓을지. 1이면 행마다, 5면 다섯 행에 하나다.
	 *
	 * 표식 수만 줄이고 훑는 시간은 리드인 그대로다 — 간격이 그만큼 벌어진다. 행 수는 맵 크기를
	 * 따라가므로(상한 MaxRows) 넓은 맵에서 표식이 촘촘해 보이면 이 값을 올린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "1"))
	int32 TelegraphRowStride = 1;

	/**
	 * 표식을 지면에서 이만큼 띄운다(cm). 0이면 지면에 딱 붙는다.
	 *
	 * 0으로 두면 이펙트의 바닥 카드가 지형과 공면이 되어 깊이 판정이 매 프레임 뒤집히고, 표식
	 * 하나하나가 깜빡인다. 위 AimPreviewHeight가 같은 이유로 있는 값이고 기본값도 같다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TelegraphHeight = 5.0f;

	/** 경계 상자의 최고점에서 이만큼 위에서 떨어진다(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DropHeight = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float DropSpeed = 1500.0f;

	/** 행 수의 상한. 맵이 아무리 커도 이 이상은 떨어지지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bombardment", meta = (ClampMin = "1"))
	int32 MaxRows = 200;

	virtual void LogUnsetReferences(const UObject* Owner) const override;
};
