#include "UI/TitleWidget.h"

#include "Components/Widget.h"

#include "Screen/ScreenFadeSubsystem.h"

namespace
{
	/** 한 틱에 흘려보낼 최대 시간(초). */
	constexpr float MaxDropStep = 1.0f / 30.0f;

	/** 화면 밖으로 조금 더 올린다. 로고의 외곽선이나 그림자가 위 가장자리에 걸려 보이지 않게. */
	constexpr float OffscreenMargin = 16.0f;
}

void FTitleLogoDrop::Restart()
{
	Phase = EPhase::WaitingForScreen;
	Elapsed = 0.0f;
}

bool FTitleLogoDrop::Tick(float DeltaTime, bool bScreenVisible)
{
	const EPhase Before = Phase;

	if (Phase == EPhase::WaitingForScreen)
	{
		if (bScreenVisible)
		{
			// 이번 틱의 시간은 세지 않는다. 화면은 방금 열리기 시작했다.
			Phase = EPhase::Delaying;
			Elapsed = 0.0f;
		}
		return Phase != Before;
	}

	Elapsed += FMath::Clamp(DeltaTime, 0.0f, MaxDropStep);

	if (Phase == EPhase::Delaying && Elapsed >= Delay)
	{
		Elapsed -= Delay;
		Phase = EPhase::Dropping;
	}

	if (Phase == EPhase::Dropping && Elapsed >= Duration)
	{
		Elapsed = 0.0f;
		Phase = EPhase::Landed;
	}

	return Phase != Before;
}

float FTitleLogoDrop::GetHeight() const
{
	switch (Phase)
	{
	case EPhase::Dropping:
		return EvaluateHeight(Duration > 0.0f ? Elapsed / Duration : 1.0f, Restitution, Bounces);
	case EPhase::Landed:
		return 0.0f;
	default:
		return 1.0f;
	}
}

float FTitleLogoDrop::EvaluateHeight(float Alpha, float Restitution, int32 Bounces)
{
	// 단위를 잡는다: 높이 1에서 떨어져 바닥에 닿기까지 시간 1. 그러면 중력은 2, 착지 속도도 2다.
	// n번째 튐은 속도 2e^n으로 떠올라 같은 시간(2e^n) 뒤에 내려오고, 꼭대기는 e^(2n)이다.
	const float E = FMath::Clamp(Restitution, 0.0f, 0.95f);
	const int32 NumBounces = E > 0.0f ? FMath::Max(Bounces, 0) : 0;

	float Total = 1.0f;
	float Speed = 2.0f;
	for (int32 Index = 0; Index < NumBounces; ++Index)
	{
		Speed *= E;
		Total += Speed;
	}

	float Time = FMath::Clamp(Alpha, 0.0f, 1.0f) * Total;
	if (Time < 1.0f)
	{
		return 1.0f - Time * Time;
	}

	Time -= 1.0f;
	Speed = 2.0f;
	for (int32 Index = 0; Index < NumBounces; ++Index)
	{
		Speed *= E;
		if (Time < Speed)
		{
			return FMath::Max(Speed * Time - Time * Time, 0.0f);
		}
		Time -= Speed;
	}
	return 0.0f;
}

void UTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Drop.Duration = DropDuration;
	Drop.Delay = DropDelay;
	Drop.Restitution = DropRestitution;
	Drop.Bounces = DropBounces;
	Drop.Restart();
	bDropFinished = false;

	if (!Title_Logo)
	{
		return;
	}

	// 다시 Construct돼도(뷰포트 재추가) 떨어지던 도중의 값을 원래 값으로 착각하지 않게 한 번만 잡는다.
	if (!bRestCaptured)
	{
		RestOpacity = Title_Logo->GetRenderOpacity();
		RestTranslation = Title_Logo->GetRenderTransform().Translation;
		bRestCaptured = true;
	}

	// 떨어지기 전에는 착지점에 투명하게 둔다. 화면 밖으로 옮겨 두면 캔버스가 컬링해서 그리지 않고,
	// 그러면 떨어질 거리를 잴 위치(캐시된 지오메트리)도 갱신되지 않는다. 불투명도 0은 그대로 그려진다.
	Title_Logo->SetRenderOpacity(0.0f);
	Title_Logo->SetRenderTranslation(RestTranslation);
}

void UTitleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Title_Logo || bDropFinished)
	{
		return;
	}

	// 서브시스템이 없으면(에디터 미리보기) 가림막도 없다.
	const UScreenFadeSubsystem* const Fade = UScreenFadeSubsystem::Get(this);
	const EScreenFadePhase FadePhase = Fade ? Fade->GetPhase() : EScreenFadePhase::Clear;
	const bool bScreenVisible = FadePhase == EScreenFadePhase::Clear || FadePhase == EScreenFadePhase::FadingIn;

	const bool bWasVisible = Drop.IsLogoVisible();
	Drop.Tick(InDeltaTime, bScreenVisible);
	if (!Drop.IsLogoVisible())
	{
		return;
	}

	if (!bWasVisible)
	{
		// 같은 틱 안에서 위로 올리고 보이게 한다. 틱이 그리기보다 먼저라 착지점에서 번쩍이지 않는다.
		DropDistance = MeasureDropDistance(MyGeometry);
		Title_Logo->SetRenderOpacity(RestOpacity);
	}

	Title_Logo->SetRenderTranslation(RestTranslation - FVector2D(0.0f, DropDistance * Drop.GetHeight()));
	bDropFinished = Drop.Phase == FTitleLogoDrop::EPhase::Landed;
}

float UTitleWidget::MeasureDropDistance(const FGeometry& MyGeometry) const
{
	const FGeometry& LogoGeometry = Title_Logo->GetCachedGeometry();
	const FVector2D LogoSize = LogoGeometry.GetLocalSize();

	// 한 번도 그려지지 않았으면 이 위젯 전체 높이만큼 올린다. 넉넉하지만 언제나 화면 밖이다.
	if (LogoSize.Y <= 0.0f || LogoGeometry.Scale <= 0.0f)
	{
		return MyGeometry.GetLocalSize().Y + OffscreenMargin;
	}

	// 둘 다 절대(데스크톱) 공간이다. 로고의 아래 끝에서 이 위젯의 위 끝까지를 로고 부모의 단위로 바꾼다.
	// 렌더 이동은 레이아웃 배율 앞에서 적용되므로 Scale로 나눈다.
	const float LogoBottom = LogoGeometry.LocalToAbsolute(FVector2D(0.0f, LogoSize.Y)).Y;
	const float WidgetTop = MyGeometry.LocalToAbsolute(FVector2D::ZeroVector).Y;
	return FMath::Max(LogoBottom - WidgetTop, 0.0f) / LogoGeometry.Scale + OffscreenMargin;
}
