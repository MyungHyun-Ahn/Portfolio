// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Strategy/UI/StrategyTouchControls.h"
#include "Variant_Strategy/StrategyPlayerController.h"

void UStrategyTouchControls::SetPlayerController(AStrategyPlayerController* PC)
{
	PlayerController = PC;
}

void UStrategyTouchControls::ResetZoom()
{
	if (PlayerController)
	{
		PlayerController->DoCameraResetZoomCommand();

		BP_SetZoomPercentage(PlayerController->GetDefaultZoomPercentage());
	}
}

void UStrategyTouchControls::ToggleSelectAllUnits()
{
	if (PlayerController)
	{
		PlayerController->DoToggleSelectAllUnitsCommand();
	}
}

void UStrategyTouchControls::SetZoomPercentage(float Percentage)
{
	if (PlayerController)
	{
		PlayerController->DoCameraSetZoomPercentageCommand(Percentage);
	}
}