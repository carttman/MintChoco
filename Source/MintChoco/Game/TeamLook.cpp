#include "Game/TeamLook.h"

#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

#include "Game/TeamTypes.h"
#include "MintChoco.h"
#include "Paint/PaintSettings.h"

namespace
{
	const FTeamLook NeutralLook = {FLinearColor(0.5f, 0.5f, 0.5f), FLinearColor(0.5f, 0.5f, 0.5f), 0.5f, 0.5f, 0.0f, 0.0f, 0.8f, 0.5f, 0.0f, 0.5f};

	enum class ELookEntry : uint8 { Color, Subsurface, Surface, Surface2, Count };

	FName EntryName(int32 TeamId, ELookEntry Entry)
	{
		static const TCHAR* const Suffixes[] = {TEXT("Color"), TEXT("Subsurface"), TEXT("Surface"), TEXT("Surface2")};
		static FName Cache[Teams::Count][static_cast<int32>(ELookEntry::Count)];
		if (!Teams::IsValidId(TeamId))
		{
			return NAME_None;
		}
		FName& Cached = Cache[TeamId][static_cast<int32>(Entry)];
		if (Cached.IsNone())
		{
			Cached = FName(*(FString(Teams::GetInternalName(TeamId)) + Suffixes[static_cast<int32>(Entry)]));
		}
		return Cached;
	}

	bool ReadVector(const UMaterialParameterCollection& Collection, const UWorld* World, FName Name, FLinearColor& Out)
	{
		if (World)
		{
			if (const UMaterialParameterCollectionInstance* const Instance = World->GetParameterCollectionInstance(&Collection))
			{
				if (Instance->GetVectorParameterValue(Name, Out))
				{
					return true;
				}
			}
		}
		if (const FCollectionVectorParameter* const Parameter = Collection.GetVectorParameterByName(Name))
		{
			Out = Parameter->DefaultValue;
			return true;
		}
		return false;
	}

	const UMaterialParameterCollection* ResolveCollection()
	{
		const TSoftObjectPtr<UMaterialParameterCollection>& Pointer = UPaintSettings::Get().TeamLookCollection;
		const UMaterialParameterCollection* const Collection = Pointer.LoadSynchronous();
		static bool bWarned = false;
		if (!Collection && !bWarned)
		{
			bWarned = true;
			UE_LOG(LogMintChoco, Warning, TEXT("TeamLookCollection %s 을 불러오지 못해 두 팀이 중립 회색으로 나온다."), *Pointer.ToString());
		}
		return Collection;
	}
}

FTeamLook TeamLook::GetFrom(const UMaterialParameterCollection* Collection, int32 TeamId, const UWorld* World)
{
	if (!Teams::IsValidId(TeamId))
	{
		return NeutralLook;
	}
	if (!Collection)
	{
		return NeutralLook;
	}

	FTeamLook Look = NeutralLook;
	FLinearColor Value;
	if (ReadVector(*Collection, World, ColorParameterName(TeamId), Value))
	{
		Look.Color = Value;
	}
	if (ReadVector(*Collection, World, SubsurfaceParameterName(TeamId), Value))
	{
		Look.Subsurface = Value;
	}
	if (ReadVector(*Collection, World, SurfaceParameterName(TeamId), Value))
	{
		Look.Roughness = Value.R;
		Look.Specular = Value.G;
		Look.Metallic = Value.B;
		Look.WetCoat = Value.A;
	}
	if (ReadVector(*Collection, World, Surface2ParameterName(TeamId), Value))
	{
		Look.SecondRoughness = Value.R;
		Look.SecondRoughnessWeight = Value.G;
		Look.FuzzAmount = Value.B;
		Look.FuzzRoughness = Value.A;
	}
	return Look;
}

FTeamLook TeamLook::Get(int32 TeamId, const UWorld* World)
{
	return GetFrom(ResolveCollection(), TeamId, World);
}

FLinearColor TeamLook::GetColor(int32 TeamId, const UWorld* World)
{
	return Get(TeamId, World).Color;
}

FColor TeamLook::GetDisplayColor(int32 TeamId, const UWorld* World)
{
	return Teams::IsValidId(TeamId) ? GetColor(TeamId, World).ToFColorSRGB() : FColor::Silver;
}

FName TeamLook::ColorParameterName(int32 TeamId)
{
	return EntryName(TeamId, ELookEntry::Color);
}

FName TeamLook::SubsurfaceParameterName(int32 TeamId)
{
	return EntryName(TeamId, ELookEntry::Subsurface);
}

FName TeamLook::SurfaceParameterName(int32 TeamId)
{
	return EntryName(TeamId, ELookEntry::Surface);
}

FName TeamLook::Surface2ParameterName(int32 TeamId)
{
	return EntryName(TeamId, ELookEntry::Surface2);
}
