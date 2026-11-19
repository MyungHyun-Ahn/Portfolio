#pragma once

namespace Chatting
{
	class CConsoleChatClient : private IClientPacketHandler
	{
	public:
		CConsoleChatClient(NetworkLib::CNetClientManager &network, const SChatConfig &config)
			: m_network(network)
			, m_config(config)
		{
		}
		bool Connect() noexcept;
		void Disconnect() noexcept;
		void Tick() noexcept;
		bool IsActive() const noexcept
		{
			return m_sessionId != 0;
		}
		bool IsConnected() const noexcept
		{
			return m_isConnected;
		}
		bool SendReqMessage(const std::wstring &text) noexcept;
		bool SendNotiHeartbeat() noexcept;
		bool IsReady() const noexcept
		{
			return m_isReady;
		}

	private:
		void onConnect() noexcept;
		bool login() noexcept;
		bool onRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept;
		void onDisconnect() noexcept;
		bool HandleResLogin(const SResLogin &packet) noexcept override;
		bool HandleResSectorMove(const SResSectorMove &packet) noexcept override;
		bool HandleResMessage(const SResMessage &packet) noexcept override;
		NetworkLib::CNetClientManager &m_network;
		SChatConfig m_config;
		NetworkLib::SessionId m_sessionId = 0;
		std::shared_ptr<NetworkLib::CClientMailbox> m_spMailbox;
		bool m_isConnected = false;
		bool m_isReady = false;
		bool m_isLogin = false;
	};
}
