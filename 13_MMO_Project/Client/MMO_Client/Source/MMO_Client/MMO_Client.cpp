// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMO_Client.h"
#include "Private/UI/SGameLogin.h"
#include "GameLoginSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMMO_Client);

class FMMOClientModule final : public FDefaultGameModuleImpl
{
	struct FPresentation
	{
		TSharedPtr<SGameLogin> Screen;
		TWeakObjectPtr<UGameViewportClient> Viewport;
		TWeakObjectPtr<APlayerController> Controller;
		bool bPreviousCursor = false;
	};

public:
	void StartupModule() override
	{
		if (IsRunningCommandlet() || IsRunningDedicatedServer() || FParse::Param(FCommandLine::Get(), TEXT("MMOChatLogin"))) return;
		FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FMMOClientModule::UpdateScreen);
		FWorldDelegates::OnWorldCleanup.AddRaw(this, &FMMOClientModule::OnWorldCleanup);
	}

	void ShutdownModule() override
	{
		FWorldDelegates::OnWorldPostActorTick.RemoveAll(this);
		FWorldDelegates::OnWorldCleanup.RemoveAll(this);
		for (auto& Item : Presentations) RemoveScreen(Item.Value);
		Presentations.Empty();
	}

private:
	static void RemoveScreen(FPresentation& View)
	{
		if (!View.Screen) return;
		if (View.Viewport.IsValid()) View.Viewport->RemoveViewportWidgetContent(View.Screen.ToSharedRef());
		if (View.Controller.IsValid() && FSlateApplication::IsInitialized())
		{
			View.Controller->SetInputMode(FInputModeGameOnly());
			View.Controller->bShowMouseCursor = View.bPreviousCursor;
		}
		View.Screen.Reset();
	}

	void UpdateScreen(UWorld* World, ELevelTick, float)
	{
		if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || !FSlateApplication::IsInitialized()) return;
		auto* Instance = World->GetGameInstance();
		if (!Instance) return;
		if (auto* View = Presentations.Find(World))
		{
			// 화면을 제거한 이후에도 월드가 정리될 때까지 항목을 유지해 재전환을 막는다.
			if (View->Screen && Instance->GetSubsystem<UGameLoginSubsystem>()->GetLoginPhase() == EGameLoginPhase::Accepted)
			{
				RemoveScreen(*View);
				UE_LOG(LogMMO_Client, Log, TEXT("event=game_screen_entered | 로그인 화면을 닫고 로컬 게임 입력을 복구했습니다."));
			}
			return;
		}
		auto* Controller = Instance->GetFirstLocalPlayerController(World);
		auto* Viewport = Instance->GetGameViewportClient();
		auto* Session = Instance->GetSubsystem<UGameLoginSubsystem>();
		if (!Controller || !Viewport || !Session || !Viewport->GetGameViewportWidget().IsValid()) return;
		auto& View = Presentations.Add(World);
		if (Session->GetLoginPhase() == EGameLoginPhase::Accepted) return;
		View.Controller = Controller;
		View.Viewport = Viewport;
		View.bPreviousCursor = Controller->bShowMouseCursor;
		View.Screen = SNew(SGameLogin).Session(Session);
		Viewport->AddViewportWidgetContent(View.Screen.ToSharedRef(), 1000);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(View.Screen->InitialFocus());
		Controller->SetInputMode(Mode);
		Controller->bShowMouseCursor = true;
		UE_LOG(LogMMO_Client, Log, TEXT("event=login_screen_shown | 로그인 화면을 표시했습니다."));
	}

	void OnWorldCleanup(UWorld* World, bool, bool)
	{
		if (auto* View = Presentations.Find(World)) RemoveScreen(*View);
		Presentations.Remove(World);
	}

private:
	TMap<TWeakObjectPtr<UWorld>, FPresentation> Presentations;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMMOClientModule, MMO_Client, "MMO_Client");
