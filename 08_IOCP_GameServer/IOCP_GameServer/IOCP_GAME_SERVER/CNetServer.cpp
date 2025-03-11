#include "pch.h" 
#include "MyInclude.h"
#include "ServerSetting.h"
#include "CNetServer.h"
#include "SystemEvent.h"
#include "CContentThread.h"
#include "CBaseContent.h"

namespace NETWORK_SERVER
{
	CNetServer *g_NetServer = nullptr;

	void CNetSession::RecvCompleted(int size) noexcept
	{
		m_pRecvBuffer->MoveRear(size);

		bool delayFlag = false;

		// �������� ������ �Ȱ� �ִ� ���
		if (m_pDelayedBuffer != nullptr)
		{
			// ������ �Ȱ� ó���ؾ���
			USHORT delayedHeaderSize = m_pDelayedBuffer->isReadHeaderSize();
			// ��� ������ ����
			if (delayedHeaderSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE)
			{
				// ����� ������ �Ƴ�?
				// ������ �ƴٸ� ���⿡ ���ڸ� ��ŭ����
				int requireHeaderSize = (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE - delayedHeaderSize;
				// ����� 2���� �и����� ���� ����
				// ©�� ����� ���ְ�
				m_pDelayedBuffer->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), requireHeaderSize);
				m_pRecvBuffer->MoveFront(requireHeaderSize);
				// ��Ʈ��ũ ��� �ľ�
				// - ������ USHORT ���߿� ����� �����ϸ� ���⸦ ����
				NetHeader header;
				m_pDelayedBuffer->GetDelayedHeader((char *)&header, sizeof(NetHeader));

				// ���⼭ ���۸� �Ҵ�
				// - ��� ������ ����
				m_pDelayedBuffer->InitAndAlloc(header.len + sizeof(NetHeader));
				m_pDelayedBuffer->Copy((char *)&header, 0, sizeof(NetHeader));
			}

			// header�� �� ���� ��
			NetHeader header;
			m_pDelayedBuffer->GetHeader((char *)&header, sizeof(NetHeader));
			// code �ٸ��� ���� ����
			if (header.code != SERVER_SETTING::PACKET_CODE)
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				g_NetServer->Disconnect(m_uiSessionID);
				m_pDelayedBuffer = nullptr;
				m_pRecvBuffer = nullptr;
				return;
			}

			if (header.len >= 65535 || header.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				g_NetServer->Disconnect(m_uiSessionID);
				m_pDelayedBuffer = nullptr;
				m_pRecvBuffer = nullptr;
				return;
			}

			int requireSize = header.len - m_pDelayedBuffer->GetDataSize();
			// ������ ���ڸ�
			int recvBufferUseSize = m_pRecvBuffer->GetUseSize();
			if (recvBufferUseSize < requireSize)
			{
				delayFlag = true;
				m_pDelayedBuffer->Copy(m_pRecvBuffer->GetPQueuePtr(), recvBufferUseSize);
				m_pRecvBuffer->MoveFront(recvBufferUseSize);
			}
			else
			{
				// �ʿ��Ѹ�ŭ �� �� ������
				m_pDelayedBuffer->Copy(m_pRecvBuffer->GetFrontPtr(), requireSize);
				m_pRecvBuffer->MoveFront(requireSize);


				// OnRecv ȣ�� �� ��Ŷ ����
				CEncryption::Decoding(m_pDelayedBuffer->GetBufferPtr() + 4, m_pDelayedBuffer->GetFullSize() - 4, header.randKey);
				BYTE dataCheckSum = CEncryption::CalCheckSum(m_pDelayedBuffer->GetContentBufferPtr(), m_pDelayedBuffer->GetDataSize());
				m_pDelayedBuffer->GetHeader((char *)&header, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
				if (dataCheckSum != header.checkSum) // code �ٸ� �͵� �ɷ��� ����
				{
					if (m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);

					g_NetServer->Disconnect(m_uiSessionID);

					CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;
					m_pRecvBuffer = nullptr;
					return;
				}

				InterlockedIncrement(&g_monitor.m_lRecvTPS);
				m_pDelayedBuffer->IncreaseRef();
				m_RecvMsgQueue.Enqueue(m_pDelayedBuffer);
				// g_NetServer->OnRecv(m_uiSessionID, m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;
			}
		}

		DWORD currentUseSize = m_pRecvBuffer->GetUseSize();
		while (currentUseSize > 0)
		{
			NetHeader packetHeader;
			CSerializableBufferView<FALSE> *view = CSerializableBufferView<FALSE>::Alloc();

			if (currentUseSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE)
			{
				// Peek ���� delayedBuffer��
				delayFlag = true;

				int remainSize = m_pRecvBuffer->GetUseSize();
				// ���� ������ ��ŭ ��
				view->WriteDelayedHeader(m_pRecvBuffer->GetFrontPtr(), remainSize);
				m_pRecvBuffer->MoveFront(remainSize);
				m_pDelayedBuffer = view;

				// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"HeaderDelay");
				break;
			}

			int useSize = m_pRecvBuffer->GetUseSize();
			m_pRecvBuffer->Peek((char *)&packetHeader, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
			// code �ٸ��� ���� ����
			if (packetHeader.code != SERVER_SETTING::PACKET_CODE)
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}

			if (packetHeader.len >= 65535 || packetHeader.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}

			if (useSize < (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE + packetHeader.len)
			{
				// ���� �Ҵ��ؼ� �ڷ� �б� Delayed
				delayFlag = true;
				// ����� ���� ����
				// front ���� ������ ����
				view->InitAndAlloc(packetHeader.len + sizeof(NetHeader));
				view->Copy(m_pRecvBuffer->GetFrontPtr(), 0, useSize);
				m_pRecvBuffer->MoveFront(useSize);
				m_pDelayedBuffer = view;

				// g_Logger->WriteLog(L"RecvBuffer", LOG_LEVEL::DEBUG, L"PacketDelay");
				break;
			}

			// ���������� Recv ���� ���忡���� ���� ��
			// - �ϳ��� �޽����� ������ �ϼ��� ����
			int offsetStart = m_pRecvBuffer->GetFrontOffset();
			int offsetEnd = offsetStart + packetHeader.len + (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE;

			// �� view�� �׳� �ᵵ ��
			// Init ���� RecvBuffer�� Ref�� ����
			view->Init(m_pRecvBuffer, offsetStart, offsetEnd);
			m_pRecvBuffer->MoveFront((int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE + packetHeader.len);

			// OnRecv ȣ�� �� ��Ŷ ����
			CEncryption::Decoding(view->GetBufferPtr() + 4, view->GetFullSize() - 4, packetHeader.randKey);
			BYTE dataCheckSum = CEncryption::CalCheckSum(view->GetContentBufferPtr(), view->GetDataSize());
			view->GetHeader((char *)&packetHeader, (int)CSerializableBuffer<FALSE>::DEFINE::HEADER_SIZE);
			if (dataCheckSum != packetHeader.checkSum) // code �ٸ� �͵� �ɷ��� ����
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				g_NetServer->Disconnect(m_uiSessionID);
				m_pRecvBuffer = nullptr;
				return;
			}

			InterlockedIncrement(&g_monitor.m_lRecvTPS);
			view->IncreaseRef();
			m_RecvMsgQueue.Enqueue(view);
			// g_NetServer->OnRecv(m_uiSessionID, view);

			currentUseSize = m_pRecvBuffer->GetUseSize();
		}

		if (!delayFlag)
		{
			m_pDelayedBuffer = nullptr;
		}

		// Ref ����
		if (m_pRecvBuffer->DecreaseRef() == 0)
			CRecvBuffer::Free(m_pRecvBuffer);

		m_pRecvBuffer = nullptr;
	}

	// ��ť�� �� ����ȭ ������ �����͸� ��ť
	bool CNetSession::SendPacket(CSerializableBuffer<FALSE> *message) noexcept
	{
		// ���⼭ �ö� RefCount�� SendCompleted���� ������
		// Ȥ�� ReleaseSession
		message->IncreaseRef();
		m_lfSendBufferQueue.Enqueue(message);
		return TRUE;
	}

	void CNetSession::SendCompleted(int size) noexcept
	{
		InterlockedAdd(&g_monitor.m_lSendTPS, m_iSendCount);

		// m_iSendCount�� �ϰ� �Ҵ� ������ ����
		// * ����ŷ I/O�� ���� Send�� ��û�� �����ͺ��� �� ������ ��Ȳ�� �߻� ����
		// * �񵿱� I/O�� ������ ���� ������ �Ϸ� ������ ������


		int count;
		for (count = 0; count < m_iSendCount; count++)
		{
			// RefCount�� ���߰� 0�̶�� ���� �� ����
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		// SendFlag�� Ǯ�� �ʰ� ����
		// PostSend(TRUE);
		InterlockedExchange(&m_iSendFlag, FALSE);
	}

	bool CNetSession::PostRecv() noexcept
	{
		int errVal;
		int retVal;

		// ���⼭�� ���� Ref ������ �ʿ�
		// * WSARecv���� Ref ������ �Ұ���

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			InterlockedDecrement(&m_iIOCountAndRelease);
			return FALSE;
		}

		m_pRecvBuffer = CRecvBuffer::Alloc();
		m_pRecvBuffer->IncreaseRef();
		WSABUF wsaBuf;
		wsaBuf.buf = m_pRecvBuffer->GetPQueuePtr();
		wsaBuf.len = m_pRecvBuffer->GetCapacity();

		ZeroMemory((m_pMyOverlappedStartAddr + 1), sizeof(OVERLAPPED));

		DWORD flag = 0;
		{
			// PROFILE_BEGIN(0, "WSARecv");
			retVal = WSARecv(m_sSessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)(m_pMyOverlappedStartAddr + 1), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSARecv() Error : %d", errVal);

				// ��� ���⼱ 0�� �� ���� ����
				// ��ȯ���� �����ص� ��
				if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				if (m_iCacelIoCalled)
				{
					CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	bool CNetSession::PostSend(bool isPQCS) noexcept
	{
		int errVal;
		int retVal;
		int sendUseSize;

		if (!isPQCS)
		{
			sendUseSize = m_lfSendBufferQueue.GetUseSize();
			if (sendUseSize <= 0)
			{
				return FALSE;
			}


			if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
			{
				return FALSE;
			}
		}

		// ���⼭ ���� ��ŭ �� ��
		// �÷��� ȹ�� ������ ���д� �÷��� Ǯ���ֱ�
		sendUseSize = m_lfSendBufferQueue.GetUseSize();
		if (sendUseSize <= 0)
		{
			InterlockedExchange(&m_iSendFlag, FALSE);
			return FALSE;
		}

		InterlockedIncrement(&m_iIOCountAndRelease);
		if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			InterlockedDecrement(&m_iIOCountAndRelease);
			InterlockedExchange(&m_iSendFlag, FALSE);
			return FALSE;
		}

		WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

		m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);
		// WSASEND_MAX_BUFFER_COUNT ��ŭ 1�ʿ� ��� �������� ī��Ʈ
		// �� ��ġ�� ���ٸ� �� �ø� ��
		if (m_iSendCount == WSASEND_MAX_BUFFER_COUNT)
			InterlockedIncrement(&g_monitor.m_lMaxSendCount);

		int count;
		for (count = 0; count < m_iSendCount; count++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// ������ ��
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// ���� �ȵǴ� ��Ȳ
				CCrashDump::Crash();
			}

			wsaBuf[count].buf = pBuffer->GetBufferPtr();
			wsaBuf[count].len = pBuffer->GetFullSize();
			m_arrPSendBufs[count] = pBuffer;

			// // // ����� ���⼭ ����
			if (!pBuffer->GetIsEnqueueHeader())
			{
				pBuffer->m_isEnqueueHeader = true;
				NetHeader *header = (NetHeader *)pBuffer->GetBufferPtr();
				header->code = SERVER_SETTING::PACKET_CODE; // �ڵ�
				header->len = pBuffer->GetDataSize();
				header->randKey = 0;
				header->checkSum = CEncryption::CalCheckSum(pBuffer->GetContentBufferPtr(), pBuffer->GetDataSize());

				// CheckSum ���� ��ȣȭ�ϱ� ����
				CEncryption::Encoding(pBuffer->GetBufferPtr() + 4, pBuffer->GetBufferSize() - 4, header->randKey);
			}
		}

		ZeroMemory((m_pMyOverlappedStartAddr + 2), sizeof(OVERLAPPED));

		{
			// PROFILE_BEGIN(0, "WSASend");
			retVal = WSASend(m_sSessionSocket, wsaBuf, m_iSendCount, nullptr, 0, (LPOVERLAPPED)(m_pMyOverlappedStartAddr + 2), NULL);
		}
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET && errVal != WSAEINTR)
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);

				// ��� ���⼱ 0�� �� ���� ����
				// ��ȯ���� �����ص� ��
				if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
				{
					return FALSE;
				}
			}
			else
			{
				if (m_iCacelIoCalled)
				{
					CancelIoEx((HANDLE)m_sSessionSocket, nullptr);
					return FALSE;
				}
			}
		}

		return TRUE;
	}

	void CNetSession::Clear() noexcept
	{
		// ReleaseSession ��ÿ� �����ִ� send �����۸� Ȯ��
		// * �����ִ� ��찡 Ȯ�ε�
		// * ���� ����ȭ ���۸� �Ҵ� �����ϰ� ���� ����

		if (m_pDelayedBuffer != nullptr)
		{
			CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
		}

		for (int count = 0; count < m_iSendCount; count++)
		{
			if (m_arrPSendBufs[count]->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(m_arrPSendBufs[count]);
			}
		}

		m_iSendCount = 0;

		LONG useBufferSize = m_lfSendBufferQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBuffer<FALSE> *pBuffer;
			// ������ ��
			if (!m_lfSendBufferQueue.Dequeue(&pBuffer))
			{
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue::Dequeue() Error");
				// ���� �ȵǴ� ��Ȳ
				CCrashDump::Crash();
			}

			// RefCount�� ���߰� 0�̶�� ���� �� ����
			if (pBuffer->DecreaseRef() == 0)
			{
				CSerializableBuffer<FALSE>::Free(pBuffer);
			}
		}

		useBufferSize = m_lfSendBufferQueue.GetUseSize();
		if (useBufferSize != 0)
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"LFQueue is not empty Error");
		}

		useBufferSize = m_RecvMsgQueue.GetUseSize();
		for (int i = 0; i < useBufferSize; i++)
		{
			CSerializableBufferView<FALSE> *pBuffer;
			if (!m_RecvMsgQueue.Dequeue(&pBuffer))
			{
				CCrashDump::Crash();
			}

			CSerializableBufferView<FALSE>::Free(pBuffer);
		}

		if (m_pRecvBuffer != nullptr)
		{
			if (m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		m_pRecvBuffer = nullptr;
		m_pCurrentContent = nullptr;
	}

	unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_NetServer->WorkerThread();
	}

	BOOL CNetServer::Start(const CHAR *openIP, const USHORT port) noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		m_usMaxSessionCount = SERVER_SETTING::MAX_SESSION_COUNT;

		m_arrPSessions = new CNetSession * [SERVER_SETTING::MAX_SESSION_COUNT];

		ZeroMemory((char *)m_arrPSessions, sizeof(CNetSession *) * SERVER_SETTING::MAX_SESSION_COUNT);

		m_arrAcceptExSessions = new CNetSession * [SERVER_SETTING::ACCEPTEX_COUNT];

		// ��Ŀ��Ʈ ���� ä���
		USHORT val = SERVER_SETTING::MAX_SESSION_COUNT - 1;
		for (int i = 0; i < SERVER_SETTING::MAX_SESSION_COUNT; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
			return FALSE;
		}

		m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (m_sListenSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
			return FALSE;
		}

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, openIP, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		retVal = bind(m_sListenSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
			return FALSE;
		}

		// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
		if (SERVER_SETTING::USE_ZERO_COPY)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
				return FALSE;
			}
		}

		// LINGER option ����
		LINGER ling{ 1, 0 };
		retVal = setsockopt(m_sListenSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
			return FALSE;
		}

		// listen
		retVal = listen(m_sListenSocket, SOMAXCONN_HINT(65535));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"listen() ���� : %d", errVal);
			return FALSE;
		}

		// CP �ڵ� ����
		m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, SERVER_SETTING::IOCP_ACTIVE_THREAD);
		if (m_hIOCPHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
			return FALSE;
		}

		CreateIoCompletionPort((HANDLE)m_sListenSocket, m_hIOCPHandle, (ULONG_PTR)0, 0);

		DWORD dwBytes;
		retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidAcceptEx, sizeof(m_guidAcceptEx), &m_lpfnAcceptEx, sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnAcceptEx) ���� : %d", errVal);
			return FALSE;
		}

		retVal = WSAIoctl(m_sListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidGetAcceptExSockaddrs, sizeof(m_guidGetAcceptExSockaddrs), &m_lpfnGetAcceptExSockaddrs, sizeof(m_lpfnGetAcceptExSockaddrs), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnGetAcceptExSockaddrs) ���� : %d", errVal);
			return FALSE;
		}

		// AcceptEx ��û
		FristPostAcceptEx();

		// CreateContentThread
		for (int i = 0; i < SERVER_SETTING::CONTENT_THREAD_COUNT; i++)
		{
			CContentThread *pContentThread = new CContentThread;
			pContentThread->Start();
		}

		CContentThread::RunAll();

		// CreateWorkerThread
		for (int i = 1; i <= SERVER_SETTING::IOCP_WORKER_THREAD; i++)
		{
			HANDLE hWorkerThread = (HANDLE)_beginthreadex(nullptr, 0, NetWorkerThreadFunc, nullptr, 0, nullptr);
			if (hWorkerThread == 0)
			{
				errVal = GetLastError();
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WorkerThread[%d] running fail.. : %d", i, errVal);
				return FALSE;
			}

			m_arrWorkerThreads.push_back(hWorkerThread);
			g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] WorkerThread[%d] running..", i);
		}

		RegisterSystemTimerEvent();
		RegisterContentTimerEvent();

		WaitForMultipleObjects(m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);

		return TRUE;
	}

	void CNetServer::Stop()
	{
		// listen ���� �ݱ�
		InterlockedExchange(&m_isStop, TRUE);
		closesocket(m_sListenSocket);

		for (int i = 0; i < SERVER_SETTING::MAX_SESSION_COUNT; i++)
		{
			if (m_arrPSessions[i] != NULL)
			{
				Disconnect(m_arrPSessions[i]->m_uiSessionID);
			}
		}

		// while SessionCount 0�� ������
		while (true)
		{
			// ���� ���� ����
			if (m_iSessionCount == 0)
			{
				m_bIsWorkerRun = FALSE;
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
				break;
			}
		}
	}

	void CNetServer::SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		// ReleaseFlag�� �̹� ���� ��Ȳ
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
			return;
		}

		// if (!sBuffer->GetIsEnqueueHeader())
		// {
		// 	NetHeader header;
		// 	header.code = SERVER_SETTING::PACKET_CODE; // �ڵ�
		// 	header.len = sBuffer->GetDataSize();
		// 	header.randKey = 0;
		// 	header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
		// 	sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));
		// 
		// 	// CheckSum ���� ��ȣȭ�ϱ� ����
		// 	CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		// }

		pSession->SendPacket(sBuffer);
		pSession->PostSend();

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	// Sector ���� ���� ��쿡�� PQCS
	void CNetServer::EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];

		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		// ReleaseFlag�� �̹� ���� ��Ȳ
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, TRUE);
			}
			return;
		}

		// if (!sBuffer->GetIsEnqueueHeader())
		// {
		// 	NetHeader header;
		// 	header.code = SERVER_SETTING::PACKET_CODE; // �ڵ�
		// 	header.len = sBuffer->GetDataSize();
		// 	header.randKey = 0;
		// 	header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
		// 	sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));
		// 
		// 	// CheckSum ���� ��ȣȭ�ϱ� ����
		// 	CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		// }

		// Broadcast ��Ŷ�� ���⼭�� ��

		pSession->SendPacket(sBuffer);

		// {
		// 	// PROFILE_BEGIN(0, "WSASend");
		// 	pSession->PostSend();
		// }

		// {
		// 	// PROFILE_BEGIN(0, "PQCS");
		// 
		// 	if (pSession->m_iSendFlag == FALSE)
		// 	{
		// 		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::SENDPOST);
		// 	}
		// }

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession, TRUE);
		}
	}

	void CNetServer::EnqueuePacketPQCS(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
	{
		sBuffer->SetSessionId(sessionID);
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)sBuffer, (LPOVERLAPPED)IOOperation::ENQUEUE_PACKET);
	}

	void CNetServer::SendPQCS(const CNetSession *pSession)
	{
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::SENDPOST);
	}

	void CNetServer::SendAll()
	{
		for (int i = 0; i < SERVER_SETTING::MAX_SESSION_COUNT; i++)
		{
			if (m_arrPSessions[i] != nullptr)
				m_arrPSessions[i]->PostSend();
		}

		// InterlockedExchange(&m_SendAllFlag, FALSE);
	}

	void CNetServer::SendAllPQCS()
	{
		// if (InterlockedExchange(&m_SendAllFlag, TRUE) == FALSE)
			PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, (LPOVERLAPPED)IOOperation::SEND_ALL);
	}

	BOOL CNetServer::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
	{
		CNetSession *pSession = m_arrPSessions[GetIndex(sessionID)];
		if (pSession == nullptr)
			return FALSE;

		// ReleaseFlag�� �̹� ���� ��Ȳ
		InterlockedIncrement(&pSession->m_iIOCountAndRelease);
		if ((pSession->m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (sessionID != pSession->m_uiSessionID)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		if (InterlockedExchange(&pSession->m_iCacelIoCalled, TRUE) == TRUE)
		{
			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession, isPQCS);
			}
			return FALSE;
		}

		__debugbreak();

		// Io ���� ����
		CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			// IOCount == 0 �̸� ���� �õ�
			ReleaseSession(pSession, isPQCS);
		}

		return TRUE;
	}

	BOOL CNetServer::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
	{
		// IoCount üũ�� ReleaseFlag ������ ���ÿ�
		if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
		{
			// ioCount 0�� �ƴϰų� �̹� Release ���� ���ΰ� �ִٸ� return
			return FALSE;
		}

		if (isPQCS)
		{
			PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::RELEASE_SESSION);
			return TRUE;
		}

		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnClientLeave(freeSessionId);

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetServer::ReleaseSessionPQCS(CNetSession *pSession) noexcept
	{
		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnClientLeave(freeSessionId);

		if (pSession->m_pCurrentContent != nullptr)
			pSession->m_pCurrentContent->LeaveJobEnqueue(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	void CNetServer::FristPostAcceptEx() noexcept
	{
		// ó���� AcceptEx�� �ɾ�ΰ� ����
		for (int i = 0; i < SERVER_SETTING::ACCEPTEX_COUNT; i++)
		{
			PostAcceptEx(i);
		}
	}

	BOOL CNetServer::PostAcceptEx(INT index) noexcept
	{
		int retVal;
		int errVal;

		CNetSession *newAcceptEx = CNetSession::Alloc();
		m_arrAcceptExSessions[index] = newAcceptEx;

		newAcceptEx->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (newAcceptEx->m_sSessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
			return FALSE;
		}

		ZeroMemory(newAcceptEx->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));
		g_OverlappedAlloc.SetAcceptExIndex((ULONG_PTR)newAcceptEx->m_pMyOverlappedStartAddr, index);

		retVal = m_lpfnAcceptEx(m_sListenSocket, newAcceptEx->m_sSessionSocket
			, newAcceptEx->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, NULL, newAcceptEx->m_pMyOverlappedStartAddr);
		if (retVal == FALSE)
		{
			errVal = WSAGetLastError();
			if (errVal != WSA_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CNetServer::AcceptExCompleted(CNetSession *pSession) noexcept
	{
		InterlockedIncrement(&g_monitor.m_lAcceptTPS);

		int retVal;
		int errVal;
		retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&m_sListenSocket, sizeof(SOCKET));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();

			if (!m_isStop)
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_ACCEPT_CONTEXT) ���� : %d", errVal);
			return FALSE;
		}

		// ������ ���Ͽ� ���� IOCP ���
		CreateIoCompletionPort((HANDLE)pSession->m_sSessionSocket, m_hIOCPHandle, (ULONG_PTR)pSession, 0);

		SOCKADDR_IN *localAddr = nullptr;
		INT localAddrLen;

		SOCKADDR_IN *remoteAddr = nullptr;
		INT remoteAddrLen;
		m_lpfnGetAcceptExSockaddrs(pSession->m_AcceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (SOCKADDR **)&localAddr, &localAddrLen, (SOCKADDR **)&remoteAddr, &remoteAddrLen);

		InetNtop(AF_INET, &remoteAddr->sin_addr, pSession->m_ClientAddrBuffer, 16);

		pSession->m_ClientPort = remoteAddr->sin_port;

		// TODO - ������ ��� ���
		if (!OnConnectionRequest(pSession->m_ClientAddrBuffer, pSession->m_ClientPort))
			return FALSE;

		USHORT index;
		// ���� ���� : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
			return FALSE;
		}

		UINT64 combineId = CNetServer::CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));

		pSession->Init(combineId);

		InterlockedIncrement(&m_iSessionCount);
		InterlockedIncrement64(&g_monitor.m_lAcceptTotal);
		m_arrPSessions[index] = pSession;

		// ���� �ߴ� ���¸� ���� ����
		if (m_isStop)
		{
			Disconnect(combineId);
			return FALSE;
		}

		return TRUE;
	}

	int CNetServer::WorkerThread() noexcept
	{
		int retVal;
		DWORD flag = 0;
		while (m_bIsWorkerRun)
		{
			DWORD dwTransferred = 0;
			CNetSession *pSession = nullptr;
			OVERLAPPED *lpOverlapped = nullptr;

			retVal = GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
				, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
				, INFINITE);


			IOOperation oper;
			if ((UINT64)lpOverlapped >= 3)
			{
				if ((UINT64)lpOverlapped < 0xFFFF)
				{
					oper = (IOOperation)(UINT64)lpOverlapped;
				}
				else
				{
					oper = g_OverlappedAlloc.CalOperation((ULONG_PTR)lpOverlapped);
				}
			}

			if (lpOverlapped == nullptr)
			{
				if (retVal == FALSE)
				{
					// GetQueuedCompletionStatus Fail
					break;
				}

				if (dwTransferred == 0 && pSession == 0)
				{
					// ���� ���� ��ƾ
					PostQueuedCompletionStatus(m_hIOCPHandle, 0, 0, 0);
					break;
				}
			}
			// ���� ���� ����
			else if (dwTransferred == 0 && oper != IOOperation::ACCEPTEX && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case IOOperation::ACCEPTEX:
				{
					// Accept�� ������ ���� �����͸� ����
					INT index = g_OverlappedAlloc.GetAcceptExIndex((ULONG_PTR)lpOverlapped);
					pSession = m_arrAcceptExSessions[index];

					// �̰� �����ϸ� ���� ����
					// * ���� ������ ��Ȳ - setsockopt, OnConnectionRequest
					// * ioCount ������ 1�� ����
					// * �ٷ� ��� ����
					// * �ٸ� I/O ��û�� �Ȱɰ� ������ ������ �Ʒ��� ioCount 0�� ������ ���� ������ ����
					InterlockedIncrement(&pSession->m_iIOCountAndRelease);
					if (!AcceptExCompleted(pSession))
					{
						// ������ �ε����� ���� ������ �ٽ� �ɾ���
						if (!m_isStop)
							PostAcceptEx(index);
						else
							if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
								CNetSession::Free(pSession);
						break;
					}

					// OnAccept ó���� ���⼭
					OnAccept(pSession->m_uiSessionID);
					// �ش� ���ǿ� ���� Recv ����
					pSession->PostRecv();

					if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
						ReleaseSession(pSession);

					if (!m_isStop)
						PostAcceptEx(index);

					continue;
				}
				break;
				case IOOperation::RECV:
				{
					pSession->RecvCompleted(dwTransferred);
					pSession->PostRecv();
				}
				break;
				case IOOperation::SEND:
				{
					pSession->SendCompleted(dwTransferred);
				}
				break;
				case IOOperation::SENDPOST:
				{
					pSession->PostSend();
					continue;
				}
				break;
				case IOOperation::ENQUEUE_PACKET:
				{
					CSerializableBuffer<FALSE> *sBuffer = (CSerializableBuffer<FALSE> *)pSession;
					EnqueuePacket(sBuffer->GetSessionId(), sBuffer);

					continue;
				}
				case IOOperation::SEND_ALL:
				{
					// CDeque<CNetSession *> *pSessionQ = (CDeque<CNetSession *> *)pSession;

					SendAll();
					continue;
				}
				case IOOperation::RELEASE_SESSION:
				{
					ReleaseSessionPQCS(pSession);
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				ReleaseSession(pSession);
			}
		}

		return 0;
	}

	void CNetServer::RegisterSystemTimerEvent()
	{
		MonitorTimerEvent *pMonitorEvent = new MonitorTimerEvent;
		pMonitorEvent->SetEvent();
		// CContentThread::EnqueueEvent(pMonitorEvent);
		CContentThread::s_arrContentThreads[2]->EnqueueEventMy(pMonitorEvent);

		// SendAllTimerEvent *pSendAllEvent = new SendAllTimerEvent;
		// pSendAllEvent->SetEvent();
		// CContentThread::s_arrContentThreads[3]->EnqueueEventMy(pSendAllEvent);
	}
}
