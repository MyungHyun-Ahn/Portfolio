#include "SGameLogin.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Engine/GameInstance.h"
#include "GameLoginSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Layout/Children.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	void FindLoginControls(const TSharedRef<SWidget>& Root, FName Type, TArray<TSharedRef<SWidget>>& Found)
	{
		if (Root->GetType() == Type)
		{
			Found.Add(Root);
			return;
		}
		FChildren* Children = Root->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			FindLoginControls(Children->GetChildAt(Index), Type, Found);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameLoginFormTest, "MMO.Client.UI.LoginForm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FGameLoginFormTest::RunTest(const FString& Parameters)
{
	auto* Instance = NewObject<UGameInstance>();
	auto* Session = NewObject<UGameLoginSubsystem>(Instance);
	auto Screen = SNew(SGameLogin).Session(Session);
	TArray<TSharedRef<SWidget>> Fields, Buttons, Labels;
	FindLoginControls(Screen, TEXT("SEditableTextBox"), Fields);
	FindLoginControls(Screen, TEXT("SButton"), Buttons);
	FindLoginControls(Screen, TEXT("STextBlock"), Labels);
	if (!TestEqual(TEXT("Two inputs"), Fields.Num(), 2) || !TestEqual(TEXT("One login button"), Buttons.Num(), 1)) return false;
	auto Id = StaticCastSharedRef<SEditableTextBox>(Fields[0]);
	TestTrue(TEXT("Initial focus is the ID"), Screen->InitialFocus() == Fields[0]);
	TestTrue(TEXT("Password is masked"), StaticCastSharedRef<SEditableTextBox>(Fields[1])->IsPassword());
	// 유효하지 않은 입력만 사용한다. 실제 연결이나 시험용 승인 분기를 만들지 않는다.
	for (const TCHAR* Value : { TEXT(""), TEXT(" "), TEXT("123x"), TEXT("１２３"), TEXT("123456789012345678901") })
	{
		Id->SetText(FText::FromString(Value));
		StaticCastSharedRef<SButton>(Buttons[0])->SimulateClick();
		Screen->SlatePrepass(1.0f);
		TestTrue(TEXT("Rejected before starting a connection"), Session->GetLoginPhase() == EGameLoginPhase::Idle);
		const bool bShowsError = Labels.ContainsByPredicate([](const TSharedRef<SWidget>& Widget)
		{
			return StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString().Contains(TEXT("Enter 1–20 digits"));
		});
		TestTrue(TEXT("Inline validation explains the error"), bShowsError);
	}
	return true;
}
#endif
