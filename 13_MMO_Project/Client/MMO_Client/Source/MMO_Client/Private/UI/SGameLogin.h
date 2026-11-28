#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateDynamicImageBrush.h"

class SEditableTextBox;
class UGameLoginSubsystem;

class SGameLogin final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameLogin) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UGameLoginSubsystem>, Session)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	TSharedRef<SWidget> InitialFocus() const;

private:
	bool CanSubmit() const;
	FReply Submit();
	void OnIdChanged(const FText& Text);
	void OnCommitted(const FText& Text, ETextCommit::Type Commit);
	FText Status() const;

private:
	TWeakObjectPtr<UGameLoginSubsystem> Session;
	TSharedPtr<SEditableTextBox> IdField;
	TSharedPtr<FSlateDynamicImageBrush> Backdrop;
	FText InputError;
};
