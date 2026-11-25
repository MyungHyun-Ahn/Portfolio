#include "ServerNetworkLibPch.h"
#include "SNetworkCounters.h"
#include "PacketFraming.h"

namespace ServerNetworkLib
{
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
}
