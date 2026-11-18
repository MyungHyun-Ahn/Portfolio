#include "pch.h"
#include "DefineServer.h"
#include "CSerializableBuffer.h"
#include "NetPacket.h"
#include "CNetClient.h"

namespace NetworkLib
{
	CNetClient::~CNetClient()
	{
		Disconnect();
	}

	bool CNetClient::Connect(const char *pIp, USHORT port, BYTE packetCode) noexcept
	{
		Disconnect();
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
			return false;
		m_isWsaStarted = true;
		m_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		if (m_socket == INVALID_SOCKET)
		{
			Disconnect();
			return false;
		}

		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		if (InetPtonA(AF_INET, pIp, &address.sin_addr) != 1)
		{
			Disconnect();
			return false;
		}

		// 접속 실패 시 오래 기다리지 않도록 한다.
		u_long isNonblocking = 1;
		ioctlsocket(m_socket, FIONBIO, &isNonblocking);
		int result = connect(m_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address));
		if (result == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				Disconnect();
				return false;
			}
			fd_set writeSet;
			fd_set errorSet;
			FD_ZERO(&writeSet);
			FD_ZERO(&errorSet);
			FD_SET(m_socket, &writeSet);
			FD_SET(m_socket, &errorSet);
			timeval timeout{ 5, 0 };
			int socketError = 0;
			int errorBytes = sizeof(socketError);
			result = select(0, nullptr, &writeSet, &errorSet, &timeout);
			if (result <= 0 || FD_ISSET(m_socket, &errorSet)
				|| getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char *)&socketError, &errorBytes) == SOCKET_ERROR || socketError != 0)
			{
				Disconnect();
				return false;
			}
		}
		isNonblocking = 0;
		ioctlsocket(m_socket, FIONBIO, &isNonblocking);
		BOOL isNoDelay = TRUE;
		setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&isNoDelay, sizeof(isNoDelay));

		m_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
		if (m_iocpHandle == nullptr || CreateIoCompletionPort((HANDLE)m_socket, m_iocpHandle, 1, 0) == nullptr)
		{
			Disconnect();
			return false;
		}
		m_isClosing = false;
		m_packetCode = packetCode;
		m_isConnected = true;
		// 첫 recv도 완료 worker가 예약한다.
		PostQueuedCompletionStatus(m_iocpHandle, 0, 0, nullptr);
		try
		{
			m_worker = std::thread(&CNetClient::workerThread, this);
		}
		catch (...)
		{
			Disconnect();
			return false;
		}
		return true;
	}

	bool CNetClient::SendPacket(CSerializableBuffer<FALSE> *pMessage) noexcept
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_isConnected || m_isClosing || m_sendQueue.size() >= 512)
			return false;
		PrepareNetPacket(*pMessage, m_packetCode);
		pMessage->IncreaseRef();
		m_sendQueue.push_back(pMessage);
		if (m_pSending == nullptr)
			return postSend();

		return true;
	}

	bool CNetClient::postRecv() noexcept
	{
		ZeroMemory(&m_recvOverlapped, sizeof(m_recvOverlapped));
		WSABUF buffer{ static_cast<ULONG>(m_recvBytes.size()), m_recvBytes.data() };
		DWORD flags = 0;
		++m_pendingIo;
		if (WSARecv(m_socket, &buffer, 1, nullptr, &flags, &m_recvOverlapped, nullptr) == SOCKET_ERROR
			&& WSAGetLastError() != WSA_IO_PENDING)
		{
			--m_pendingIo;
			beginClose();
			return false;
		}
		return true;
	}

	bool CNetClient::postSend() noexcept
	{
		if (m_pSending == nullptr)
		{
			if (m_sendQueue.empty())
				return true;
			m_pSending = m_sendQueue.front();
			m_sendQueue.pop_front();
			m_sentBytes = 0;
		}
		ZeroMemory(&m_sendOverlapped, sizeof(m_sendOverlapped));
		WSABUF buffer;
		buffer.buf = m_pSending->GetBufferPtr() + m_sentBytes;
		buffer.len = m_pSending->GetFullSize() - m_sentBytes;
		++m_pendingIo;
		if (WSASend(m_socket, &buffer, 1, nullptr, 0, &m_sendOverlapped, nullptr) == SOCKET_ERROR
			&& WSAGetLastError() != WSA_IO_PENDING)
		{
			--m_pendingIo;
			beginClose();
			return false;
		}
		return true;
	}

	bool CNetClient::recvCompleted(DWORD bytes) noexcept
	{
		m_recvStream.insert(m_recvStream.end(), m_recvBytes.data(), m_recvBytes.data() + bytes);
		size_t offset = 0;
		while (m_recvStream.size() - offset >= sizeof(SNetHeader))
		{
			SNetHeader header;
			memcpy(&header, m_recvStream.data() + offset, sizeof(header));
			constexpr int kMaxPacketBytes = static_cast<int>(CSerializableBuffer<FALSE>::EBufferDefine::PacketMaxSize);
			if (header.code != m_packetCode || header.len > kMaxPacketBytes - sizeof(header))
				return false;
			size_t packetBytes = sizeof(header) + header.len;
			if (m_recvStream.size() - offset < packetBytes)
				break;

			auto *pMessage = CSerializableBuffer<FALSE>::Alloc();
			pMessage->IncreaseRef();
			memcpy(pMessage->GetBufferPtr(), m_recvStream.data() + offset, packetBytes);
			pMessage->MoveWritePos(header.len);
			MHLib::security::CEncryption::Decoding(pMessage->GetBufferPtr() + 4, header.len + 1, header.randKey);
			BYTE checksum = MHLib::security::CEncryption::CalCheckSum(pMessage->GetContentBufferPtr(), header.len);
			bool isValid = checksum == static_cast<BYTE>(pMessage->GetBufferPtr()[4]);
			if (isValid)
				isValid = OnRecv(*pMessage);
			if (pMessage->DecreaseRef() == 0)
				CSerializableBuffer<FALSE>::Free(pMessage);
			if (!isValid)
				return false;
			offset += packetBytes;
		}
		m_recvStream.erase(m_recvStream.begin(), m_recvStream.begin() + offset);
		return true;
	}

	void CNetClient::beginClose() noexcept
	{
		if (m_isClosing)
			return;
		m_isClosing = true;
		m_isConnected = false;
		shutdown(m_socket, SD_BOTH);
		CancelIoEx((HANDLE)m_socket, nullptr);
		PostQueuedCompletionStatus(m_iocpHandle, 0, 0, nullptr);
	}

	void CNetClient::workerThread() noexcept
	{
		bool isFirst = true;
		for (;;)
		{
			DWORD bytes = 0;
			ULONG_PTR key = 0;
			OVERLAPPED *pOverlapped = nullptr;
			BOOL isCompleted = GetQueuedCompletionStatus(m_iocpHandle, &bytes, &key, &pOverlapped, INFINITE);
			bool isRecv = false;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (pOverlapped != nullptr)
				{
					--m_pendingIo;
					if (!isCompleted || bytes == 0)
						beginClose();
					if (!m_isClosing)
					{
						if (pOverlapped == &m_recvOverlapped)
							isRecv = true;
						else if (pOverlapped == &m_sendOverlapped)
						{
							m_sentBytes += bytes;
							if (m_sentBytes == static_cast<DWORD>(m_pSending->GetFullSize()))
							{
								if (m_pSending->DecreaseRef() == 0)
									CSerializableBuffer<FALSE>::Free(m_pSending);
								m_pSending = nullptr;
							}
							postSend();
						}
					}
				}
				if (isFirst && !m_isClosing)
				{
					postRecv();
					isFirst = false;
				}
			}

			// 콜백에서 SendPacket을 호출할 수 있다.
			if (isRecv)
			{
				bool isValid = recvCompleted(bytes);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!isValid)
					beginClose();
				if (!m_isClosing)
					postRecv();
			}
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_isClosing && m_pendingIo == 0)
				break;
		}
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			releaseSendBuffers();
			m_isConnected = false;
		}
		OnDisconnect();
	}

	void CNetClient::releaseSendBuffers() noexcept
	{
		if (m_pSending != nullptr)
		{
			if (m_pSending->DecreaseRef() == 0)
				CSerializableBuffer<FALSE>::Free(m_pSending);
			m_pSending = nullptr;
		}
		for (auto *pMessage : m_sendQueue)
		{
			if (pMessage->DecreaseRef() == 0)
				CSerializableBuffer<FALSE>::Free(pMessage);
		}
		m_sendQueue.clear();
	}

	void CNetClient::Disconnect() noexcept
	{
		// 종료 전까지 넣은 메시지는 잠시 송신 완료를 기다린다.
		DWORD startTime = timeGetTime();
		while (m_isConnected && timeGetTime() - startTime < 2000)
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_pSending == nullptr && m_sendQueue.empty())
					break;
			}
			Sleep(1);
		}
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_socket != INVALID_SOCKET && m_iocpHandle != nullptr)
				beginClose();
		}
		// cancel한 IO의 완료를 모두 소비한 다음 버퍼와 핸들을 정리한다.
		if (m_worker.joinable())
			m_worker.join();
		if (m_socket != INVALID_SOCKET)
			closesocket(m_socket);
		if (m_iocpHandle != nullptr)
			CloseHandle(m_iocpHandle);
		m_socket = INVALID_SOCKET;
		m_iocpHandle = nullptr;
		m_isConnected = false;
		m_isClosing = false;
		m_recvStream.clear();
		if (m_isWsaStarted)
			WSACleanup();
		m_isWsaStarted = false;
	}
}
