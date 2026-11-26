// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Strategy/EnvQueryContext_MoveGoal.h"
#include "Variant_Strategy/StrategyUnit.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

void UEnvQueryContext_MoveGoal::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	// get the querying unit
	if (AStrategyUnit* QuerierActor = Cast<AStrategyUnit>(QueryInstance.Owner.Get()))
	{
		// add the last recorded danger location to the context
		UEnvQueryItemType_Point::SetContextHelper(ContextData, QuerierActor->GetMovementGoal());
	}
}
