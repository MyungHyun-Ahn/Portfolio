#include "ChatLoginWorldSubsystem.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogChatLogin, Log, All);

bool UChatLoginWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && FParse::Param(FCommandLine::Get(), TEXT("MMOChatLogin"));
}

void UChatLoginWorldSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	FString PortText, AccountText;
	int32 Port = 0;
	const TCHAR* Cmd = FCommandLine::Get();
	if (!FParse::Value(Cmd, TEXT("-MMOChatPort="), PortText) || !PortText.IsNumeric() || !LexTryParseString(Port, *PortText)
		|| Port <= 0 || Port > MAX_uint16 || !FParse::Value(Cmd, TEXT("-MMOChatAccount="), AccountText)
		|| !AccountText.IsNumeric() || !LexTryParseString(Account, *AccountText) || Account <= 0)
	{
		UE_LOG(LogChatLogin, Error, TEXT("event=ue_login_invalid_config | 포트 또는 계정 설정이 잘못되었습니다."));
		return;
	}
	Session.Reset(Chatting::StartLocalLogin(static_cast<uint16>(Port), Account));
	if (!Session)
	{
		UE_LOG(LogChatLogin, Error, TEXT("event=ue_login_start_failed | 로컬 접속을 시작하지 못했습니다."));
	}
}

void UChatLoginWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	check(IsInGameThread());
	if (!Session)
	{
		return;
	}
	const Chatting::SLoginTickResult Result = Session->TickLogin();
	if (Result.m_submitted)
	{
		UE_LOG(LogChatLogin, Display, TEXT("event=ue_login_submitted account_no=%lld | 로그인 송신 요청을 접수했습니다."), Account);
	}
	if (Result.m_acceptedAccount != 0)
	{
		UE_LOG(LogChatLogin, Display, TEXT("event=ue_login_accepted account_no=%lld game_thread=%d | 로그인 응답을 확인했습니다."),
			Result.m_acceptedAccount, IsInGameThread() ? 1 : 0);
	}
	if (Result.m_disconnected)
	{
		UE_LOG(LogChatLogin, Log, TEXT("event=ue_login_disconnected error_code=%lu | 연결이 종료되었습니다."), Result.m_error);
		Session.Reset();
	}
}

TStatId UChatLoginWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UChatLoginWorldSubsystem, STATGROUP_Tickables);
}

void UChatLoginWorldSubsystem::Deinitialize()
{
	Session.Reset();
	Super::Deinitialize();
}
