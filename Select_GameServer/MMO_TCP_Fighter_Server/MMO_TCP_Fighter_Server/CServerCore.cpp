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
BOOL CServerCore::Start(const CHAR *openIp, const USHORT port, INT maxSessionCount)
{
	int retVal;
	int errVal;
	
	m_iMaxSessionCount = maxSessionCount;

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"WSAStartup Error");

		// 여기서 예외를 던지면 UnhandledExceptionFilter에 걸려서 크래시 덤프가 남을 것
		throw 1;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"WSAStartup success");

	// 리슨 소켓 생성
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

	// 논 블로킹 소켓으로 전환
	u_long on = 1;
	retVal = ioctlsocket(m_listenSocket, FIONBIO, &on);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		g_Logger->WriteLog(L"NetworkCore", LOG_LEVEL::ERR, L"ioctlsocket() Error : %d", errVal);
		return false;
	}

	// 링거 옵션 설정
	LINGER ling;
	ling.l_linger = 0;
	ling.l_onoff = 1;
	setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));

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

		// 루프를 돌며 set에 session 등록
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

			// 63개 까지 등록했다면 루프 종료
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

		// 받을 신규 유저가 있는 경우
		if (FD_ISSET(m_listenSocket, &m_readSet))
		{
			Accept();
			// AcceptTPS 증가
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
		* 컨텐츠 코드에서 sessionId를 통해 Send 할것을 Enqueue
		* 실질적인 Send는 Select 함수에서 이뤄짐
*/
BOOL CServerCore::SendPacket(CONST UINT64 sessionId, CSerializableBuffer *message)
{
	// 헤더 세팅
	PacketHeader header{ PACKET_IDENTIFIER, message->GetDataSize() - 1 };
	message->EnqueueHeader((char *)&header, sizeof(PacketHeader));

	// SendBuffer에 등록
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
		* disconnectQueue에 있는 것을 삭제
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

	// 받은 패킷을 어떻게 처리할 것인지는 고민이 필요
	// 1. ProcessPacket 패킷 헤더 분석은 ServerCore에서 ProcessPacket의 Process 함수
	// 2. ServerCore의 OnRecv 콜백을 Process 함수의 내부에서 호출

	if (!Process(pSession))
	{
		if (pSession->m_isVaild)
		{
			// wprintf(L"Player %d disconnected!\n", s->m_Id);
			m_deqDeleteQueue.push_back(pSession);
			pSession->m_isVaild = FALSE;
		}
	}

	// recv TPS 증가

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
	
	// Send TPS 증가

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
