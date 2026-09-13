#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "ItemProfile.generated.h"

class UAnimSequenceBase;
class UItemAbility;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;

/** 유지 자세가 몸의 어디를 덮는지. 클립을 어떻게 만들었는지에 맞춘다. */
UENUM(BlueprintType)
enum class EItemPoseBlend : uint8
{
	/** 전신을 덮는다. 이동이 멈춘 것처럼 보인다(설치, 시전, 제자리 회전). */
	FullBody,
	/** 상체만 덮고 하체는 로코모션이 그대로 돈다(조준, 들고 달리기). */
	UpperBody,
};

/**
 * 아이템을 쓰는 순간 한 번 재생되는 동작. 애님 그래프의 슬롯에 동적 몽타주로 올라가므로
 * 몽타주 에셋을 따로 만들 필요가 없고, 애님 블루프린트에 상태를 추가할 필요도 없다.
 *
 * 두 캐릭터가 스켈레톤을 공유하므로 클립은 아이템당 하나면 된다(ActivateFX와 같은 이유로
 * UnitData가 아니라 아이템 프로필에 있다).
 */
USTRUCT(BlueprintType)
struct FItemUseAnimation
{
	GENERATED_BODY()

	/** 비어 있으면 아무 동작도 하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> Animation;

	/**
	 * 얹을 슬롯. 상체 슬롯이면 이동이 그대로 살아 있고, 전신 슬롯이면 전신을 덮는다.
	 *
	 * 애님 그래프에 같은 이름의 Slot 노드가 **있고 그 순간 실제로 평가되어야** 보인다. 노드가
	 * 없을 때도, 노드가 Blend Poses by bool의 꺼진 가지에 있을 때도 엔진은 오류를 내지 않는다:
	 * 몽타주는 정상적으로 돌고 받아 줄 노드만 없는 상태라 조용히 안 보인다. 아무것도 재생되지
	 * 않는 Slot 노드는 입력을 그대로 흘려보내므로, 항상 평가되는 자리에 두어도 손해가 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName Slot = TEXT("UpperBody");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendIn = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendOut = 0.15f;
};

/**
 * 아이템 한 종의 정의. 아이템 하나 = 에셋 하나이며 로직은 없다.
 *
 * 무엇을 보여주고(아이콘, 픽업 메시, 연출) 얼마나 오래 가는지, 그리고 발동하면 어떤
 * 어빌리티가 도는지만 들어 있다. 효과 자체는 AbilityClass가 가리키는 UItemAbility의
 * 서브클래스가 구현하고, 그 어빌리티가 읽을 튜닝값은 이 클래스의 서브클래스에 둔다.
 * 아이템을 추가하는 일은 프로필 서브클래스 + 어빌리티 서브클래스 + 에셋 하나다.
 */
UCLASS(Abstract, BlueprintType)
class MINTCHOCO_API UItemProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** HUD 슬롯에 보이는 아이콘. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<USoundBase> PickupSound;

	/**
	 * 효과 지속시간(초). 같은 아이템을 효과 중에 다시 쓰면 이 값으로 다시 시작한다.
	 * 0이면 즉발이다: GE도 상태 태그도 없이 어빌리티가 발동 직후 끝나고, 효과는 스폰된 액터가 이어받는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0", ForceUnits = "s"))
	float Duration = 2.0f;

	/** 발동하면 도는 어빌리티. 비어 있으면 주울 수는 있어도 아무 일도 하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UItemAbility> AbilityClass;

	/** 효과 중 캐릭터에 붙는 이펙트. 두 캐릭터가 같은 연출을 쓰므로 UnitData가 아니라 여기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UNiagaraSystem> ActivateFX;

	/** ActivateFX가 붙을 때의 균일 배율. 1이 에셋 원래 크기다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.01"))
	float ActivateFXScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TObjectPtr<USoundBase> ActivateSound;

	/** 아이템을 쓰는 순간 한 번. 모든 머신에서 같은 타이밍에 나온다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FItemUseAnimation UseAnimation;

	/**
	 * 효과가 도는 동안 유지하는 자세. 애님 블루프린트의 ItemPose 변수로 나가므로, 어떻게 섞을지는
	 * 애님 그래프가 정한다(상체만 덮을지, 전신을 덮을지). 시퀀스 플레이어에 바인딩하고 Loop를 켠다.
	 *
	 * 조준형 아이템(꿀풍선)은 조준하는 동안 이 자세를 잡는다. 효과가 시작되면 상태 태그가
	 * 이어받으므로 자세는 끊기지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequenceBase> PoseAnimation;

	/**
	 * 그 자세가 덮는 범위. 애님 블루프린트가 이 값으로 전신 가지와 상체 가지 중 하나를 고른다.
	 *
	 * 전신은 이동이 멈춘 것처럼 보이므로 설치·시전에 맞고, 상체는 하체가 계속 걸으므로 조준이나
	 * 무언가를 들고 달리는 자세에 맞는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	EItemPoseBlend PoseBlend = EItemPoseBlend::FullBody;

	/** 효과 중 ASC에 붙는 상태 태그. 어빌리티 클래스가 정하며, 없으면 빈 태그. */
	UFUNCTION(BlueprintPure, Category = "Item")
	FGameplayTag GetStateTag() const;

	/**
	 * 스피드 스타처럼 무브먼트의 속도 부스트 플래그를 쓰는 아이템인지. 서버가 클라이언트의
	 * 플래그를 인정할지 판단할 때 든 아이템을 이걸로 묻는다.
	 */
	virtual bool GrantsSpeedBoost() const { return false; }

	/** 즉발 아이템인지(Duration 0). */
	bool IsInstant() const { return Duration <= 0.0f; }

	/** 효과 중 잉크병을 오버라이드 재질(빨강)로 보이는 아이템인지(무한 탄환). 슬롯이 태그를 보고 켠다. */
	virtual bool OverridesInkLook() const { return false; }

	/** 장착·습득 시점에 비어 있으면 "아무 일도 안 일어나는" 참조를 경고로 남긴다. */
	virtual void LogUnsetReferences(const UObject* Owner) const;
};
