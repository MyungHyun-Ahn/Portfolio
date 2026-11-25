#include "ServerNetworkLibPch.h"
#include "SNetworkCounters.h"
#include "PacketFraming.h"


namespace ServerNetworkLib
{
CServer *g_pServer = nullptr;

	// 분할 패킷을 이어 붙이고 완성된 패킷을 검증해서 큐에 넣는다.
	bool CSession::RecvCompleted(int size) noexcept
	{
		constexpr int kRecvHeaderBytes = sizeof(SPacketHeader);
		constexpr int kMaxFrameBytes = static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize);
		constexpr int kChecksumOffset = offsetof(SPacketHeader, checkSum);
		const BYTE packetCode = serverSetting::packetCode;
		const EPacketMode mode = g_pServer->GetPacketMode();
		if (size <= 0 || size > m_pRecvBuffer->GetCapacity())
		{
			if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
			m_pRecvBuffer = nullptr;

			if (m_pDelayedBuffer != nullptr)
			{
				CSerializableBufferView::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;
			}
			return false;
		}

		m_pRecvBuffer->MoveRear(size);

		// 이전 수신에서 잘린 패킷부터 처리한다.
		if (m_pDelayedBuffer != nullptr)
		{
			auto *pView = m_pDelayedBuffer;
			SPacketHeader header;
			if (pView->isReadHeaderSize() < kRecvHeaderBytes)
			{
				// 헤더가 또 나뉠 수 있으니 이번에 받은 만큼만 이어 붙인다.
				const int headerBytes = (std::min)(kRecvHeaderBytes - pView->isReadHeaderSize(), m_pRecvBuffer->GetUseSize());
				pView->writeDelayedHeader(m_pRecvBuffer->GetFrontPtr(), headerBytes);
				m_pRecvBuffer->MoveFront(headerBytes);
				if (pView->isReadHeaderSize() < kRecvHeaderBytes)
				{
					if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);
					m_pRecvBuffer = nullptr;
					return true;
				}

				pView->getDelayedHeader(reinterpret_cast<char *>(&header), kRecvHeaderBytes);
				if (header.code != packetCode || header.len > kMaxFrameBytes - kRecvHeaderBytes)
				{
					if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);
					m_pRecvBuffer = nullptr;

					if (m_pDelayedBuffer != nullptr)
					{
						CSerializableBufferView::Free(m_pDelayedBuffer);
						m_pDelayedBuffer = nullptr;
					}
					return false;
				}

				const int frameBytes = kRecvHeaderBytes + header.len;
				char *pBuffer = new (std::nothrow) char[frameBytes];
				if (pBuffer == nullptr)
				{
					if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);
					m_pRecvBuffer = nullptr;

					if (m_pDelayedBuffer != nullptr)
					{
						CSerializableBufferView::Free(m_pDelayedBuffer);
						m_pDelayedBuffer = nullptr;
					}
					return false;
				}
				pView->initStorage(pBuffer, frameBytes);
				pView->copy(reinterpret_cast<char *>(&header), kRecvHeaderBytes);
			}
			else
			{
				pView->getHeader(reinterpret_cast<char *>(&header), kRecvHeaderBytes);
			}

			const int bodyBytes = (std::min)(header.len - pView->GetDataSize(), m_pRecvBuffer->GetUseSize());
			pView->copy(m_pRecvBuffer->GetFrontPtr(), bodyBytes);
			m_pRecvBuffer->MoveFront(bodyBytes);
			if (pView->GetDataSize() < header.len)
			{
				if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);
				m_pRecvBuffer = nullptr;
				return true;
			}

			if (mode == EPacketMode::Net && !MHLib::security::CEncryption::DecodeAndValidateChecksum(
				pView->getBufferPtr() + kChecksumOffset, pView->getFullSize() - kChecksumOffset, header.randKey))
			{
				if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);
				m_pRecvBuffer = nullptr;

				if (m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;
				}
				return false;
			}

			m_pDelayedBuffer = nullptr;
			InterlockedIncrement(&g_networkCounters.m_recvTps);
			m_recvMsgQueue.Enqueue(pView);
		}

		for (int useSize = m_pRecvBuffer->GetUseSize(); useSize > 0; useSize = m_pRecvBuffer->GetUseSize())
		{
			if (useSize < kRecvHeaderBytes)
			{
				m_pDelayedBuffer = CSerializableBufferView::alloc();
				m_pDelayedBuffer->writeDelayedHeader(m_pRecvBuffer->GetFrontPtr(), useSize);
				m_pRecvBuffer->MoveFront(useSize);
				break;
			}

			SPacketHeader header;
			m_pRecvBuffer->Peek(reinterpret_cast<char *>(&header), kRecvHeaderBytes);
			if (header.code != packetCode || header.len > kMaxFrameBytes - kRecvHeaderBytes)
			{
				if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);
				m_pRecvBuffer = nullptr;

				if (m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;
				}
				return false;
			}

			const int frameBytes = kRecvHeaderBytes + header.len;
			auto *pView = CSerializableBufferView::alloc();
			if (useSize < frameBytes)
			{
				m_pDelayedBuffer = pView;
				char *pBuffer = new (std::nothrow) char[frameBytes];
				if (pBuffer == nullptr)
				{
					if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);
					m_pRecvBuffer = nullptr;

					if (m_pDelayedBuffer != nullptr)
					{
						CSerializableBufferView::Free(m_pDelayedBuffer);
						m_pDelayedBuffer = nullptr;
					}
					return false;
				}
				pView->initStorage(pBuffer, frameBytes);
				pView->copy(m_pRecvBuffer->GetFrontPtr(), useSize);
				m_pRecvBuffer->MoveFront(useSize);
				break;
			}

			// 한 페이지에서 끝난 패킷은 오프셋으로만 참조한다.
			const int offsetStart = m_pRecvBuffer->GetFrontOffset();
			pView->init(MHLib::utils::CSmartPtr<CRecvBuffer>(m_pRecvBuffer), offsetStart, offsetStart + frameBytes);
			m_pRecvBuffer->MoveFront(frameBytes);
			if (mode == EPacketMode::Net && !MHLib::security::CEncryption::DecodeAndValidateChecksum(
				pView->getBufferPtr() + kChecksumOffset, pView->getFullSize() - kChecksumOffset, header.randKey))
			{
				CSerializableBufferView::Free(pView);
				if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);
				m_pRecvBuffer = nullptr;

				if (m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;
				}
				return false;
			}

			InterlockedIncrement(&g_networkCounters.m_recvTps);
			m_recvMsgQueue.Enqueue(pView);
		}
		if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);
		m_pRecvBuffer = nullptr;
		return true;
	}

	// m_iSendCount를 믿고 할당 해제를 진행
	// * 논블락킹 I/O일 때만 Send를 요청한 데이터보다 덜 보내는 상황이 발생 가능
	// * 비동기 I/O는 무조건 전부 보내고 완료 통지가 도착함

	void CSession::SendCompleted(int size) noexcept
	{
		InterlockedAdd(&g_networkCounters.m_sendTps, m_sendCount);

		for (int count = 0; count < m_sendCount; count++)
		{
			if (m_pSendBuffers[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer::Free(m_pSendBuffers[count]);
			}
		}

		m_sendCount = 0;

		InterlockedExchange(&m_sendFlag, FALSE);

		PostSend();
	}

	bool CSession::PostRecv() noexcept
	{
		int errVal;
		int retVal;

		// 여기서는 수동 Ref 버전이 필요
		// * WSARecv에서 Ref 관리가 불가능

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			return FALSE;
		}

		// 완료 통지가 올 때까지 수신 페이지 참조를 유지한다.
		m_pRecvBuffer = CRecvBuffer::Alloc();
		m_pRecvBuffer->IncreaseRef();
		WSABUF wsaBuf{ static_cast<ULONG>(m_pRecvBuffer->GetCapacity()), m_pRecvBuffer->GetPQueuePtr() };

		ZeroMemory((m_pMyOverlappedStartAddr + 1), sizeof(OVERLAPPED));

		DWORD flag = 0;
		{
			// PROFILE_BEGIN(0, "WSARecv");
			retVal = WSARecv(m_sessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)(m_pMyOverlappedStartAddr + 1), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", m_sessionId, std::source_location::current(),
						L"event=recv_failed error_code=%d | 소켓 수신 요청에 실패했습니다.", errVal);
				if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);
				m_pRecvBuffer = nullptr;

				if (m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;
				}

				// 사실 여기선 0이 될 일이 없음
				// 반환값을 사용안해도 됨
				if (InterlockedDecrement(&m_ioCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				if (m_cancelIoCalled)
				{
					CancelIoEx((HANDLE)m_sessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	bool CSession::PostSend(bool isSendFlag) noexcept
	{
		int errVal;
		int retVal;
		int sendUseSize;

		if (!isSendFlag)
		{
			sendUseSize = m_lfSendBufferQueue.GetUseSize();
			if (sendUseSize <= 0)
			{
				return FALSE;
			}

			if ((InterlockedExchange(&m_sendFlag, TRUE) & ~(sm_enqueueFlag)) == TRUE)
			{
				return FALSE;
			}
		}

		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			LONG beforeSendFlag = InterlockedCompareExchange(&m_sendFlag, FALSE, TRUE);
			if ((beforeSendFlag & sm_enqueueFlag) == sm_enqueueFlag) // 최상위 비트 켜졌으면
			{
				PostSend(TRUE);
				return FALSE;
			}
			return FALSE;
		}

		InterlockedIncrement(&m_ioCountAndRelease);
		if ((m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			InterlockedDecrement(&m_ioCountAndRelease);
			InterlockedExchange(&m_sendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[kWsasendMaxBufferCount];

		m_sendCount = min(sendUseSize, kWsasendMaxBufferCount);
		// WSASEND_MAX_BUFFER_COUNT 만큼 1초에 몇번 보내는지 카운트
		// 이 수치가 높다면 더 늘릴 것
		if (m_sendCount == kWsasendMaxBufferCount)
			InterlockedIncrement(&g_networkCounters.m_maxSendCount);

		int count;
		for (count = 0; count < m_sendCount; count++)
		{
			CSerializableBuffer *pSBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pSBuffer))
			{
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", m_sessionId, std::source_location::current(),
					L"event=send_dequeue_failed | 송신 큐에서 패킷을 꺼내지 못했습니다.");
				// 말도 안되는 상황
				MHLib::diagnostics::CCrashDump::Crash();
			}

			PreparePacket(*pSBuffer, serverSetting::packetCode, g_pServer->GetPacketMode());

			wsaBuf[count].buf = pSBuffer->GetBufferPtr();
			wsaBuf[count].len = pSBuffer->GetFullSize();

			m_pSendBuffers[count] = pSBuffer;
		}

		InterlockedAnd(&m_sendFlag, TRUE);

		ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));
		InterlockedIncrement64(&g_networkCounters.m_sendCallCount);
		{
			PROFILE_BEGIN(0, "WSASend");
			retVal = WSASend(m_sessionSocket, wsaBuf, m_sendCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", m_sessionId, std::source_location::current(),
						L"event=send_failed error_code=%d | 소켓 송신 요청에 실패했습니다.", errVal);

				// 사실 여기선 0이 될 일이 없음
				// 반환값을 사용안해도 됨
				if (InterlockedDecrement(&m_ioCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				InterlockedIncrement64(&g_networkCounters.m_sendPendingCount);
				if (m_cancelIoCalled)
				{
					CancelIoEx((HANDLE)m_sessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	void CSession::Clear() noexcept
	{
		for (int count = 0; count < m_sendCount; count++)
		{
			if (m_pSendBuffers[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer::Free(m_pSendBuffers[count]);
			}
		}

		m_sendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer *pBuffer;
			// 못꺼낸 것
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", m_sessionId, std::source_location::current(),
					L"event=send_dequeue_failed | 송신 큐에서 패킷을 꺼내지 못했습니다.");
				// 말도 안되는 상황
				CCrashDump::Crash();
			}

			// RefCount를 낮추고 0이라면 보낸 거 삭제
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", m_sessionId, std::source_location::current(),
				L"event=send_queue_not_empty | 세션 정리 후에도 송신 큐에 패킷이 남아 있습니다.");
		}

		useBufferSize = m_recvMsgQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBufferView *pBuffer;
			if (!m_recvMsgQueue.Dequeue(&pBuffer))
			{
				CCrashDump::Crash();
			}

			CSerializableBufferView::Free(pBuffer);
		}

		m_sessionSocket = INVALID_SOCKET;
		m_sessionId = 0;
		m_pCurrentContent = nullptr;
		if (m_pRecvBuffer != nullptr && m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);
		m_pRecvBuffer = nullptr;

		if (m_pDelayedBuffer != nullptr)
		{
			CSerializableBufferView::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
		}
		InterlockedExchange(&m_sendFlag, FALSE);
	}

	unsigned int NetWorkerThreadFunc(LPVOID pParam) noexcept
	{
		g_pLogger->RegisterLogProducer();
		return g_pServer->WorkerThread();
	}

	BOOL CServer::Start(const CHAR *pOpenIp, const USHORT port, EPacketMode packetMode) noexcept
	{
		if (packetMode != EPacketMode::Net && packetMode != EPacketMode::Lan)
			return FALSE;

		m_packetMode = packetMode;
		int retVal;
		int errVal;
		WSAData wsaData;

		m_pSessions = new CSession * [serverSetting::maxSessionCount];

		ZeroMemory((char *)m_pSessions, sizeof(CSession *) * serverSetting::maxSessionCount);

		m_pAcceptExSessions = new CSession * [serverSetting::acceptexCount];

		// 디스커넥트 스택 채우기
		USHORT val = serverSetting::maxSessionCount - 1;
		for (int i = 0; i < serverSetting::maxSessionCount; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=winsock_start_failed error_code=%d | Winsock 초기화에 실패했습니다.", errVal);
			return FALSE;
		}

		m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_listenSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=listen_socket_create_failed error_code=%d | 수신 대기 소켓을 생성하지 못했습니다.", errVal);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, pOpenIp, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		retVal = bind(m_listenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=bind_failed error_code=%d | 서버 소켓 바인딩에 실패했습니다.", errVal);
			return FALSE;
		}

		// 송신 버퍼를 0으로 만들어서 실직적인 I/O를 우리의 버퍼를 이용하도록 만듬
		if (serverSetting::useZeroCopy)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=send_buffer_set_failed error_code=%d | 소켓 송신 버퍼 설정에 실패했습니다.", errVal);
				return FALSE;
			}
		}

		// LINGER option 설정
		LINGER ling{ 1, 0 };
		retVal = setsockopt(m_listenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=linger_set_failed error_code=%d | 소켓 종료 대기 옵션 설정에 실패했습니다.", errVal);
			return FALSE;
		}

		// listen
		retVal = listen(m_listenSocket, SOMAXCONN_HINT(65535));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=listen_failed error_code=%d | 소켓 수신 대기를 시작하지 못했습니다.", errVal);
			return FALSE;
		}

		// CP 핸들 생성
		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, serverSetting::iocpActiveThread);
		if (m_iocpHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=iocp_create_failed error_code=%d | I/O 완료 포트를 생성하지 못했습니다.", errVal);
			return FALSE;
		}

		CreateIoCompletionPort((HANDLE)m_listenSocket, m_iocpHandle, (ULONG_PTR)0, 0);

		DWORD dwBytes;
		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx),
			&m_pAcceptEx, sizeof(m_pAcceptEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_ex_lookup_failed error_code=%d | AcceptEx 함수 조회에 실패했습니다.", errVal);
			return FALSE;
		}

		retVal = WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs,
			sizeof(m_guidGetAcceptExSockaddrs), &m_pGetAcceptExSockaddrs, sizeof(m_pGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_addr_lookup_failed error_code=%d | GetAcceptExSockaddrs 함수 조회에 실패했습니다.", errVal);
			return FALSE;
		}

		// AcceptEx 요청
		FristPostAcceptEx();

		m_hasStartedBackgroundWork = true;
		for (int i = 0; i < serverSetting::contentsThreadCount; i++)
		{
			CContentsThread *pContentsThread = new CContentsThread;
			pContentsThread->Create();
		}

		CContentsThread::RunAll();

		// CreateWorkerThread
		for (int i = 1; i <= serverSetting::iocpWorkerThread; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				const int workerError = errno;
				g_pLogger->WriteEmergencyLogToStderr(
					"[Error][NetworkLib] event=worker_start_failed worker_index=%d error_code=%d | 네트워크 워커를 시작하지 못했습니다.\n",
					i, workerError);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_pLogger->WriteLog(ELogLevel::Info, L"NetworkLib", 0, std::source_location::current(),
				L"event=worker_started worker_index=%d | 네트워크 워커를 시작했습니다.", i);
		}

		RegisterSystemTimerEvent();
		RegisterContentTimerEvent();

		WaitForMultipleObjects((DWORD)m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);

		return TRUE;
	}

	void CServer::Stop()
	{
		// listen 소켓 닫기
		InterlockedExchange(&m_isStop, TRUE);
		closesocket(m_listenSocket);

		for (int i = 0; i < serverSetting::maxSessionCount; i++)
		{
			if (m_pSessions[i] != NULL)
			{
				Disconnect(m_pSessions[i]->m_sessionId);
			}
		}

		// while SessionCount 0일 때까지
		while (true)
		{
			// 세션 전부 끊고
			if (GetSessionCount() == 0)
			{
				m_isWorkerRun = FALSE;
				PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
				break;
			}
		}
	}

	bool CServer::TryPopRecvPacket(UINT64 sessionId, RecvPacketPtr &outPacket) noexcept
	{
		outPacket.reset();
		const USHORT index = getIndex(sessionId);
		if (m_pSessions == nullptr || index >= serverSetting::maxSessionCount)
			return false;
		CSession *pSession = m_pSessions[index];
		if (pSession == nullptr)
			return false;

		const LONG ioCount = InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((ioCount & CSession::sm_releaseFlag) == 0 && pSession->m_sessionId == sessionId)
		{
			CSerializableBufferView *pView = nullptr;
			if (pSession->m_recvMsgQueue.Dequeue(&pView))
				outPacket = RecvPacketPtr(pView, CSerializableBufferView::Free);
		}

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			ReleaseSession(pSession);
		return outPacket != nullptr;
	}

	void CServer::SendPacket(const UINT64 sessionId, CSerializableBuffer *pSBuffer) noexcept
	{
		CSession *pSession = m_pSessions[getIndex(sessionId)];

		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (pSession->SendPacket(pSBuffer))
			pSession->PostSend();
		else
			Disconnect(sessionId);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	BOOL CServer::Disconnect(const UINT64 sessionId, BOOL isPqcs) noexcept
	{
		CSession *pSession = m_pSessions[getIndex(sessionId)];

		// ReleaseFlag가 이미 켜진 상황
		InterlockedIncrement(&pSession->m_ioCountAndRelease);
		if ((pSession->m_ioCountAndRelease & CSession::sm_releaseFlag) == CSession::sm_releaseFlag)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		if (sessionId != pSession->m_sessionId)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		if (InterlockedExchange(&pSession->m_cancelIoCalled, TRUE) == TRUE)
		{
			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPqcs);
			}
			return FALSE;
		}

		// Io 실패 유도
		CancelIoEx((HANDLE)pSession->m_sessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
		{
			// IOCount == 0 이면 해제 시도
			ReleaseSession(pSession, isPqcs);
		}

		return TRUE;
	}

	BOOL CServer::ReleaseSession(CSession *pSession, BOOL isPqcs) noexcept
	{
		if (InterlockedCompareExchange(&pSession->m_ioCountAndRelease, CSession::sm_releaseFlag, 0) != 0)
		{
			return FALSE;
		}

		if (isPqcs)
		{
			PostQueuedCompletionStatus(m_iocpHandle, 0
				, (ULONG_PTR)pSession, (LPOVERLAPPED)EIoOperation::ReleaseSession);
			return TRUE;
		}

		closesocket(pSession->m_sessionSocket);
		USHORT index = getIndex(pSession->m_sessionId);
		UINT64 freeSessionId = pSession->m_sessionId;
	
		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);
		OnClientLeave(freeSessionId);

		CSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CServer::ReleaseSessionPqcs(CSession *pSession) noexcept
	{
		USHORT index = getIndex(pSession->m_sessionId);
		closesocket(pSession->m_sessionSocket);
		UINT64 freeSessionId = pSession->m_sessionId;

		OnClientLeave(freeSessionId);

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);

		CSession::Free(pSession);
		InterlockedDecrement(&m_sessionCount);
		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	void CServer::FristPostAcceptEx() noexcept
	{
		// 처음에 AcceptEx를 걸어두고 시작
		for (int i = 0; i < serverSetting::acceptexCount; i++)
		{
			PostAcceptEx(i);
		}
	}

	BOOL CServer::PostAcceptEx(INT index) noexcept
	{
		int retVal;
		int errVal;

		CSession *pNewAcceptEx = CSession::Alloc();
		m_pAcceptExSessions[index] = pNewAcceptEx;

		pNewAcceptEx->m_sessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pNewAcceptEx->m_sessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			CSession::Free(pNewAcceptEx);
			if (m_isStop)
				return FALSE;

			// 자원 오류 등: 반납만 하고 슬롯은 포기한다.
			const LONG lostCount = InterlockedIncrement(&m_lostAcceptSlotCount);
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=accept_slot_lost index=%d error_code=%d remaining=%d total=%d | 접속 수락 슬롯을 다시 걸지 못했습니다.",
				index, errVal, serverSetting::acceptexCount - lostCount, serverSetting::acceptexCount);
			return FALSE;
		}

		ZeroMemory(pNewAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
		g_overlappedAllocator.SetAcceptExIndex((ULONG_PTR)pNewAcceptEx->m_pMyOverlappedStartAddr, index);

		retVal = m_pAcceptEx(m_listenSocket, pNewAcceptEx->m_sessionSocket
			, pNewAcceptEx->m_acceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL,
			pNewAcceptEx->m_pMyOverlappedStartAddr);
		if (retVal == FALSE)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				// 즉시 실패는 완료 통지가 오지 않으므로 여기서 반납한다.
				closesocket(pNewAcceptEx->m_sessionSocket);
				CSession::Free(pNewAcceptEx);

				// 중단 중에는 리슨 소켓이 닫혀 실패하는 것이 정상이다.
				if (m_isStop)
					return FALSE;

				// 상대가 수락 전에 끊은 연결 : 같은 슬롯에 다시 건다.
				if (errVal == WSAECONNRESET || errVal == WSAECONNABORTED)
					return PostAcceptEx(index);

				// 리슨 소켓·버퍼·인자를 잘못 넘긴 코드 버그
				if (errVal == WSAEINVAL || errVal == WSAENOTSOCK || errVal == WSAEFAULT || errVal == WSAEOPNOTSUPP)
					__debugbreak();

				// 자원 오류 등: 반납만 하고 슬롯은 포기한다.
				const LONG lostCount = InterlockedIncrement(&m_lostAcceptSlotCount);
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=accept_slot_lost index=%d error_code=%d remaining=%d total=%d | 접속 수락 슬롯을 다시 걸지 못했습니다.",
					index, errVal, serverSetting::acceptexCount - lostCount, serverSetting::acceptexCount);
				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CServer::AcceptExCompleted(CSession *pSession) noexcept
	{
		InterlockedIncrement(&g_networkCounters.m_acceptTps);

		int retVal;
		int errVal;
		retVal = setsockopt(pSession->m_sessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_listenSocket, sizeof(SOCKET));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();

			if (!m_isStop)
				g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
					L"event=accept_context_update_failed error_code=%d | 수락 소켓의 컨텍스트 갱신에 실패했습니다.", errVal);
			return FALSE;
		}

		// 성공한 소켓에 대해 IOCP 등록
		CreateIoCompletionPort((HANDLE)pSession->m_sessionSocket, m_iocpHandle, (ULONG_PTR)pSession, 0);

		SOCKADDR_IN *pLocalAddr = nullptr;
		INT localAddrLen;

		SOCKADDR_IN *pRemoteAddr = nullptr;
		INT remoteAddrLen;
		m_pGetAcceptExSockaddrs(pSession->m_acceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
			(SOCKADDR **)&pLocalAddr, &localAddrLen, (SOCKADDR **)&pRemoteAddr, &remoteAddrLen);

		InetNtop(AF_INET, &pRemoteAddr->sin_addr, pSession->m_clientAddrBuffer, 16);

		pSession->m_clientPort = pRemoteAddr->sin_port;

		// TODO - 끊어줄 방법 고민
		if (!OnConnectionRequest(pSession->m_clientAddrBuffer, pSession->m_clientPort))
			return FALSE;

		USHORT index;
		// 연결 실패 : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
				L"event=session_slot_pop_failed | 비어 있는 세션 슬롯을 꺼내지 못했습니다.");
			return FALSE;
		}

		UINT64 combineId = CServer::combineIndex(index, InterlockedIncrement64(&m_currentId));

		pSession->Init(combineId);

		InterlockedIncrement(&m_sessionCount);
		InterlockedIncrement64(&g_networkCounters.m_acceptTotal);
		m_pSessions[index] = pSession;

		// 서버 중단 상태면 연결 끊기
		// * 이미 등록·카운트된 세션이므로 TRUE를 반환해 정상 해제 경로(OnAccept → 수신 취소 → ReleaseSession)로 보낸다.
		if (m_isStop)
		{
			Disconnect(combineId);
			return TRUE;
		}

		return TRUE;
	}

	int CServer::WorkerThread() noexcept
	{
		int retVal;
		DWORD flag = 0;
		while (m_isWorkerRun)
		{
			DWORD dwTransferred = 0;
			CSession *pSession = nullptr;
			OVERLAPPED *pLpOverlapped = nullptr;

			retVal = GetQueuedCompletionStatus(m_iocpHandle, &dwTransferred
				, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&pLpOverlapped
				, INFINITE);


			EIoOperation oper;
			if ((UINT64)pLpOverlapped >= 3)
			{
				if ((UINT64)pLpOverlapped < 0xFFFF)
				{
					oper = (EIoOperation)(UINT64)pLpOverlapped;
				}
				else
				{
					oper = g_overlappedAllocator.CalOperation((ULONG_PTR)pLpOverlapped);
				}
			}

			if (pLpOverlapped == nullptr)
			{
				if (retVal == FALSE)
				{
					const DWORD errorCode = GetLastError();
					g_pLogger->WriteLog(ELogLevel::Error, L"NetworkLib", 0, std::source_location::current(),
						L"event=iocp_wait_failed error_code=%lu | I/O 완료 대기에 실패해 워커를 종료합니다.", errorCode);
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// 정상 종료 루틴
					PostQueuedCompletionStatus(m_iocpHandle, 0, 0, 0);
					break;
				}
			}
			// 수신 종료 통지 뒤에는 OS가 페이지를 더 이상 사용하지 않는다.
			else if (oper == EIoOperation::Recv && (retVal == FALSE || dwTransferred == 0))
			{
				if (pSession->m_pRecvBuffer != nullptr && pSession->m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(pSession->m_pRecvBuffer);
				pSession->m_pRecvBuffer = nullptr;

				if (pSession->m_pDelayedBuffer != nullptr)
				{
					CSerializableBufferView::Free(pSession->m_pDelayedBuffer);
					pSession->m_pDelayedBuffer = nullptr;
				}
			}
			// 소켓 정상 종료
			else if (dwTransferred == 0 && oper != EIoOperation::AcceptEx && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case EIoOperation::AcceptEx:
				{
					// Accept가 성공한 세션 포인터를 얻어옴
					INT index = g_overlappedAllocator.GetAcceptExIndex((ULONG_PTR)pLpOverlapped);
					pSession = m_pAcceptExSessions[index];

					// 이거 실패하면 연결 끊음
					// * 실패 가능한 상황 - setsockopt, OnConnectionRequest
					// * ioCount 무조건 1일 것임
					// * 바로 끊어도 괜춘
					// * 다른 I/O 요청을 안걸고 끝내기 때문에 아래의 ioCount 0이 됨으로 연결 끊김을 유도
					InterlockedIncrement(&pSession->m_ioCountAndRelease);
					if (!AcceptExCompleted(pSession))
					{
						// 실패한 인덱스에 대한 예약은 다시 걸어줌
						if (!m_isStop)
							PostAcceptEx(index);
						
						InterlockedDecrement(&pSession->m_ioCountAndRelease);
						closesocket(pSession->m_sessionSocket);
						CSession::Free(pSession);
						continue;
					}

					// OnAccept 처리는 여기서
					OnAccept(pSession->m_sessionId);
					// 해당 세션에 대해 Recv 예약
					pSession->PostRecv();

					if (!m_isStop)
						PostAcceptEx(index);

					break;
				}
				break;
				case EIoOperation::Recv:
				{
					if (pSession->RecvCompleted(dwTransferred))
						pSession->PostRecv();
					else
						Disconnect(pSession->m_sessionId);
				}
				break;
				case EIoOperation::Send:
				{
					pSession->SendCompleted(dwTransferred);
				}
				break;
				case EIoOperation::ReleaseSession:
				{
					ReleaseSessionPqcs(pSession);
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_ioCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
		}

		return 0;
	}

	void CServer::RegisterSystemTimerEvent()
	{
		SMonitorTimerEvent *pMonitorEvent = new SMonitorTimerEvent;
		pMonitorEvent->SetEvent();
		CContentsThread::EnqueueEvent(pMonitorEvent);
		// CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pMonitorEvent);
	}
}
