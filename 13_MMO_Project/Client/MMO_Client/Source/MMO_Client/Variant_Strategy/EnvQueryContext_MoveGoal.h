// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_MoveGoal.generated.h"

/**
 *  Simple EnvQueryContext that returns a Unit's current movement goal location
 */
UCLASS()
class MMO_CLIENT_API UEnvQueryContext_MoveGoal : public UEnvQueryContext
{
	GENERATED_BODY()
	
	/** Provides context data to the query instance */
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
