#include "pch.h"
#include "ContentEvent.h"
#include "CNetClient.h"
#include "LoginSetting.h"
#include "CLoginClient.h"
#include "CGenPacket.h"
#include "CommonProtocol.h"

namespace LOGIN_CLIENT
{
	CLoginClient *g_LoginClient;
}

void LOGIN_CLIENT::CUser::Init(const INT64 accountNo, const WCHAR *ID, const WCHAR *Nickname)
{
	m_iAccountNo = accountNo;
	wcsncpy_s(m_ID, ID, _TRUNCATE);
	wcsncpy_s(m_Nickname, Nickname, _TRUNCATE);
}

void LOGIN_CLIENT::CUser::Clear() noexcept
{
}


LOGIN_CLIENT::CLoginClient::CLoginClient() noexcept
{
	InitializeSRWLock(&m_srwLockUserMap);
}

void LOGIN_CLIENT::CLoginClient::LoginPacketReq(const UINT64 sessionID) noexcept
{
	std::unordered_map<UINT64, CUser *> &userMap = m_umapUsers;
	AcquireSRWLockExclusive(&m_srwLockUserMap);
	CUser *user = userMap[sessionID];
	ReleaseSRWLockExclusive(&m_srwLockUserMap);

	char key[64] = { 0, };

	auto *pBuffer = CGenPacket::makePacketReqLoginServer(user->m_iAccountNo, key);

	SendPacket(sessionID, pBuffer);
}

void LOGIN_CLIENT::CLoginClient::OnConnect(const UINT64 sessionID) noexcept
{
	CUser *newUser = CUser::Alloc();
	newUser->Init(LOGIN_SETTING::ACCOUNTNO, LOGIN_SETTING::ID.c_str(), LOGIN_SETTING::NICKNAME.c_str());
	AcquireSRWLockExclusive(&m_srwLockUserMap);
	m_umapUsers.insert(std::make_pair(sessionID, newUser));
	ReleaseSRWLockExclusive(&m_srwLockUserMap);

	// 로그인 대기 -> 몇초후 수행
	LoginEvent *event = new LoginEvent;
	event->SetEvent(sessionID);
	NETWORK_CLIENT::g_netClientMgr->ContentEventEnqueue(event);
}

void LOGIN_CLIENT::CLoginClient::OnDisconnect(const UINT64 sessionID) noexcept
{
	AcquireSRWLockExclusive(&m_srwLockUserMap);
	auto it = m_umapUsers.find(sessionID);
	if (it != m_umapUsers.end())
	{
		m_umapUsers.erase(sessionID);
		ReleaseSRWLockExclusive(&m_srwLockUserMap);
		return;
	}
	ReleaseSRWLockExclusive(&m_srwLockUserMap);
}

void LOGIN_CLIENT::CLoginClient::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	message->IncreaseRef();
	WORD type;
	*message >> type;
	if (type == en_PACKET_CS_LOGIN_RES_LOGIN)
	{
		INT64 accountNo;
		BYTE status;

		*message >> accountNo >> status;

		WCHAR ID[20];
		WCHAR Nickname[20];

		WCHAR GameServerIP[16];
		USHORT GameServerPort;

		WCHAR ChatServerIP[16];
		USHORT ChatServerPort;


		message->Dequeue((char *)ID, 20 * sizeof(WCHAR));
		message->Dequeue((char *)Nickname, 20 * sizeof(WCHAR));
		message->Dequeue((char *)GameServerIP, 16 * sizeof(WCHAR));
		*message >> GameServerPort;
		message->Dequeue((char *)ChatServerIP, 16 * sizeof(WCHAR));
		*message >> ChatServerPort;
	}


	if (message->DecreaseRef() == 0)
		CSerializableBufferView<FALSE>::Free(message);
}

void LOGIN_CLIENT::CLoginClient::RegisterContentTimerEvent() noexcept
{
}