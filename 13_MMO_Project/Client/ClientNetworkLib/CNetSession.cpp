#include "pch.h"
#include "NetHeaders.h"
#include "CSerializableBuffer.h"
#include "NetPacket.h"
#include "CClientMailbox.h"
#include "CNetClientManager.h"
#include "CNetSession.h"

namespace ClientNetworkLib
{
	using namespace NetworkLib;

	CNetSession::CNetSession(BYTE packetCode, std::shared_ptr<CClientMailbox> spMailbox)
		: m_packetCode(packetCode)
		, m_spMailbox(std::move(spMailbox))
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
		++m_pendingIo;
		DWORD bytesSent = 0;
		if (pConnectEx(m_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address),
			nullptr, 0, &bytesSent, &m_connectOverlapped)
			|| WSAGetLastError() == WSA_IO_PENDING)
			return true;
		--m_pendingIo;
		return false;
	}

	bool CNetSession::queueSendLocked(std::shared_ptr<CSerializableBuffer<FALSE>> spMessage) noexcept
	{
		if (m_state != EState::Connected || m_isDrainingSend || m_sendQueue.size() >= 512)
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
		return m_spSending != nullptr || postSendLocked();
	}

	bool CNetSession::postSendLocked() noexcept
	{
		if (m_spSending == nullptr)
		{
			if (m_sendQueue.empty())
			{
				m_sendDrainedCv.notify_all();
				return true;
			}
			m_spSending = std::move(m_sendQueue.front());
			m_sendQueue.pop_front();
			m_sentBytes = 0;
		}
		m_sendOverlapped = {};
		WSABUF buffer;
		buffer.buf = m_spSending->GetBufferPtr() + m_sentBytes;
		buffer.len = m_spSending->GetFullSize() - m_sentBytes;
		++m_pendingIo;
		if (WSASend(m_socket, &buffer, 1, nullptr, 0, &m_sendOverlapped, nullptr) == 0)
			return true;
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
			return true;
		--m_pendingIo;
		beginCloseLocked(error);
		return false;
	}

	bool CNetSession::postRecvLocked() noexcept
	{
		m_recvOverlapped = {};
		WSABUF buffer{ static_cast<ULONG>(m_recvBytes.size()), m_recvBytes.data() };
		DWORD flags = 0;
		++m_pendingIo;
		if (WSARecv(m_socket, &buffer, 1, nullptr, &flags, &m_recvOverlapped, nullptr) == 0)
			return true;
		const int error = WSAGetLastError();
		if (error == WSA_IO_PENDING)
			return true;
		--m_pendingIo;
		beginCloseLocked(error);
		return false;
	}

	void CNetSession::completeLocked(OVERLAPPED *pOverlapped, DWORD bytes, DWORD error) noexcept
	{
		--m_pendingIo;
		if (m_state == EState::Closing)
			return;
		if (pOverlapped == &m_connectOverlapped && GetTickCount64() >= m_connectDeadline)
			beginCloseLocked(WSAETIMEDOUT);
		else if (error != 0)
			beginCloseLocked(error);
		else if (pOverlapped == &m_connectOverlapped)
			connectCompletedLocked(); // 첫 데이터를 보내지 않은 ConnectEx도 0바이트 성공이다.
		else if (bytes == 0)
			beginCloseLocked(0);
		else if (pOverlapped == &m_sendOverlapped)
			sendCompletedLocked(bytes);
		else if (!m_isDrainingSend)
		{
			try
			{
				const DWORD recvError = recvCompletedLocked(bytes);
				if (recvError != 0)
					beginCloseLocked(recvError);
				else
					(void)postRecvLocked();
			}
			catch (const std::bad_alloc &)
			{
				beginCloseLocked(ERROR_NOT_ENOUGH_MEMORY);
			}
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

	void CNetSession::sendCompletedLocked(DWORD bytes) noexcept
	{
		m_sentBytes += bytes;
		if (m_sentBytes == static_cast<DWORD>(m_spSending->GetFullSize()))
			m_spSending.reset();
		(void)postSendLocked();
	}

	DWORD CNetSession::recvCompletedLocked(DWORD bytes)
	{
		m_recvStream.insert(m_recvStream.end(), m_recvBytes.data(), m_recvBytes.data() + bytes);
		size_t offset = 0;
		while (m_recvStream.size() - offset >= sizeof(SNetHeader))
		{
			SNetHeader header;
			memcpy(&header, m_recvStream.data() + offset, sizeof(header));
			constexpr int kMaxPacketBytes = static_cast<int>(CSerializableBuffer<FALSE>::EBufferDefine::PacketMaxSize);
			if (header.code != m_packetCode || header.len > kMaxPacketBytes - sizeof(header))
				return ERROR_INVALID_DATA;
			size_t packetBytes = sizeof(header) + header.len;
			if (m_recvStream.size() - offset < packetBytes)
				break;

			auto spMessage = CSerializableBuffer<FALSE>::MakeShared();
			if (spMessage == nullptr)
				return ERROR_NOT_ENOUGH_MEMORY;
			memcpy(spMessage->GetBufferPtr(), m_recvStream.data() + offset, packetBytes);
			spMessage->MoveWritePos(header.len);
			CEncryption::Decoding(spMessage->GetBufferPtr() + 4, header.len + 1, header.randKey);
			BYTE checksum = CEncryption::CalCheckSum(spMessage->GetContentBufferPtr(), header.len);
			if (checksum != static_cast<BYTE>(spMessage->GetBufferPtr()[4]))
				return ERROR_INVALID_DATA;
			if (!m_spMailbox->pushPacket(std::move(spMessage)))
				return ERROR_NOT_ENOUGH_MEMORY;
			offset += packetBytes;
		}
		m_recvStream.erase(m_recvStream.begin(), m_recvStream.begin() + offset);
		return 0;
	}

	void CNetSession::beginCloseLocked(DWORD error) noexcept
	{
		if (m_state == EState::Closing)
			return;
		m_state = EState::Closing;
		m_closeError = error;
		if (m_wasConnected)
			shutdown(m_socket, SD_BOTH);
		// closesocket으로 취소해도 등록한 I/O의 완료는 모두 소비한다.
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		m_sendDrainedCv.notify_all();
	}

	bool CNetSession::finishCloseLocked() noexcept
	{
		if (m_state != EState::Closing || m_pendingIo != 0 || m_isFinalized)
			return false;
		m_isFinalized = true;
		m_spSending.reset();
		m_sendQueue.clear();
		m_spMailbox->publishTerminal(m_wasConnected ? EClientEvent::Disconnected : EClientEvent::ConnectFailed, m_closeError);
		return true;
	}
}
