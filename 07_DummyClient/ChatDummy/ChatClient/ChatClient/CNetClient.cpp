#include "pch.h"
#include "ClientSetting.h"
#include "CNetClient.h"
#include "COverlappedAllocator.h"

namespace NETWORK_CLIENT
{
	CNetClientManager *g_netClientMgr;

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

		if (m_pRecvBuffer != nullptr)
		{
			if (m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);
		}

		m_sSessionSocket = INVALID_SOCKET;
		m_uiSessionID = 0;
		m_pRecvBuffer = nullptr;
	}

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
			if (header.code != CLIENT_SETTING::PACKET_KEY)
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			if (header.len >= 65535 || header.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
				m_pDelayedBuffer = nullptr;

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
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
					CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
					m_pDelayedBuffer = nullptr;

					if (m_pRecvBuffer->DecreaseRef() == 0)
						CRecvBuffer::Free(m_pRecvBuffer);

					m_pRecvBuffer = nullptr;

					g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
					return;
				}

				InterlockedIncrement(&g_monitor.m_lRecvTPS);
				m_pDelayedBuffer->m_uiSessionId = m_uiSessionID;
				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->OnRecv(m_uiSessionID, m_pDelayedBuffer);
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
			if (packetHeader.code != CLIENT_SETTING::PACKET_KEY)
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			if (packetHeader.len >= 65535 || packetHeader.len >= m_pRecvBuffer->GetCapacity())
			{
				CSerializableBufferView<FALSE>::Free(view);

				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
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

				m_pRecvBuffer = nullptr;

				g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->Disconnect(m_uiSessionID);
				return;
			}

			InterlockedIncrement(&g_monitor.m_lRecvTPS);
			view->m_uiSessionId = m_uiSessionID;
			g_netClientMgr->m_arrNetClients[m_ClientMgrIndex]->OnRecv(m_uiSessionID, view);

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
		PostSend(TRUE);
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

	// NetClient
	// ���� ���� �غ� �۾�
	BOOL CNetClient::Init(INT maxSessionCount) noexcept
	{
		m_iMaxSessionCount = maxSessionCount;
		// ��Ŀ��Ʈ ���� ä���
		USHORT val = m_iMaxSessionCount - 1;
		for (int i = 0; i < m_iMaxSessionCount; i++)
		{
			m_stackDisconnectIndex.Push(val--);
		}

		m_arrPSessions = new CNetSession * [maxSessionCount];

		return TRUE;
	}

	void CNetClient::SendPacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
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

		if (!sBuffer->GetIsEnqueueHeader())
		{
			NetHeader header;
			header.code = 119; // �ڵ�
			header.len = sBuffer->GetDataSize();
			header.randKey = rand() % 256;
			header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
			sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

			// CheckSum ���� ��ȣȭ�ϱ� ����
			CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		}

		pSession->SendPacket(sBuffer);
		pSession->PostSend();

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			ReleaseSession(pSession);
		}
	}

	void CNetClient::EnqueuePacket(const UINT64 sessionID, CSerializableBuffer<FALSE> *sBuffer) noexcept
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

		if (!sBuffer->GetIsEnqueueHeader())
		{
			NetHeader header;
			header.code = 119; // �ڵ�
			header.len = sBuffer->GetDataSize();
			header.randKey = rand() % 256;
			header.checkSum = CEncryption::CalCheckSum(sBuffer->GetContentBufferPtr(), sBuffer->GetDataSize());
			sBuffer->EnqueueHeader((char *)&header, sizeof(NetHeader));

			// CheckSum ���� ��ȣȭ�ϱ� ����
			CEncryption::Encoding(sBuffer->GetBufferPtr() + 4, sBuffer->GetBufferSize() - 4, header.randKey);
		}

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

	BOOL CNetClient::Disconnect(const UINT64 sessionID, BOOL isPQCS) noexcept
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

		// Io ���� ����
		CancelIoEx((HANDLE)pSession->m_sSessionSocket, nullptr);

		if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
		{
			// IOCount == 0 �̸� ���� �õ�
			ReleaseSession(pSession, isPQCS);
		}

		return TRUE;
	}

	BOOL CNetClient::ReleaseSession(CNetSession *pSession, BOOL isPQCS) noexcept
	{
		// IoCount üũ�� ReleaseFlag ������ ���ÿ�
		if (InterlockedCompareExchange(&pSession->m_iIOCountAndRelease, CNetSession::RELEASE_FLAG, 0) != 0)
		{
			// ioCount 0�� �ƴϰų� �̹� Release ���� ���ΰ� �ִٸ� return
			return FALSE;
		}

		if (isPQCS)
		{
			PostQueuedCompletionStatus(g_netClientMgr->m_hIOCPHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)IOOperation::RELEASE_SESSION);
			return TRUE;
		}

		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnDisconnect(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetClient::ReleaseSessionPQCS(CNetSession *pSession) noexcept
	{
		USHORT index = GetIndex(pSession->m_uiSessionID);
		closesocket(pSession->m_sSessionSocket);
		UINT64 freeSessionId = pSession->m_uiSessionID;
		CNetSession::Free(pSession);
		InterlockedDecrement(&m_iSessionCount);

		OnDisconnect(freeSessionId);

		m_stackDisconnectIndex.Push(index);

		return TRUE;
	}

	BOOL CNetClient::PostConnectEx(const CHAR *ip, const USHORT port) noexcept
	{
		int retVal;
		int errVal;

		CNetSession *pConnectSession = CNetSession::Alloc();
		pConnectSession->m_sSessionSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pConnectSession->m_sSessionSocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"PostAcceptEx socket() ���� : %d", errVal);
			return FALSE;
		}

		// �۽� ���۸� 0���� ���� �������� I/O�� �츮�� ���۸� �̿��ϵ��� ����
		if (CLIENT_SETTING::USE_ZERO_COPY)
		{
			DWORD sendBufferSize = 0;
			retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBufferSize, sizeof(DWORD));
			if (retVal == SOCKET_ERROR)
			{
				errVal = WSAGetLastError();
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) ���� : %d", errVal);
				return FALSE;
			}
		}

		// LINGER option ����
		LINGER ling{ 1, 0 };
		retVal = setsockopt(pConnectSession->m_sSessionSocket, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof(ling));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) ���� : %d", errVal);
			return FALSE;
		}

		SOCKADDR_IN clientAddr;
		ZeroMemory(&clientAddr, sizeof(clientAddr));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_addr.s_addr = INADDR_ANY;
		clientAddr.sin_port = 0;

		retVal = bind(pConnectSession->m_sSessionSocket, (SOCKADDR *)&clientAddr, sizeof(clientAddr));
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"bind() ���� : %d", errVal);
			return FALSE;
		}

		// ���� ID ����
		USHORT index;
		// ���� ���� : FALSE
		if (!m_stackDisconnectIndex.Pop(&index))
		{
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"m_stackDisconnectIndex.Pop(&index) failed");
			return FALSE;
		}

		UINT64 combineId = CombineIndex(index, InterlockedIncrement64(&m_iCurrentID));
		pConnectSession->Init(combineId, m_ClientMgrIndex);

		m_arrPSessions[index] = pConnectSession;

		g_netClientMgr->RegisterIoCompletionPort(pConnectSession->m_sSessionSocket, (ULONG_PTR)pConnectSession);

		ZeroMemory(pConnectSession->m_pMyOverlappedStartAddr, sizeof(OVERLAPPED));

		SOCKADDR_IN serverAddr;
		ZeroMemory(&serverAddr, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		InetPtonA(AF_INET, ip, &serverAddr.sin_addr);
		serverAddr.sin_port = htons(port);

		if (!g_netClientMgr->ConnectEx(pConnectSession->m_sSessionSocket, (sockaddr *)&serverAddr, sizeof(serverAddr), pConnectSession->m_pMyOverlappedStartAddr))
		{
			errVal = WSAGetLastError();
			if (errVal != ERROR_IO_PENDING)
			{
				if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
					g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"AcceptEx() Error : %d", errVal);

				return FALSE;
			}
		}

		return TRUE;
	}

	BOOL CNetClient::ConnectExCompleted(CNetSession *pSession) noexcept
	{
		int retVal;
		int errVal;

		// SO_UPDATE_CONNECT_CONTEXT �ɼ� ����
		retVal = setsockopt(pSession->m_sSessionSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"setsockopt(SO_UPDATE_CONNECT_CONTEXT) ���� : %d", errVal);
			return FALSE;
		}

		return TRUE;
	}

	unsigned int NetWorkerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_netClientMgr->WorkerThread();
	}

	unsigned int NetTimerEventSchedulerThreadFunc(LPVOID lpParam) noexcept
	{
		return g_netClientMgr->TimerEventSchedulerThread();
	}

	BOOL CNetClientManager::Init() noexcept
	{
		int retVal;
		int errVal;
		WSAData wsaData;

		retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (retVal != 0)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAStartup() ���� : %d", errVal);
			return FALSE;
		}

		m_hIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, CLIENT_SETTING::IOCP_ACTIVE_THREAD);
		if (m_hIOCPHandle == NULL)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CreateIoCompletionPort(����) ���� : %d", errVal);
			return FALSE;
		}

		// ���� ����
		SOCKET dummySocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
		if (dummySocket == INVALID_SOCKET)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASocket() ���� : %d", errVal);
			return FALSE;
		}

		DWORD dwBytes;
		retVal = WSAIoctl(dummySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guidConnectEx, sizeof(m_guidConnectEx), &m_lpfnConnectEx, sizeof(m_lpfnConnectEx), &dwBytes, NULL, NULL);
		if (retVal == SOCKET_ERROR)
		{
			errVal = WSAGetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSAIoctl(lpfnConnectEx) ���� : %d", errVal);
			return FALSE;
		}

		closesocket(dummySocket);

		return TRUE;
	}

	BOOL CNetClientManager::Start() noexcept
	{
		int retVal;
		int errVal;

		// CreateTimerSchedulerThread
		m_hTimerEventSchedulerThread = (HANDLE)_beginthreadex(nullptr, 0, NetTimerEventSchedulerThreadFunc, nullptr, 0, nullptr);
		if (m_hTimerEventSchedulerThread == 0)
		{
			errVal = GetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"TimerEventSchedulerThread running fail.. : %d", errVal);
			return FALSE;
		}

		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] TimerEventSchedulerThread running..");

		// CreateWorkerThread
		for (int i = 1; i <= CLIENT_SETTING::IOCP_WORKER_THREAD; i++)
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

		return TRUE;
	}

	BOOL CNetClientManager::Wait() noexcept
	{
		WaitForMultipleObjects(m_arrWorkerThreads.size(), m_arrWorkerThreads.data(), TRUE, INFINITE);
		WaitForSingleObject(m_hTimerEventSchedulerThread, INFINITE);
		return TRUE;
	}

	void CNetClientManager::RegisterIoCompletionPort(SOCKET sock, ULONG_PTR sessionID)
	{
		CreateIoCompletionPort((HANDLE)sock, m_hIOCPHandle, sessionID, 0);
	}

	int CNetClientManager::WorkerThread() noexcept
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
			else if (dwTransferred == 0 && oper != IOOperation::CONNECTEX && (UINT)oper < 3)
			{
				// Disconnect(pSession->m_uiSessionID);
			}
			else
			{
				switch (oper)
				{
				case IOOperation::CONNECTEX:
				{
					InterlockedIncrement(&pSession->m_iIOCountAndRelease);
					if (!m_arrNetClients[pSession->m_ClientMgrIndex]->ConnectExCompleted(pSession))
					{
						if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
							CNetSession::Free(pSession);
						break;
					}

					m_arrNetClients[pSession->m_ClientMgrIndex]->OnConnect(pSession->m_uiSessionID);
					pSession->PostRecv();

					if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
						m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);

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
					pSession->PostSend();
				}
				break;
				case IOOperation::SENDPOST:
				{
					pSession->PostSend();
					continue;
				}
				break;
				case IOOperation::RELEASE_SESSION:
				{
					m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSessionPQCS(pSession);
					continue;
				}
				break;
				case IOOperation::TIMER_EVENT:
				{
					TimerEvent *timerEvent = (TimerEvent *)pSession;
					timerEvent->nextExecuteTime += timerEvent->timeMs;
					timerEvent->execute();

					// Event Apc Enqueue - PQ ����ȭ ���ϱ� ����
					RegisterTimerEvent(timerEvent);
					continue;
				}
				break;
				case IOOperation::CONTENT_EVENT: // ��ȸ�� �̺�Ʈ
				{
					BaseEvent *contentEvent = (BaseEvent *)pSession;
					contentEvent->execute();
					delete contentEvent;
					continue;
				}
				break;
				}
			}

			if (InterlockedDecrement(&pSession->m_iIOCountAndRelease) == 0)
			{
				m_arrNetClients[pSession->m_ClientMgrIndex]->ReleaseSession(pSession);
			}
		}

		return 0;
	}

	int CNetClientManager::TimerEventSchedulerThread() noexcept
	{
		// IOCP�� ���༺ ������ �ޱ� ���� GQCS
		DWORD dwTransferred = 0;
		CNetSession *pSession = nullptr;
		OVERLAPPED *lpOverlapped = nullptr;

		GetQueuedCompletionStatus(m_hIOCPHandle, &dwTransferred
			, (PULONG_PTR)&pSession, (LPOVERLAPPED *)&lpOverlapped
			, 0);


		while (m_bIsTimerEventSchedulerRun)
		{
			if (m_TimerEventQueue.empty())
				SleepEx(INFINITE, TRUE); // APC �۾��� Enqueue �Ǹ� ��� ��

			// EventQueue�� ���� ���� ��
			TimerEvent *timerEvent = m_TimerEventQueue.top();
			// ���� ���� ���� �Ǵ�
			LONG dTime = timerEvent->nextExecuteTime - timeGetTime();
			if (dTime <= 0) // 0���� ������ ���� ����
			{
				m_TimerEventQueue.pop();
				PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)timerEvent, (LPOVERLAPPED)IOOperation::TIMER_EVENT);
				continue;
			}

			// ���� �ð���ŭ ���
			SleepEx(dTime, TRUE);
		}

		return 0;
	}

	void CNetClientManager::RegisterTimerEvent(BaseEvent *timerEvent) noexcept
	{
		int retVal;
		int errVal;

		retVal = QueueUserAPC(RegisterTimerEventAPCFunc, m_hTimerEventSchedulerThread, (ULONG_PTR)timerEvent);
		if (retVal == FALSE)
		{
			errVal = GetLastError();
			g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"QueueUserAPC() ���� : %d", errVal);
		}
	}

	// EventSchedulerThread���� �����
	void CNetClientManager::RegisterTimerEventAPCFunc(ULONG_PTR lpParam) noexcept
	{
		g_netClientMgr->m_TimerEventQueue.push((TimerEvent *)lpParam);
	}
	void CNetClientManager::ContentEventEnqueue(BaseEvent *event) noexcept
	{
		PostQueuedCompletionStatus(m_hIOCPHandle, 0, (ULONG_PTR)event, (LPOVERLAPPED)IOOperation::CONTENT_EVENT);
	}
}
