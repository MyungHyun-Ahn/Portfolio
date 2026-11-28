#include "SGameLogin.h"
#include "GameLoginSubsystem.h"
#include "Misc/Paths.h"
#include "Algo/AllOf.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Brass = FLinearColor(FColor(226, 185, 133));
	const FLinearColor Ivory = FLinearColor(FColor(250, 237, 213));

	TSharedRef<STextBlock> Label(const TCHAR* Text, int32 Size, const FLinearColor& Color, FName Face = TEXT("Regular"))
	{
		return SNew(STextBlock).Text(FText::FromString(Text)).ColorAndOpacity(Color).Font(FCoreStyle::GetDefaultFontStyle(Face, Size));
	}
}

void SGameLogin::Construct(const FArguments& Args)
{
	Session = Args._Session;
	const FString ImagePath = FPaths::ProjectContentDir() / TEXT("Slate/Login/AshenGate.png");
	Backdrop = MakeShared<FSlateDynamicImageBrush>(FName(*ImagePath), FVector2D(1440, 810));
	auto Form = SNew(SVerticalBox);
	Form->AddSlot().AutoHeight()[Label(TEXT("I    /    THE GATEHOUSE"), 11, Brass, TEXT("Bold"))];
	Form->AddSlot().AutoHeight().Padding(0, 8, 0, 24)[Label(TEXT("Ashen Gate"), 36, Ivory, TEXT("Light"))];
	Form->AddSlot().AutoHeight().Padding(0, 0, 0, 7)[Label(TEXT("ACCOUNT ID"), 10, Brass, TEXT("Bold"))];
	Form->AddSlot().AutoHeight()[SAssignNew(IdField, SEditableTextBox)
		.HintText(INVTEXT("1–20 digits")).Padding(FMargin(10, 8)).IsEnabled(this, &SGameLogin::CanSubmit)
		.OnTextChanged(this, &SGameLogin::OnIdChanged).OnTextCommitted(this, &SGameLogin::OnCommitted)];
	Form->AddSlot().AutoHeight().Padding(0, 16, 0, 7)[Label(TEXT("PASSWORD   /   OPTIONAL"), 10, Brass, TEXT("Bold"))];
	// 시험 비밀번호는 마스킹만 하고 위젯 바깥으로 읽지 않는다.
	Form->AddSlot().AutoHeight()[SNew(SEditableTextBox).IsPassword(true).HintText(INVTEXT("Leave blank"))
		.Padding(FMargin(10, 8)).IsEnabled(this, &SGameLogin::CanSubmit)];
	Form->AddSlot().AutoHeight().Padding(0, 22, 0, 9)[SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
		.ButtonColorAndOpacity(FLinearColor(FColor(156, 101, 62))).IsEnabled(this, &SGameLogin::CanSubmit)
		.OnClicked(this, &SGameLogin::Submit)[Label(TEXT("LOG IN"), 13, Ivory, TEXT("Bold"))]];
	Form->AddSlot().AutoHeight()[SNew(STextBlock).Text(this, &SGameLogin::Status).ColorAndOpacity(Ivory).WrapTextAt(290)];

	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	ChildSlot[SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(Backdrop.Get())]
		+ SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
		[SNew(SBox).WidthOverride(1152).HeightOverride(648)
		[SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0, 0, 76, 0)
			[SNew(SBox).WidthOverride(350)[SNew(SBorder).BorderImage(White).BorderBackgroundColor(Brass).Padding(1)
			[SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(FColor(29, 28, 39))).Padding(30)[Form]]]]
		]]]];
}

TSharedRef<SWidget> SGameLogin::InitialFocus() const
{
	return IdField.ToSharedRef();
}

bool SGameLogin::CanSubmit() const
{
	if (!Session.IsValid()) return false;
	const auto Phase = Session->GetLoginPhase();
	return Phase == EGameLoginPhase::Idle || Phase == EGameLoginPhase::Rejected || Phase == EGameLoginPhase::Failed;
}

FReply SGameLogin::Submit()
{
	if (!CanSubmit()) return FReply::Handled();
	const FString Id = IdField->GetText().ToString();
	if (Id.IsEmpty() || Id.Len() > 20 || !Algo::AllOf(Id, [](TCHAR C) { return C >= TEXT('0') && C <= TEXT('9'); }))
	{
		InputError = INVTEXT("Enter 1–20 digits (0–9) for your account ID.");
	}
	else if (!Session->SubmitLoginId(Id))
	{
		InputError = INVTEXT("Could not start login. Please try again.");
	}
	else
	{
		InputError = FText::GetEmpty();
	}
	return FReply::Handled();
}

void SGameLogin::OnIdChanged(const FText& Text)
{
	InputError = FText::GetEmpty();
}

void SGameLogin::OnCommitted(const FText& Text, ETextCommit::Type Commit)
{
	if (Commit == ETextCommit::OnEnter) Submit();
}

FText SGameLogin::Status() const
{
	if (!Session.IsValid()) return INVTEXT("Login service unavailable.");
	if (!InputError.IsEmpty()) return InputError;
	switch (Session->GetLoginPhase())
	{
	case EGameLoginPhase::Connecting: return INVTEXT("Connecting…");
	case EGameLoginPhase::Waiting: return INVTEXT("Waiting for the gatekeeper…");
	case EGameLoginPhase::Rejected: return INVTEXT("Login rejected. Check your ID and try again.");
	case EGameLoginPhase::Failed: return INVTEXT("No response from the gate. Please try again.");
	default: return FText::GetEmpty();
	}
}
