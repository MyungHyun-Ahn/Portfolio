#pragma once

namespace ClientNetworkLib
{
	class CNetClient
	{
	public:
		CNetClient(const CNetClient &) = delete;
		CNetClient &operator=(const CNetClient &) = delete;

		[[nodiscard]]
		bool Connect(const char *pIp, USHORT port, BYTE packetCode, DWORD timeoutMilliseconds) noexcept;
		void Disconnect() noexcept;

		// 연결 상태와 콜백은 Tick()을 호출하는 앱 스레드에서 처리한다.
		void Tick() noexcept;

		[[nodiscard]]
		bool IsActive() const noexcept;
		[[nodiscard]]
		bool IsConnected() const noexcept;

	protected:
		explicit CNetClient(CNetClientManager &network) noexcept;
		virtual ~CNetClient() = default;

		[[nodiscard]]
		bool sendNetworkPacket(std::shared_ptr<NetworkCommonLib::CSerializableBuffer<FALSE>> spPacket) noexcept;

	private:
		virtual bool onConnect() noexcept = 0;
		virtual bool onRecv(NetworkCommonLib::CSerializableBuffer<FALSE> &message) noexcept = 0;
		// onRecv()의 실패를 더미 클라이언트에서 기록하기 위한 용도
		// - 일반 클라는 구현 안함
		virtual void onRecvRejected() noexcept
		{
		}
		virtual void onDisconnect(bool wasConnected, DWORD error) noexcept = 0;

	private:
		CNetClientManager 	&m_network;
		SessionId 			m_sessionId = 0;
		bool 				m_isConnected = false;
	};
}