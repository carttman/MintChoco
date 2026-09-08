#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Paint/PaintSplat.h"

#include "PaintSplatLog.generated.h"

USTRUCT()
struct FPaintSplatLogItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FPaintSplat Splat;
};

/**
 * Every splat the server has applied this match, in order. Replicating the log rather than
 * multicasting each splat means nothing is ever dropped, the order every client draws in is
 * the server's, and a client that joins late receives the whole history and simply replays it.
 * The owner reads it through a RepNotify and draws whatever it has not drawn yet.
 */
USTRUCT()
struct FPaintSplatLog : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FPaintSplatLogItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FPaintSplatLogItem, FPaintSplatLog>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FPaintSplatLog> : public TStructOpsTypeTraitsBase2<FPaintSplatLog>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
