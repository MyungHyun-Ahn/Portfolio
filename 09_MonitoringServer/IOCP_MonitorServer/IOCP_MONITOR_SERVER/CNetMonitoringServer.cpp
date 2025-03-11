#include "pch.h"
#include "CNetServer.h"
#include "CommonProtocol.h"
#include "CNetMonitoringServer.h"
#include "CGenPacket.h"
#include "MonitoringServerSetting.h"

CNetMonitoringServer::CNetMonitoringServer()
{
	InitializeSRWLock(&m_umapLock);
}

bool CNetMonitoringServer::OnConnectionRequest(const WCHAR *ip, USHORT port) noexcept
{
	return true;
}

void CNetMonitoringServer::OnAccept(const UINT64 sessionID) noexcept
{
	AcquireSRWLockExclusive(&m_umapLock);
	m_umapMonitorClients.insert(std::make_pair(sessionID, FALSE));
	ReleaseSRWLockExclusive(&m_umapLock);
}

void CNetMonitoringServer::OnClientLeave(const UINT64 sessionID) noexcept
{
	AcquireSRWLockExclusive(&m_umapLock);
	m_umapMonitorClients.erase(sessionID);
	ReleaseSRWLockExclusive(&m_umapLock);
}

void CNetMonitoringServer::OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept
{
	BOOL isSuccessRecv = TRUE;
	WORD type;
	*message >> type;

	switch (type)
	{
	case en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
	{
		char loginSessionKey[33];
		message->Dequeue(loginSessionKey, 32);
		loginSessionKey[32] = '\0';

		if (MONITOR_SETTING::LOGIN_KEY != loginSessionKey)
		{
			isSuccessRecv = FALSE;
			CSerializableBuffer<FALSE> *resMsg = CGenPacket::makePacketResMonitorToolLogin(dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY);
			SendPacket(sessionID, resMsg);
			break;
		}

		// LOGIN_KEY 검사하고 성공하면 -> TRUE 성공 상태 전환
		AcquireSRWLockExclusive(&m_umapLock);
		auto it = m_umapMonitorClients.find(sessionID);
		if (it == m_umapMonitorClients.end())
		{
			ReleaseSRWLockExclusive(&m_umapLock);
			isSuccessRecv = FALSE;
			break;
		}
		it->second = TRUE;
		ReleaseSRWLockExclusive(&m_umapLock);

		CSerializableBuffer<FALSE> *resMsg = CGenPacket::makePacketResMonitorToolLogin(dfMONITOR_TOOL_LOGIN_OK);
		SendPacket(sessionID, resMsg);
	}
		break;

	default:
		isSuccessRecv = FALSE;
		break;
	}

	if (message->DecreaseRef() == 0)
		CSerializableBufferView<FALSE>::Free(message);

	if (!isSuccessRecv)
		Disconnect(sessionID);
}

void CNetMonitoringServer::OnError(int errorcode, WCHAR *errMsg) noexcept
{
}

// DB 저장 루틴 등록
void CNetMonitoringServer::RegisterContentTimerEvent() noexcept
{
}

void CNetMonitoringServer::SendMonitoringData(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp) noexcept
{
	CSerializableBuffer<FALSE> *updateMsg = CGenPacket::makePacketResMonitorToolDataUpdate(serverNo, dataType, dataValue, timeStamp);
	updateMsg->IncreaseRef();
	AcquireSRWLockExclusive(&m_umapLock);
	for (auto &[key, value] : m_umapMonitorClients)
	{
		if (value)
		{
			SendPacket(key, updateMsg);
		}
	}
	ReleaseSRWLockExclusive(&m_umapLock);
	if (updateMsg->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(updateMsg);

}
