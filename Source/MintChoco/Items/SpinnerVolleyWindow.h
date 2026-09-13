#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "SpinnerVolleyWindow.generated.h"

/**
 * 애니메이션에서 "이 동안 돈다"를 표시하는 구간. 스위트 스피너가 이 구간에서만 산탄을 쏜다.
 *
 * 이 클래스는 실행 시점에 아무것도 하지 않는다. 표시일 뿐이고, 스피너는 시퀀스에서 이 구간의
 * 시작·끝 시각만 읽어 간다.
 *
 * 런타임 노티파이로 발사하지 않는 이유는 서버 때문이다. 페인트는 서버가 찍어야 하는데,
 * 데디케이티드 서버는 메시의 포즈를 돌리지 않아 노티파이가 오지 않는다. 반면 구간은 에셋에
 * 박힌 데이터라 포즈를 돌리지 않아도 그대로 읽을 수 있다.
 */
UCLASS(meta = (DisplayName = "Spinner Volley Window"))
class MINTCHOCO_API UAnimNotifyState_SpinnerVolley : public UAnimNotifyState
{
	GENERATED_BODY()
};
