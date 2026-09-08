// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UnitDataAsset.h"

const FUnitActionFeedback* UUnitDataAsset::FindFeedback(EUnitAction Action) const
{
	return ActionFeedback.Find(Action);
}
