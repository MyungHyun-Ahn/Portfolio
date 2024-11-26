#include "pch.h"
#include "DefineNetwork.h"
#include "CSession.h"
#include "CLanClients.h"
#include "CEchoClients.h"

int oversendCount;

void CEchoClients::OnConnect(const UINT64 sessionID)
{
	CClient *pClient = new CClient;
	pClient->m_iCreateTime = timeGetTime();
	pClient->m_iPrevSendTime = pClient->m_iCreateTime;

	USHORT index = GetIndex(sessionID);

	AcquireSRWLockExclusive(&m_srwLock);
	m_clientMap.insert(std::make_pair(sessionID, pClient));
	ReleaseSRWLockExclusive(&m_srwLock);


	for (int i = 0; i < oversendCount; i++)
	{
		CSmartPtr<CSerializableBuffer> buffer = CSmartPtr<CSerializableBuffer>::MakeShared();
		*buffer << pClient->m_iSendNum++;
		if (i + 1 == oversendCount)
			SendPacket(sessionID, buffer.GetRealPtr(), true);
		else
			SendPacket(sessionID, buffer.GetRealPtr(), false);
	}
}

void CEchoClients::OnRecv(const UINT64 sessionID, CSmartPtr<CSerializableBufferView> message)
{
	__int64 num;
	*message >> num;

	AcquireSRWLockExclusive(&m_srwLock);
	auto it = m_clientMap.find(sessionID);
	if (it == m_clientMap.end())
		__debugbreak();
	CClient *pClient = it->second;
	AcquireSRWLockExclusive(&pClient->m_srwLock);
	ReleaseSRWLockExclusive(&m_srwLock);
	ReleaseSRWLockExclusive(&pClient->m_srwLock);

	if (num == LOGIN_PACKET)
	{
		if (pClient->m_bLoginPacketRecv)
			InterlockedIncrement(&g_monitor.m_lLoginPacketDuplication); // 로그인 패킷 2번 온 것

		pClient->m_bLoginPacketRecv = true;
		return;
	}

	// PacketError
	if (num != pClient->m_iRecvNum)
		InterlockedIncrement(&g_monitor.m_lPacketError);

	pClient->m_iRecvNum++;

	if (pClient->m_iRecvNum == pClient->m_iSendNum)
	{
		pClient->m_iPrevSendTime = timeGetTime();

		for (int i = 0; i < oversendCount; i++)
		{
			CSmartPtr<CSerializableBuffer> buffer = CSmartPtr<CSerializableBuffer>::MakeShared();
			*buffer << pClient->m_iSendNum++;
			if (i + 1 == oversendCount)
				SendPacket(sessionID, buffer.GetRealPtr(), true);
			else
				SendPacket(sessionID, buffer.GetRealPtr(), false);
		}
	}
}

void CEchoClients::OnReleaseSession(const UINT64 sessionID)
{
	AcquireSRWLockExclusive(&m_srwLock);
	auto it = m_clientMap.find(sessionID);
	if (it == m_clientMap.end())
		__debugbreak();
	CClient *pClient = it->second;
	AcquireSRWLockExclusive(&pClient->m_srwLock);
	ReleaseSRWLockExclusive(&m_srwLock);
	ReleaseSRWLockExclusive(&pClient->m_srwLock);

	delete pClient;
}
