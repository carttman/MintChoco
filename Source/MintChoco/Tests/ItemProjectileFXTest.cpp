#include "Misc/AutomationTest.h"

#include "NiagaraSystem.h"

#include "Items/ItemProjectile.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * 투사체에 붙는 이펙트를 켤 머신을 고르는 규칙. 트레일과 바디가 같은 판단을 탄다.
 * 데디케이티드 서버는 그림을 그리지 않고, 슬롯이 비어 있으면 어느 머신에서도 아무 일이 없다.
 * 슬롯이 찼는지만 보는 판단이라 에셋을 컴파일할 필요가 없다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FItemProjectileFXNetModeTest,
	"MintChoco.Items.Projectile.FXNetMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FItemProjectileFXNetModeTest::RunTest(const FString& Parameters)
{
	UNiagaraSystem* const FX = NewObject<UNiagaraSystem>();

	TestFalse(TEXT("데디 서버는 그림을 그리지 않는다"), AItemProjectile::ShouldShowFX(NM_DedicatedServer, FX));
	TestTrue(TEXT("리슨 서버는 그린다"), AItemProjectile::ShouldShowFX(NM_ListenServer, FX));
	TestTrue(TEXT("클라이언트는 그린다"), AItemProjectile::ShouldShowFX(NM_Client, FX));
	TestTrue(TEXT("스탠드얼론도 그린다"), AItemProjectile::ShouldShowFX(NM_Standalone, FX));

	TestFalse(TEXT("슬롯이 비면 그리지 않는다"), AItemProjectile::ShouldShowFX(NM_Standalone, nullptr));
	TestFalse(TEXT("슬롯이 비면 데디 서버도 마찬가지"), AItemProjectile::ShouldShowFX(NM_DedicatedServer, nullptr));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
