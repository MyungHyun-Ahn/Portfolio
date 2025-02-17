#include "pch.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "DBSetting.h"
#include "CDBConnector.h"
#include "CUser.h"
#include "LoginServerSetting.h"
#include "CLoginServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "ContentEvent.h"

CLoginServer::CLoginServer() noexcept
{
	m_pProcessPacket = new CLoginProcessPacket;
	m_pProcessPacket->SetLoginServer(this);

	InitializeSRWLock(&m_userMapLock);
}

void CLoginServer::HeartBeat() noexcept
{
	AcquireSRWLockShared(&m_userMapLock);
	DWORD nowTime = timeGetTime();
	for (auto it = m_umapUser.begin(); it != m_umapUser.end(); ++it)
	{
		DWORD dTime = nowTime - it->second.m_dwPrevRecvTime;
		if (dTime > LOGIN_SERVER_SETTING::LOGIN_TIME_OUT)
			Disconnect(it->first, TRUE); // ReleaseSession을 PQCS로 우회
	}
	ReleaseSRWLockShared(&m_userMapLock);
}

bool CLoginServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	// 화이트 IP 체크
	return true;
}

void CLoginServer::OnAccept(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);
	CUser newUser = { timeGetTime() };
	AcquireSRWLockExclusive(&m_userMapLock);
	m_umapUser.insert(std::make_pair(sessionID, newUser));
	ReleaseSRWLockExclusive(&m_userMapLock);
}

void CLoginServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);

	AcquireSRWLockExclusive(&m_userMapLock);
	auto it = m_umapUser.find(sessionID);
	if (it != m_umapUser.end())
	{
		m_umapUser.erase(sessionID);
		ReleaseSRWLockExclusive(&m_userMapLock);
		return;
	}
	ReleaseSRWLockExclusive(&m_userMapLock);

	__debugbreak();
}

void CLoginServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	InterlockedIncrement(&g_monitor.m_lUpdateTPS);

	message->IncreaseRef();
	WORD type;
	*message >> type;
	if (!m_pProcessPacket->ConsumPacket((en_PACKET_TYPE)type, message->GetSessionID(), message))
	{
		Disconnect(message->GetSessionID());
	}

	if (message->DecreaseRef() == 0)
		CSerializableBufferView<FALSE>::Free(message);
}

void CLoginServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

void CLoginServer::RegisterContentTimerEvent() noexcept
{
}
