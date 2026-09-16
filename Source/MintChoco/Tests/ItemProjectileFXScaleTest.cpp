#include "Misc/AutomationTest.h"

#include "Items/ItemProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 보호된 UPROPERTY의 CDO 값. 배율은 FX 슬롯 곁에 있어야 하므로 접근 지정자를 풀지 않는다. */
	float DefaultScale(const TCHAR* Name)
	{
		const UClass* const Class = AItemProjectile::StaticClass();
		const FFloatProperty* const Property = CastField<FFloatProperty>(Class->FindPropertyByName(Name));
		return Property ? Property->GetPropertyValue_InContainer(Class->GetDefaultObject()) : -1.0f;
	}
}

/**
 * 배율 기본값은 에셋 원래 크기다. 값을 넣지 않은 투사체(꿀벌)의 연출이 갑자기 커지거나,
 * 0이 들어가 아예 안 보이게 되면 안 된다. APaintBurst의 BurstFXScale과 같은 계약이다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemProjectileFXScaleDefaultTest,
	"MintChoco.Items.Projectile.FXScaleDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemProjectileFXScaleDefaultTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("TrailFXScale 기본값은 1"), DefaultScale(TEXT("TrailFXScale")), 1.0f);
	TestEqual(TEXT("BodyFXScale 기본값은 1"), DefaultScale(TEXT("BodyFXScale")), 1.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
