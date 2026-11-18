#pragma once

namespace NetworkLib
{
	template<bool isLanServer> class CSerializableBuffer;

	class CNetClient
	{
	public:
		CNetClient() noexcept = default;
		virtual ~CNetClient();
		CNetClient(const CNetClient &) = delete;
		CNetClient &operator=(const CNetClient &) = delete;

		bool Connect(const char *pIp, USHORT port, BYTE packetCode) noexcept;
		bool SendPacket(CSerializableBuffer<FALSE> *pMessage) noexcept;
		void Disconnect() noexcept;
		bool IsConnected() const noexcept
		{
			return m_isConnected.load();
		}

	protected:
		// 콜백에 전달한 버퍼는 반환 시 무효가 된다.
		virtual bool OnRecv(CSerializableBuffer<FALSE> &message) noexcept = 0;
		virtual void OnDisconnect() noexcept = 0;

	private:
		bool postRecv() noexcept;
		bool postSend() noexcept;
		bool recvCompleted(DWORD bytes) noexcept;
		void beginClose() noexcept;
		void workerThread() noexcept;
		void releaseSendBuffers() noexcept;

		SOCKET m_socket = INVALID_SOCKET;
		HANDLE m_iocpHandle = nullptr;
		OVERLAPPED m_recvOverlapped{};
		OVERLAPPED m_sendOverlapped{};
		std::thread m_worker;
		std::mutex m_mutex;
		std::atomic<bool> m_isConnected = false;
		bool m_isClosing = false;
		bool m_isWsaStarted = false;
		BYTE m_packetCode = 0;
		int m_pendingIo = 0;
		std::array<char, 4096> m_recvBytes{};
		std::vector<char> m_recvStream;
		std::deque<CSerializableBuffer<FALSE> *> m_sendQueue;
		CSerializableBuffer<FALSE> *m_pSending = nullptr;
		DWORD m_sentBytes = 0;
	};
}
