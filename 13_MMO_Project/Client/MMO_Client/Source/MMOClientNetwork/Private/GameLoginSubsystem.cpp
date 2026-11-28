#include "GameLoginSubsystem.h"
#include "GameLoginFacade.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

bool UGameLoginSubsystem::SubmitLoginId(const FString& Id)
{
	check(IsInGameThread());
	if (Id.IsEmpty() || Id.Len() > 20)
	{
		return false;
	}
	for (int32 Index = 0; Index < Id.Len(); ++Index)
	{
		if (Id[Index] < TEXT('0') || Id[Index] > TEXT('9'))
		{
			return false;
		}
	}
	if (!Session)
	{
		FString PortText;
		if (!FParse::Value(FCommandLine::Get(), TEXT("-MMOGamePort="), PortText) && GConfig)
		{
			GConfig->GetString(TEXT("GameLogin"), TEXT("Port"), PortText, GGameIni);
		}
		int32 Port = 0;
		if (PortText.IsEmpty() || PortText.Len() > 5 || !PortText.IsNumeric() || !LexTryParseString(Port, *PortText) || Port <= 0 || Port > MAX_uint16)
		{
			bStartFailed = true;
			return false;
		}
		Session = GameLoginBridge::Create(static_cast<uint16>(Port));
		bStartFailed = !Session;
		if (!Session)
		{
			return false;
		}
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGameLoginSubsystem::TickSession));
	}
	return Session->Submit(*Id);
}

bool UGameLoginSubsystem::TickSession(float)
{
	check(IsInGameThread());
	Session->Tick();
	return true;
}

EGameLoginPhase UGameLoginSubsystem::GetLoginPhase() const
{
	if (!Session)
	{
		return bStartFailed ? EGameLoginPhase::Failed : EGameLoginPhase::Idle;
	}
	using GameLoginBridge::Phase;
	switch (Session->GetPhase())
	{
	case Phase::Idle: return EGameLoginPhase::Idle;
	case Phase::Connecting: return EGameLoginPhase::Connecting;
	case Phase::Waiting: return EGameLoginPhase::Waiting;
	case Phase::Rejected: return EGameLoginPhase::Rejected;
	case Phase::Accepted: return EGameLoginPhase::Accepted;
	default: return EGameLoginPhase::Failed;
	}
}

void UGameLoginSubsystem::Deinitialize()
{
	FTSTicker::RemoveTicker(TickHandle);
	TickHandle.Reset();
	if (Session)
	{
		GameLoginBridge::Destroy(Session);
		Session = nullptr;
	}
	Super::Deinitialize();
}
