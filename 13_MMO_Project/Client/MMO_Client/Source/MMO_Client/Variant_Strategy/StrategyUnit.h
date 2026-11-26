// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "StrategyUnit.generated.h"

class USphereComponent;
class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;

/** Delegate to report that this unit has finished moving */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitMoveCompletedDelegate, AStrategyUnit*, Unit);

/**
 *  A simple strategy game unit
 *  Rather than react to inputs, it's controlled indirectly by the Strategy Player Controller
 */
UCLASS(abstract)
class AStrategyUnit : public ACharacter
{
	GENERATED_BODY()

private:

	/** Interaction range sphere */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USphereComponent* InteractionRange;

protected:

	/** Cast reference to the AI Controlling this unit */
	TObjectPtr<AAIController> AIController;

public:

	/** Constructor */
	AStrategyUnit();

protected:

	virtual void NotifyControllerChanged() override;

public:

	/** Stops unit movement immediately */
	void StopMoving();

	/** Notifies this unit that it was selected */
	void UnitSelected();

	/** Notifies this unit that it was deselected */
	void UnitDeselected();

	/** Notifies this unit that it's been interacted with by another actor */
	void Interact(AStrategyUnit* Interactor);

	/** Attempts to move this unit to the passed location, and optionally signals it to interact on arrival */
	void MoveToLocation(const FVector& Location, bool bInteract, const TArray<AStrategyUnit*> IgnoreList);

	/** Returns the last cached movement goal location */
	FVector GetMovementGoal() const;

protected:

	/** Called by EQS when the movement destination query has finished */
	UFUNCTION()
	void OnEQSFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);

	/** Called by the AI controller when this unit has finished moving */
	void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

	/** Wraps up movement logic */
	void HandleMoveFinished();

protected:

	/** Blueprint handler for strategy game selection */
	UFUNCTION(BlueprintImplementableEvent, Category="NPC", meta = (DisplayName="Unit Selected"))
	void BP_UnitSelected();

	/** Blueprint handler for strategy game deselection */
	UFUNCTION(BlueprintImplementableEvent, Category="NPC", meta = (DisplayName="Unit Deselected"))
	void BP_UnitDeselected();

	/** Blueprint handler to stop the unit's interaction animation */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="NPC", meta = (DisplayName="Stop Animation"))
	void BP_StopAnimation();

	/** Blueprint handler for strategy game interactions */
	UFUNCTION(BlueprintImplementableEvent, Category="NPC", meta = (DisplayName="Interaction Behavior"))
	void BP_InteractionBehavior(AStrategyUnit* Interactor);

protected:

	/** EnvQuery to use when this unit interacts after movement */
	UPROPERTY(EditAnywhere, Category="NPC")
	TObjectPtr<UEnvQuery> InteractionQuery;

	/** EnvQuery to use when this unit does not interact after movement */
	UPROPERTY(EditAnywhere, Category="NPC")
	TObjectPtr<UEnvQuery> NoInteractionQuery;

	/** How close we should get to the movement goal to consider ourselves as having reached it */
	UPROPERTY(EditAnywhere, Category="NPC", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float MovementAcceptanceRadius = 100.0f;

	/** Max distance to look for nearby units when doing an interaction check */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float InteractionRadius = 250.0f;

	/** EQS instance running the movement query for this unit */
	TObjectPtr<UEnvQueryInstanceBlueprintWrapper> EnvQueryInstance;

	/** Cached movement goal for this unit */
	FVector CurrentMovementGoal;

	/** If true, this unit will attempt to interact with a nearby unit upon finishing movement */
	bool bInteractOnArrival = false;

	/** List of actors to ignore when searching for units to interact with */
	TArray<AStrategyUnit*> InteractIgnoreList;

public:

	FOnUnitMoveCompletedDelegate OnMoveCompleted;
};
