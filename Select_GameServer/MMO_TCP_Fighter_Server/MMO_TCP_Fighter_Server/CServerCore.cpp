#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CServerCore.h"
#include "CProcessPacket.h"

/*
	CServerCore::Start
		* �Է� ���� �Ű������� ������ ������ �ʱ�ȭ
		* listen�� ���� �ɼǱ��� ����
		* maxClientCount���� ������ �������� ������ �ź��ϰ� ���� ��
*/
BOOL CServerCore::Start(const CHAR *openIp, const USHORT port, INT maxSessionCount)
{
	int retVal;
	int errVal;
	
	m_iMaxSessionCount = maxSessionCount;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"WSAStartup Error");

		// ���⼭ ���ܸ� ������ UnhandledExceptionFilter�� �ɷ��� ũ���� ������ ���� ��
		throw 1;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"WSAStartup success");

	// ���� ���� ����
	m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_listenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"socket() Error : %d", errVal);
		return FALSE;
	}

	// bind
	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	InetPtonA(AF_INET, openIp, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);
	retVal = bind(m_listenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"bind() Error : %d", errVal);
		return false;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Bind OK # Port : %d", port);

	// listen()
	retVal = listen(m_listenSocket, SOMAXCONN);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"listen() Error : %d", errVal);
		return false;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Listen OK #");

	// �� ����ŷ �������� ��ȯ
	u_long on = 1;
	retVal = ioctlsocket(m_listenSocket, FIONBIO, &on);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"ioctlsocket() Error : %d", errVal);
		return false;
	}

	// ���� �ɼ� ����
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));

	return TRUE;
}

/*
	CServerCore::Stop
		* ������ ������ �����ϰ� �����
		* ���μ����� �������� �ʰ� ��� ������� ��ü�� ó�� ���·� �ǵ���
		* TODO
*/
VOID CServerCore::Stop()
{

}


/*
	CServerCore::Select
		* Select �Լ��� ���� ������ŷ ���� ����
		* 63�� �� ������ ���� ȣ��
*/
BOOL CServerCore::Select()
{
	int errVal;
	int retVal;

	int loopCount = 0;

	auto startIt = m_mapSessions.begin();
	auto prevStartIt = startIt;
	auto endIt = startIt;

	for (size_t i = 0; i < 63; i++)
	{
		++endIt;
		if (endIt == m_mapSessions.end())
			break;
	}

	INT time = timeGetTime();

	while (TRUE)
	{
		FD_ZERO(&m_readSet);
		FD_ZERO(&m_writeSet);

		FD_SET(m_listenSocket, &m_readSet);
		
		startIt = prevStartIt;

		// ������ ���� set�� session ���
		int sesCount = 0;
		for (; startIt != m_mapSessions.end(); ++startIt)
		{
			++sesCount;

			CSession *ses = startIt->second;

			if (time - ses->m_iPrevRecvTime > NETWORK_PACKET_RECV_TIMEOUT && ses->m_isVaild)
			{
				m_deqDeleteQueue.push_back(ses);
				ses->m_isVaild = FALSE;
				continue;
			}

			FD_SET(ses->m_ClientSocket, &m_readSet);

			// 63�� ���� ����ߴٸ� ���� ����
			if (sesCount == 63)
				break;
		}

		// select()
		timeval t{ 0,0 };
		retVal = select(0, &m_readSet, &m_writeSet, NULL, &t);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"select() Error : %d", errVal);
			throw 1;
		}

		// ���� �ű� ������ �ִ� ���
		if (FD_ISSET(m_listenSocket, &m_readSet))
		{
			Accept();
			// AcceptTPS ����
		}

		startIt = prevStartIt;

		for (; startIt != endIt; ++startIt)
		{
			if (startIt == m_mapSessions.end())
				break;

			// ������ �ִ��� üũ
			CSession *ses = startIt->second;
			if (FD_ISSET(ses->m_ClientSocket, &m_readSet))
			{
				// recv
				if (!Recv(ses))
					continue;
			}

			if (FD_ISSET(ses->m_ClientSocket, &m_writeSet))
			{
				// send
				if (!Send(ses))
					continue;
			}
		}

		for (size_t i = 0; i < 63; i++)
		{
			++prevStartIt;
			++endIt;
			if (endIt == m_mapSessions.end())
				break;
		}

		if (startIt == m_mapSessions.end())
			break;

		loopCount++;
	}

	return TRUE;
}


/*
	CServerCore::SendPacket
		* ������ �ڵ忡�� sessionId�� ���� Send �Ұ��� Enqueue
		* �������� Send�� Select �Լ����� �̷���
*/
BOOL CServerCore::SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message)
{
	// ��� ����
	PacketHeader header{ PACKET_IDENTIFIER, message->GetDataSize() - 1 };
	message->EnqueueHeader((char *)&header, sizeof(PacketHeader));

	// SendBuffer�� ���
	CSession *pSession = m_mapSessions[sessionId];
	int ret = pSession->m_SendBuffer.Enqueue(message->GetBufferPtr(), message->GetFullSize());
	if (ret == -1)
		return FALSE;

	return TRUE;
}

VOID CServerCore::TimeoutCheck()
{
	INT time = timeGetTime();
	for (auto &ses : m_mapSessions)
	{
		if (time - ses.second->m_iPrevRecvTime > NETWORK_PACKET_RECV_TIMEOUT)
		{
			if (ses.second->m_isVaild)
			{
				m_deqDeleteQueue.push_back(ses.second);
				ses.second->m_isVaild = FALSE;
			}
		}
	}
}

/*
	CServerCore::Disconnect
		* disconnectQueue�� �ִ� ���� ����
*/
BOOL CServerCore::Disconnect()
{
	for (CSession *pSession : m_deqDeleteQueue)
	{
		OnClientLeave(pSession->m_iId);
		m_mapSessions.erase(pSession->m_iId);
		closesocket(pSession->m_ClientSocket);
		delete pSession;
	}

	m_deqDeleteQueue.clear();
	return TRUE;
}

BOOL CServerCore::Accept()
{
	int errVal;

	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(clientAddr);
	SOCKET clientSock = accept(m_listenSocket, (SOCKADDR *)&clientAddr, &addrLen);
	if (clientSock == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"accept() Error : %d", errVal);
		return FALSE;
	}

	// ���� ����
	++m_iCurSessionIDValue;
	UINT64 id = m_iCurSessionIDValue;

	CSession *newSession = new CSession;
	newSession->m_iId = id;
	newSession->m_ClientSocket = clientSock;
	newSession->m_iPrevRecvTime = timeGetTime();

	InetNtop(AF_INET, &clientAddr.sin_addr, newSession->m_szClientIP, 16);
	m_mapSessions.insert(std::make_pair(id, newSession));

	OnAccept(id);

	return 0;
}

BOOL CServerCore::Recv(CSession *pSession)
{
	int retVal;
	int errVal;
	int directSize = pSession->m_RecvBuffer.DirectEnqueueSize();
	retVal = recv(pSession->m_ClientSocket, pSession->m_RecvBuffer.GetRearPtr(), directSize, 0);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		if (errVal != WSAEWOULDBLOCK)
		{
			// WOULDBLOCK�� �ƴ� ������ deleteQueue�� Enqueue
			if (pSession->m_isVaild)
			{
				m_deqDeleteQueue.push_back(pSession);
				pSession->m_isVaild = FALSE;
			}

			return FALSE;
		}
	}

	// ���� ����
	if (retVal == 0)
	{
		if (pSession->m_isVaild)
		{
			m_deqDeleteQueue.push_back(pSession);
			pSession->m_isVaild = FALSE;
		}

		return FALSE;
	}

	pSession->m_iPrevRecvTime = timeGetTime();
	pSession->m_RecvBuffer.MoveRear(retVal);

	// ���� ��Ŷ�� ��� ó���� �������� ������ �ʿ�
	// 1. ProcessPacket ��Ŷ ��� �м��� ServerCore���� ProcessPacket�� Process �Լ�
	// 2. ServerCore�� OnRecv �ݹ��� Process �Լ��� ���ο��� ȣ��

	if (!Process(pSession))
	{
		if (pSession->m_isVaild)
		{
			// wprintf(L"Player %d disconnected!\n", s->m_Id);
			m_deqDeleteQueue.push_back(pSession);
			pSession->m_isVaild = FALSE;
		}
	}

	// recv TPS ����

	return TRUE;
}

BOOL CServerCore::Send(CSession *pSession)
{
	int retVal;
	int errVal;
	int directSize = pSession->m_SendBuffer.DirectDequeueSize();
	retVal = send(pSession->m_ClientSocket, pSession->m_SendBuffer.GetFrontPtr(), directSize, 0);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		if (errVal != WSAEWOULDBLOCK)
		{
			if (pSession->m_isVaild)
			{
				m_deqDeleteQueue.push_back(pSession);
				pSession->m_isVaild = FALSE;
			}

			return FALSE;
		}
	}

	if (directSize == retVal)
	{
		pSession->m_SendBuffer.Clear();
	}
	else
	{
		pSession->m_SendBuffer.MoveFront(retVal);
	}
	
	// Send TPS ����

	return TRUE;
}

BOOL CServerCore::Process(CSession *pSession)
{
	int size = pSession->m_RecvBuffer.GetUseSize();

	while (size > 0)
	{
		PacketHeader header;
		int ret = pSession->m_RecvBuffer.Peek((char *)&header, sizeof(PacketHeader));
		// PacketHeader + PacketType + size
		if (pSession->m_RecvBuffer.GetUseSize() < sizeof(PacketHeader) + 1 + header.bySize)
			break;

		pSession->m_RecvBuffer.MoveFront(ret);

		CSerializableBuffer *buffer = new CSerializableBuffer;
		ret = pSession->m_RecvBuffer.Dequeue(buffer->GetContentBufferPtr(), header.bySize + 1);

		if (OnRecv(pSession->m_iId, buffer))
			return FALSE;

		delete buffer;
	}

	return TRUE;
}