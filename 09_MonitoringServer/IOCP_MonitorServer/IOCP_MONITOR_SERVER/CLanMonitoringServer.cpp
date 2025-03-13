#include "pch.h"
#include "CommonProtocol.h"
#include "CLanServer.h"
#include "CLanMonitoringServer.h"
#include "CNetServer.h"
#include "CNetMonitoringServer.h"
#include "MonitoringServerSetting.h"
#include "ContentEvent.h"

CLanMonitoringServer::CLanMonitoringServer()
{
	InitializeSRWLock(&m_umapLock);
}

bool CLanMonitoringServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

void CLanMonitoringServer::OnAccept(const UINT64 sessionID) noexcept
{
	AcquireSRWLockExclusive(&m_umapLock);
	m_umapServers.insert(std::make_pair(sessionID, 0));
	ReleaseSRWLockExclusive(&m_umapLock);
}

void CLanMonitoringServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	AcquireSRWLockExclusive(&m_umapLock);
	m_umapServers.erase(sessionID);
	ReleaseSRWLockExclusive(&m_umapLock);
}

void CLanMonitoringServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<TRUE> *message) noexcept
{
	BOOL isSuccessRecv = TRUE;
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_SS_MONITOR_LOGIN:
	{
		int serverNo;
		*message >> serverNo;

		if (serverNo != MONITOR_SETTING::CHAT_SERVER_NO && serverNo != MONITOR_SETTING::GAME_SERVER_NO && serverNo != MONITOR_SETTING::LOGIN_SERVER_NO)
		{
			isSuccessRecv = FALSE;
			break;
		}

		AcquireSRWLockExclusive(&m_umapLock);
		auto it = m_umapServers.find(sessionID);
		if (it == m_umapServers.end())
		{
			ReleaseSRWLockExclusive(&m_umapLock);
			isSuccessRecv = FALSE;
			break;
		}
		// 서버 No 설정
		it->second = serverNo;
		ReleaseSRWLockExclusive(&m_umapLock);

		// 응답은 안보냄 Lan 이니깐
	}
	break;

	case en_PACKET_SS_MONITOR_DATA_UPDATE:
	{
		INT serverNo;
		AcquireSRWLockExclusive(&m_umapLock);
		auto it = m_umapServers.find(sessionID);
		if (it == m_umapServers.end())
		{
			ReleaseSRWLockExclusive(&m_umapLock);
			isSuccessRecv = FALSE;
			break;
		}

		serverNo = it->second;
		ReleaseSRWLockExclusive(&m_umapLock);

		BYTE dataType;
		INT dataValue;
		INT timeStamp;
		*message >> dataType >> dataValue >> timeStamp;

		m_arrMonitorInfo[dataType].serverNo = serverNo;
		m_arrMonitorInfo[dataType].dataValue = dataValue;
		m_arrMonitorInfo[dataType].timeStamp = timeStamp;	

		((CNetMonitoringServer *)NET_SERVER::g_NetServer)->SendMonitoringData(serverNo, dataType, dataValue, timeStamp);
	}
	break;

	default:
		isSuccessRecv = FALSE;
		break;
	}

	if (message->DecreaseRef() == 0)
		CSerializableBufferView<TRUE>::Free(message);

	if (!isSuccessRecv)
		Disconnect(sessionID);
}

void CLanMonitoringServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

void CLanMonitoringServer::RegisterContentTimerEvent() noexcept
{
	DBTimerEvent *pDBEvent = new DBTimerEvent;
	pDBEvent->SetEvent();
	RegisterTimerEvent(pDBEvent);
}
