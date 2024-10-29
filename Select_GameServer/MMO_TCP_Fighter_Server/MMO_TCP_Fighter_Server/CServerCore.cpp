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
BOOL CServerCore::Start(CONST CHAR *openIp, CONST USHORT port, INT maxSessionCount)
{
	int retVal;
	int errVal;
	
	m_iMaxSessionCount = maxSessionCount;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"WSAStartup Error");

		// ���⼭ ���ܸ� ������ UnhandledExceptionFilter�� �ɷ��� ũ���� ������ ���� ��
		throw FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"WSAStartup success");

	// ���� ���� ����
	m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_listenSocket == INVALID_SOCKET)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"socket() Error : %d", errVal);
		throw FALSE;
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
		throw FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Bind OK # Port : %d", port);

	// listen()
	retVal = listen(m_listenSocket, SOMAXCONN);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"listen() Error : %d", errVal);
		throw FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Listen OK #");

	// �� ���ŷ �������� ��ȯ
	u_long on = 1;
	retVal = ioctlsocket(m_listenSocket, FIONBIO, &on);
	if (retVal == SOCKET_ERROR) // ���� �� SOCKET_ERROR ��ȯ
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"ioctlsocket() Error : %d", errVal);
		throw FALSE;
	}

	// ���� �ɼ� ����
	LINGER ling{ 1, 0 };
	retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"setsockopt() Error : %d", errVal);
		throw FALSE;
	}

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
		* Select �Լ��� ���� ����ŷ ���� ����
		* 63�� �� ������ ���� ȣ��
		* writeSet, readSet�� ����ŷ ������ Pending�� ��ȯ���� �ʰ� �� �۾��� �ִ� ��쿡
		* FD_ISSET���� ������ Ȯ���Ͽ� �۾� ����
*/
BOOL CServerCore::Select()
{
	int errVal;
	int retVal;

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
		for (; startIt != endIt; ++startIt)
		{
			CSession *ses = startIt->second;

			if (time - ses->m_iPrevRecvTime > NETWORK_PACKET_RECV_TIMEOUT && ses->m_isVaild)
			{
				m_deqDeleteQueue.push_back(ses);
				ses->m_isVaild = FALSE;
				continue;
			}

			FD_SET(ses->m_ClientSocket, &m_readSet);
			if (ses->m_SendBuffer.GetUseSize() > 0)
				FD_SET(ses->m_ClientSocket, &m_writeSet);
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
				Recv(ses);
			}

			if (FD_ISSET(ses->m_ClientSocket, &m_writeSet))
			{
				// send
				Send(ses);
			}
		}

		// iterator�� 63�� �о���
		for (size_t i = 0; i < 63; i++)
		{
			++prevStartIt;
			++endIt;
			if (endIt == m_mapSessions.end())
				break;
		}

		// startIt�� ���� �����Ͽ��ٸ� ��
		if (startIt == m_mapSessions.end())
			break;
	}

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
	CServerCore::ReleaseSession
		* disconnectQueue�� �ִ� ���� ����
*/
BOOL CServerCore::ReleaseSession()
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
/*
	CServerCore::Disconnect
		* ������ �ڵ忡�� ���� ���⸦ ��û�ϱ� ���� ����
*/

VOID CServerCore::Disconnect(UINT64 sessionId)
{
	CSession *pSession = m_mapSessions[sessionId];
	if (pSession->m_isVaild)
	{
		m_deqDeleteQueue.push_back(pSession);
		pSession->m_isVaild = FALSE;
	}
}

/*
	CServerCore::SendPacket
		* ������ �ڵ忡�� sessionId�� ���� Send �Ұ��� Enqueue
		* �������� Send�� Select �Լ����� �̷���
*/
BOOL CServerCore::SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message)
{
	// ��� ����
	BYTE headerSize = (BYTE)(message->GetDataSize() - 1);
	PacketHeader header{ PACKET_IDENTIFIER, headerSize };
	message->EnqueueHeader((char *)&header, sizeof(PacketHeader));

	// SendBuffer�� ���
	CSession *pSession = m_mapSessions[sessionId];
	int fullSize = message->GetFullSize();
	int ret = pSession->m_SendBuffer.Enqueue(message->GetBufferPtr(), fullSize);
	if (ret == -1)
		return FALSE;

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

	InterlockedIncrement(&g_monitor.m_lAcceptTPS);

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
			if (errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"recv() Error : %d, SessionID : %d", errVal, pSession->m_iId);

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

	if (!Process(pSession))
	{
		if (pSession->m_isVaild)
		{
			m_deqDeleteQueue.push_back(pSession);
			pSession->m_isVaild = FALSE;
		}
	}

	// recv TPS ����
	InterlockedIncrement(&g_monitor.m_lRecvTPS);

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
			if (errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"send() Error : %d, SessionID : %d", errVal, pSession->m_iId);

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
	InterlockedIncrement(&g_monitor.m_lSendTPS);

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
		buffer->MoveWritePos(ret);


		if (!OnRecv(pSession->m_iId, buffer))
			return FALSE;

		delete buffer;
	}

	return TRUE;
}
