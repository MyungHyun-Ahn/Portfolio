// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StrategyPlayerController.generated.h"

class AStrategyPawn;
class UInputMappingContext;
class UNiagaraSystem;
struct FInputActionValue;
class AStrategyHUD;
class UInputAction;
struct FInputActionInstance;
class AStrategyUnit;
class UStrategyTouchControls;

/**
 *  Player Controller for a top-down strategy game.
 *  Handles unit selection and commands.
 *  Implements both mouse and touch controls.
 */
UCLASS(abstract)
class AStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Strategy Pawn associated with this controller */
	TObjectPtr<AStrategyPawn> ControlledCameraPawn;

	/** Strategy HUD associated with this controller */
	TObjectPtr<AStrategyHUD> StrategyHUD;

	/** Input mapping context to use with mouse input */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputMappingContext* MouseMappingContext;

	/** Input mapping context to use with touch input */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputMappingContext* TouchMappingContext;

	/** If true, the player is adding or removing units to the selected units list */
	bool bSelectionModifier = false;

	/** If true, double-tap touch select all mode is active */
	bool bDoubleTapActive = false;

	/** If true, allow the player to interact with game objects */
	bool bAllowInteraction = true;

	/** Input Action for moving the camera */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveCameraAction;

	/** Input Action for zooming the camera */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ZoomCameraAction;

	/** Input Action for resetting the camera to its default position */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ResetCameraAction;

	/** Input Action for select click */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SelectClickAction;

	/** Input Action for additive select click */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SelectClickAdditiveAction;

	/** Input Action for select all double click */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SelectAllDoubleClickAction;

	/** Input Action for select press and hold */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SelectHoldAction;

	/** Input Action for click interaction */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractClickAction;

	/** Input Action for interaction press and hold */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractHoldAction;

	/** Input Action for modifying selection mode */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SelectionModifierAction;

	/** Input Action for primary touch hold */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* TouchPrimaryHoldAction;

	/** Input Action for secondary touch */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* TouchSecondaryAction;

	/** Pointer to the mobile controls widget */
	TObjectPtr<UStrategyTouchControls> MobileControlsWidget;

	/** Touch controls widget class to spawn on mobile platforms */
	UPROPERTY(EditAnywhere, Category="Input")
	TSubclassOf<UStrategyTouchControls> MobileControlsWidgetClass;

	/** If true, the PC will be initialized with touchscreen controls */
	UPROPERTY(EditAnywhere, Category="Input")
	bool bForceTouchControls = false;

	/** Max distance to look for nearby units when doing a click or touch interaction */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0, ClampMax = 10000, Units = "cm"))
	float SelectionRadius = 250.0f;

	/** Cached starting position for camera drag scrolling */
	FVector2D StartingDragScrollPosition;

	/** Cached starting position for box select */
	FVector2D StartingBoxSelectionPosition;

	/** Last game time when a drag scroll was performed, so we can avoid spamming commands on drag scroll end */
	float LastTouchDragScrollTime = 0.0f;

	/** Time the finger needs to be held down to initiate a drag scroll */
	UPROPERTY(EditAnywhere, Category="Input", meta = (ClampMin = 0, ClampMax = 1, Units = "s"))
	float TouchDragScrollHoldTime = 0.15f;

	/** Current camera zoom level */
	float CameraZoom = 0.0f;

	/** Default camera zoom level */
	float DefaultZoom = 1000.0f;

	/** Minimum allowed camera zoom level */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = 0, ClampMax = 10000))
	float MinZoomLevel = 1000.0f;

	/** Maximum allowed camera zoom level */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = 0, ClampMax = 10000))
	float MaxZoomLevel = 2500.0f;

	/** Scales zoom inputs by this value */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = 0, ClampMax = 1000))
	float ZoomScaling = 100.0f;

	/** Affects how fast the camera moves while dragging with the mouse */
	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = 0, ClampMax = 10000))
	float DragMultiplier = 0.1f;

	/** Trace channel to use for selection trace checks */
	UPROPERTY(EditAnywhere, Category = "Selection")
	TEnumAsByte<ETraceTypeQuery> SelectionTraceChannel;

	/** Currently selected unit list */
	TArray<AStrategyUnit*> ControlledUnits;

public:

	/** Constructor */
	AStrategyPlayerController();

	/** BeginPlay initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn);

public:

	/** Updates selected units from the HUD's drag select box */
	void DragSelectUnits(const TArray<AStrategyUnit*>& Units);

	/** Passes the list of selected units */
	const TArray<AStrategyUnit*>& GetSelectedUnits();

	/** Returns the default camera zoom percentage value */
	float GetDefaultZoomPercentage() const;

protected:

	/** Returns true if the PC should run using touchscreen controls */
	bool ShouldUseTouchControls() const;

	// mouse + keyboard input

protected:

	/** Moves the camera by the given input */
	void MoveCamera(const FInputActionValue& Value);

	/** Changes the camera zoom level by the given input */
	void ZoomCamera(const FInputActionValue& Value);

	/** Resets the camera to its initial value */
	void ResetCamera(const FInputActionValue& Value);

	/** Start a select and hold input */
	void SelectHoldStarted(const FInputActionValue& Value);
	
	/** Select and hold input triggered */
	void SelectHoldTriggered(const FInputActionValue& Value);

	/** Select and hold input completed */
	void SelectHoldCompleted(const FInputActionValue& Value);

	/** Select click action */
	void SelectClick(const FInputActionValue& Value);

	/** Select Click Additive Action */
	void SelectClickAdditive(const FInputActionValue& Value);

	/** Select All Double Click Action */
	void SelectAllDoubleClick(const FInputActionValue& Value);

	/** Starts an interaction hold input */
	void InteractHoldStarted(const FInputActionValue& Value);

	/** Interaction hold input triggered */
	void InteractHoldTriggered(const FInputActionValue& Value);

	/** Interaction click input started */
	void InteractClick(const FInputActionValue& Value);

	/** Touch primary finger hold started */
	void TouchPrimaryHoldStarted(const FInputActionValue& Value);

	/** Touch primary finger hold triggered */
	void TouchPrimaryHoldTriggered(const FInputActionInstance& Instance);

	/** Touch primary finger hold completed */
	void TouchPrimaryHoldCompleted(const FInputActionValue& Value);

	/** Touch secondary finger triggered */
	void TouchSecondaryTriggered(const FInputActionValue& Value);

	/** Touch secondary finger completed */
	void TouchSecondaryCompleted(const FInputActionValue& Value);

	// commands

public:

	/** Attempt to select or deselect a unit near the given location. Supports additive selection modifier. */
	bool DoSelectCommand(const FVector& SelectLocation, bool bAdditiveSelection);

	/** Attempts to select all units on screen */
	void DoSelectAllUnitsOnScreenCommand();

	/** Deselects any selected units */
	void DoDeselectAllUnitsCommand();

	/** Toggles between selecting all units on screen and deselecting units */
	void DoToggleSelectAllUnitsCommand();

	/** Scrolls the camera based on a new screen coordinate */
	void DoCameraDragScrollCommand(const FVector2D& CurrentCursorPosition);

	/** Attempts to move all selected units to the given location */
	void DoMoveUnitsCommand(const FVector& GoalLocation);

	/** Applies a zoom change to the camera */
	void DoCameraModifyZoomCommand(float ZoomDelta);

	/** Resets the camera zoom to default */
	void DoCameraResetZoomCommand();

	/** Sets the camera zoom to a percentage between min and max zoom */
	void DoCameraSetZoomPercentageCommand(float Percentage);

protected:

	/** Sorts all controlled units based on their distance to the provided world location */
	AStrategyUnit* GetClosestSelectedUnitToLocation(FVector TargetLocation);

	/** Calculates and returns the current mouse location */
	FVector2D GetMouseLocationForPlayer();

	/** Attempts to get the world location under the cursor, returns true if successful */
	bool GetLocationUnderCursor(FVector& Location);

	/** Attempts to get the world location under the Touch 1 finger, returns true if successful */
	bool GetLocationUnderFinger(FVector& Location);

	/** Projects the current touch location into world space */
	FVector ProjectTouchPointToWorldSpace();

protected:

	/** Spawns the positive cursor effect */
	UFUNCTION(BlueprintImplementableEvent, Category="Cursor", meta = (DisplayName="Cursor Feedback"))
	void BP_CursorFeedback(FVector Location, bool bPositive);
};
