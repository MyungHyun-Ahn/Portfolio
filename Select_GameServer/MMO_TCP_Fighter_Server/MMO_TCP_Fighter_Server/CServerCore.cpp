#include "pch.h"
#include "Config.h"
#include "DefinePacket.h"
#include "CSession.h"
#include "CServerCore.h"
#include "CProcessPacket.h"

/*
	CServerCore::Start
		* 입력 받은 매개변수의 정보로 서버를 초기화
		* listen과 소켓 옵션까지 설정
		* maxClientCount까지 받으면 서버에서 연결을 거부하고 끊을 것
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

		// 여기서 예외를 던지면 UnhandledExceptionFilter에 걸려서 크래시 덤프가 남을 것
		throw FALSE;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"WSAStartup success");

	// 리슨 소켓 생성
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

	// 논 블로킹 소켓으로 전환
	u_long on = 1;
	retVal = ioctlsocket(m_listenSocket, FIONBIO, &on);
	if (retVal == SOCKET_ERROR) // 실패 시 SOCKET_ERROR 반환
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"ioctlsocket() Error : %d", errVal);
		throw FALSE;
	}

	// 링거 옵션 설정
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
		* 서버를 완전히 종료하고 재시작
		* 프로세스는 종료하지 않고 모든 사용중인 객체를 처음 상태로 되돌림
		* TODO
*/
VOID CServerCore::Stop()
{

}


/*
	CServerCore::Select
		* Select 함수를 통한 논블로킹 소켓 제어
		* 63개 씩 소켓을 끊어 호출
		* writeSet, readSet에 논블락킹 소켓이 Pending을 반환하지 않고 할 작업이 있는 경우에
		* FD_ISSET으로 소켓을 확인하여 작업 진행
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

		// 루프를 돌며 set에 session 등록
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

		// 받을 신규 유저가 있는 경우
		if (FD_ISSET(m_listenSocket, &m_readSet))
		{
			Accept();
		}

		startIt = prevStartIt;

		for (; startIt != endIt; ++startIt)
		{
			if (startIt == m_mapSessions.end())
				break;

			// 읽을게 있는지 체크
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

		// iterator를 63번 밀어줌
		for (size_t i = 0; i < 63; i++)
		{
			++prevStartIt;
			++endIt;
			if (endIt == m_mapSessions.end())
				break;
		}

		// startIt이 끝에 도달하였다면 끝
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
		* disconnectQueue에 있는 것을 삭제
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
		* 컨텐츠 코드에서 연결 끊기를 요청하기 위해 존재
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
		* 컨텐츠 코드에서 sessionId를 통해 Send 할것을 Enqueue
		* 실질적인 Send는 Select 함수에서 이뤄짐
*/
BOOL CServerCore::SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message)
{
	// 헤더 세팅
	BYTE headerSize = (BYTE)(message->GetDataSize() - 1);
	PacketHeader header{ PACKET_IDENTIFIER, headerSize };
	message->EnqueueHeader((char *)&header, sizeof(PacketHeader));

	// SendBuffer에 등록
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

	// 접속 수용
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

			// WOULDBLOCK이 아닌 에러는 deleteQueue에 Enqueue
			if (pSession->m_isVaild)
			{
				m_deqDeleteQueue.push_back(pSession);
				pSession->m_isVaild = FALSE;
			}

			return FALSE;
		}
	}

	// 정상 종료
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

	// recv TPS 증가
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
	
	// Send TPS 증가
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
