#include "pch.h"
#include "ServerSetting.h"
#include "CNetSession.h"
#include "CNetServer.h"

void CNetSession::RecvCompleted(int size) noexcept
{
	m_pRecvBuffer->MoveRear(size);
	InterlockedIncrement(&g_monitor.m_lRecvTPS);

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
		if (header.code != PACKET_KEY)
		{
			CSerializableBufferView<FALSE>::Free(m_pDelayedBuffer);
			m_pDelayedBuffer = nullptr;
			__debugbreak();
			// ���⼭ ���°� ����?
			g_NetServer->Disconnect(m_uiSessionID);
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

				// Ref ����
				if (m_pRecvBuffer->DecreaseRef() == 0)
					CRecvBuffer::Free(m_pRecvBuffer);

				__debugbreak();

				// ���⼭ ���°� ����?
				g_NetServer->Disconnect(m_uiSessionID);
				return;
			}

			m_pDelayedBuffer->m_uiSessionId = m_uiSessionID;
			g_NetServer->OnRecv(m_uiSessionID, m_pDelayedBuffer);
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
		if (packetHeader.code != PACKET_KEY)
		{
			CSerializableBufferView<FALSE>::Free(view);

			__debugbreak();

			// ���⼭ ���°� ����?
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

			// Ref ����
			if (m_pRecvBuffer->DecreaseRef() == 0)
				CRecvBuffer::Free(m_pRecvBuffer);

			__debugbreak();

			// ���⼭ ���°� ����?
			g_NetServer->Disconnect(m_uiSessionID);
			return;
		}

		view->m_uiSessionId = m_uiSessionID;
		g_NetServer->OnRecv(m_uiSessionID, view);

		currentUseSize = m_pRecvBuffer->GetUseSize();
	}

	if (!delayFlag)
	{
		m_pDelayedBuffer = nullptr;
	}

	// Ref ����
	if (m_pRecvBuffer->DecreaseRef() == 0)
		CRecvBuffer::Free(m_pRecvBuffer);
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
	// m_SendBuffer.MoveFront(size);

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

	if (count != m_iSendCount)
	{
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CSession::SendCompleted %d != %d", count + 1, m_iSendCount);
	}

	m_iSendCount = 0;

#ifdef POSTSEND_LOST_DEBUG
	UINT64 index = InterlockedIncrement(&sendIndex);
	sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), 0xff, TRUE, 0 };
#endif
	InterlockedIncrement(&g_monitor.m_lSendTPS);

	if (!PostSend(TRUE))
	{
		InterlockedExchange(&m_iSendFlag, FALSE);
	}
}

bool CNetSession::PostRecv() noexcept
{
	int errVal;
	int retVal;

	// ���⼭�� ���� Ref ������ �ʿ�
	// * WSARecv���� Ref ������ �Ұ���
	m_pRecvBuffer = CRecvBuffer::Alloc();
	m_pRecvBuffer->IncreaseRef();
	WSABUF wsaBuf;
	wsaBuf.buf = m_pRecvBuffer->GetPQueuePtr();
	wsaBuf.len = m_pRecvBuffer->GetCapacity();

	ZeroMemory(&m_RecvOverlapped, sizeof(OVERLAPPED));

	InterlockedIncrement(&m_iIOCountAndRelease);
	if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
	{
		InterlockedDecrement(&m_iIOCountAndRelease);
		return FALSE;
	}


	DWORD flag = 0;
	retVal = WSARecv(m_sSessionSocket, &wsaBuf, 1, nullptr, &flag, (LPWSAOVERLAPPED)&m_RecvOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSARecv() Error : %d", errVal);

			// ��� ���⼱ 0�� �� ���� ����
			// ��ȯ���� �����ص� ��
			if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

bool CNetSession::PostSend(BOOL isCompleted) noexcept
{
	int errVal;
	int retVal;

	int sendUseSize = m_lfSendBufferQueue.GetUseSize();
	if (sendUseSize <= 0)
	{
		return FALSE;
	}

	if (!isCompleted)
	{
		if (InterlockedExchange(&m_iSendFlag, TRUE) == TRUE)
		{
#ifdef POSTSEND_LOST_DEBUG
			UINT64 index = InterlockedIncrement(&sendIndex);
			sendDebug[index % 65535] = { index, (USHORT)GetCurrentThreadId(), wher, FALSE, 2 };
#endif
			return TRUE;
		}
	}

	// ���⼭ ���� ��ŭ �� ��
	sendUseSize = m_lfSendBufferQueue.GetUseSize();
	if (sendUseSize <= 0)
	{
		InterlockedExchange(&m_iSendFlag, FALSE);
		return FALSE;
	}

	WSABUF wsaBuf[WSASEND_MAX_BUFFER_COUNT];

	m_iSendCount = min(sendUseSize, WSASEND_MAX_BUFFER_COUNT);

	InterlockedAdd(&g_monitor.m_lSendTPS, m_iSendCount);

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

	if (count != m_iSendCount)
	{
		g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"CSession::PostSend %d != %d", count + 1, m_iSendCount);
	}

	ZeroMemory(&m_SendOverlapped, sizeof(OVERLAPPED));

	InterlockedIncrement(&m_iIOCountAndRelease);
	if ((m_iIOCountAndRelease & CNetSession::RELEASE_FLAG) == CNetSession::RELEASE_FLAG)
	{
		InterlockedDecrement(&m_iIOCountAndRelease);
		return FALSE;
	}

	retVal = WSASend(m_sSessionSocket, wsaBuf, m_iSendCount, nullptr, 0, (LPOVERLAPPED)&m_SendOverlapped, NULL);
	if (retVal == SOCKET_ERROR)
	{
		errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (errVal != WSAECONNABORTED && errVal != WSAECONNRESET)
				g_Logger->WriteLog(L"SYSTEM", L"NetworkLib", LOG_LEVEL::ERR, L"WSASend() Error : %d", errVal);

			// ��� ���⼱ 0�� �� ���� ����
			// ��ȯ���� �����ص� ��
			if (InterlockedDecrement(&m_iIOCountAndRelease) == 0)
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}