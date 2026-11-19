#include "pch.h"
#include "NetHeaders.h"
#include "CSerializableBuffer.h"
#include "NetPacket.h"
#include "ClientEvent.h"
#include "CClientMailbox.h"
#include "CNetClientManager.h"
#include "NetworkLimits.h"
#include "CRingBuffer.h"
#include "CNetSession.h"

namespace ClientNetworkLib
{
	using namespace NetworkLib;

	CNetSession::CNetSession(BYTE packetCode, std::shared_ptr<CClientMailbox> spMailbox, int recvBufferBytes)
		: m_packetCode(packetCode)
		, m_spMailbox(std::move(spMailbox))
		, m_recvBuffer(recvBufferBytes)
	{
	}

	CNetSession::~CNetSession()
	{
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);
	}

	bool CNetSession::postConnectLocked(LPFN_CONNECTEX pConnectEx, const SOCKADDR_IN &address, DWORD timeoutMilliseconds) noexcept
	{
		m_connectDeadline = GetTickCount64() + timeoutMilliseconds;
		++m_ioCount;
		DWORD bytesSent = 0;
		if (pConnectEx(m_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address),
			nullptr, 0, &bytesSent, &m_connectOverlapped)
			|| WSAGetLastError() == WSA_IO_PENDING)
			return true;
		--m_ioCount;
		return false;
	}

	bool CNetSession::queueSendLocked(std::shared_ptr<CSerializableBuffer<FALSE>> spMessage) noexcept
	{
		if (m_state != EState::Connected || m_sendQueue.size() >= 512)
			return false;
		try
		{
			m_sendQueue.push_back(std::move(spMessage));
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		PrepareNetPacket(*m_sendQueue.back(), m_packetCode);
		return postSendLocked();
	}

	bool CNetSession::postSendLocked() noexcept
	{
		if (m_sendCount != 0)
			return true;

		if (m_sendQueue.empty())
			return true;

		WSABUF buffers[kWsasendMaxBufferCount];
		while (m_sendCount < m_sendBatch.size() && !m_sendQueue.empty())
		{
			auto &spPacket = m_sendBatch[m_sendCount];
			spPacket = std::move(m_sendQueue.front());
			m_sendQueue.pop_front();
			buffers[m_sendCount].buf = spPacket->GetBufferPtr();
			buffers[m_sendCount].len = spPacket->GetFullSize();
			++m_sendCount;
		}
		m_sendOverlapped = {};
		++m_ioCount;
		if (WSASend(m_socket, buffers, m_sendCount, nullptr, 0, &m_sendOverlapped, nullptr) == 0)
			return true;
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
			return true;
		--m_ioCount;
		beginCloseLocked(error);
		return false;
	}

	bool CNetSession::postRecvLocked() noexcept
	{
		const int freeBytes = m_recvBuffer.GetFreeSize();
		if (freeBytes == 0)
		{
			beginCloseLocked(ERROR_INVALID_DATA);
			return false;
		}

		m_recvOverlapped = {};
		const auto firstBytes = static_cast<ULONG>(m_recvBuffer.DirectEnqueueSize());
		const ULONG secondBytes = static_cast<ULONG>(freeBytes) - firstBytes;
		WSABUF buffers[2]{ { firstBytes, m_recvBuffer.GetRearPtr() }, { secondBytes, m_recvBuffer.GetPQueuePtr() } };
		DWORD flags = 0;
		++m_ioCount;
		if (WSARecv(m_socket, buffers, secondBytes == 0 ? 1 : 2, nullptr, &flags, &m_recvOverlapped, nullptr) == 0)
			return true;
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
			return true;
		--m_ioCount;
		beginCloseLocked(error);
		return false;
	}

	void CNetSession::completeLocked(OVERLAPPED *pOverlapped, DWORD bytes, DWORD error) noexcept
	{
		--m_ioCount;
		if (m_state == EState::Closing)
			return;
		if (pOverlapped == &m_connectOverlapped && GetTickCount64() >= m_connectDeadline)
			beginCloseLocked(WSAETIMEDOUT);
		else if (error != 0)
			beginCloseLocked(error);
		else if (pOverlapped == &m_connectOverlapped)
			connectCompletedLocked(); // 첫 데이터를 보내지 않은 ConnectEx도 0바이트 성공이다.
		else if (pOverlapped == &m_sendOverlapped)
			sendCompletedLocked();
		else if (bytes == 0)
			beginCloseLocked(0);
		else
		{
			const DWORD recvError = recvCompletedLocked(bytes);
			if (recvError != 0)
				beginCloseLocked(recvError);
			else
				(void)postRecvLocked();
		}
	}

	void CNetSession::connectCompletedLocked() noexcept
	{
		if (setsockopt(m_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR)
		{
			beginCloseLocked(WSAGetLastError());
			return;
		}
		BOOL isNoDelay = TRUE;
		setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&isNoDelay), sizeof(isNoDelay));
		if (!postRecvLocked())
			return;
		m_state = EState::Connected;
		m_wasConnected = true;
		m_spMailbox->publishConnected();
	}

	void CNetSession::sendCompletedLocked() noexcept
	{
		releaseSendBatchLocked();
		(void)postSendLocked();
	}

	void CNetSession::releaseSendBatchLocked() noexcept
	{
		for (DWORD index = 0; index < m_sendCount; ++index)
			m_sendBatch[index].reset();
		m_sendCount = 0;
	}

	DWORD CNetSession::recvCompletedLocked(DWORD bytes) noexcept
	{
		constexpr int kHeaderBytes = sizeof(SNetHeader);
		constexpr int kMaxPacketBytes = static_cast<int>(CSerializableBuffer<FALSE>::EBufferDefine::PacketMaxSize);

		if (bytes > static_cast<DWORD>(m_recvBuffer.GetFreeSize()))
			return ERROR_INVALID_DATA;
		m_recvBuffer.MoveRear(static_cast<int>(bytes));
		while (m_recvBuffer.GetUseSize() >= kHeaderBytes)
		{
			SNetHeader header;
			m_recvBuffer.Peek(reinterpret_cast<char *>(&header), kHeaderBytes);
			if (header.code != m_packetCode || header.len > kMaxPacketBytes - kHeaderBytes)
				return ERROR_INVALID_DATA;
			const int packetBytes = kHeaderBytes + header.len;
			if (m_recvBuffer.GetUseSize() < packetBytes)
				break;

			auto spMessage = CSerializableBuffer<FALSE>::MakeShared();
			if (spMessage == nullptr)
				return ERROR_NOT_ENOUGH_MEMORY;
			m_recvBuffer.Dequeue(spMessage->GetBufferPtr(), packetBytes);
			spMessage->MoveWritePos(header.len);
			CEncryption::Decoding(spMessage->GetBufferPtr() + 4, header.len + 1, header.randKey);
			BYTE checksum = CEncryption::CalCheckSum(spMessage->GetContentBufferPtr(), header.len);
			if (checksum != static_cast<BYTE>(spMessage->GetBufferPtr()[4]))
				return ERROR_INVALID_DATA;
			if (!m_spMailbox->pushPacket(std::move(spMessage)))
				return ERROR_NOT_ENOUGH_MEMORY;
		}
		return 0;
	}

	void CNetSession::beginCloseLocked(DWORD error) noexcept
	{
		if (m_state == EState::Closing)
			return;
		m_state = EState::Closing;
		m_closeError = error;

		// closesocket으로 취소해도 등록한 I/O의 완료는 모두 소비한다.
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	bool CNetSession::finishCloseLocked() noexcept
	{
		if (m_state != EState::Closing || m_ioCount != 0 || m_isFinalized)
			return false;
		m_isFinalized = true;
		releaseSendBatchLocked();
		m_sendQueue.clear();
		m_spMailbox->publishTerminalEvent(m_wasConnected ? EClientEvent::Disconnected : EClientEvent::ConnectFailed, m_closeError);
		return true;
	}
}
