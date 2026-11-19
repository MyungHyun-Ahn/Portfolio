#pragma once

namespace Chatting
{
	class CConsoleChatClient : public CChatNetClient
	{
	public:
		CConsoleChatClient(NetworkLib::CNetClientManager &network, const SChatConfig &config)
			: CChatNetClient(network)
			, m_config(config)
		{
		}

		bool IsReady() const noexcept
		{
			return m_isReady;
		}

	private:
		bool onConnect() noexcept override;
		bool login() noexcept;
		void onDisconnect(bool wasConnected, DWORD error) noexcept override;
		bool HandleResLogin(const SResLogin &packet) noexcept override;
		bool HandleResSectorMove(const SResSectorMove &packet) noexcept override;
		bool HandleResMessage(const SResMessage &packet) noexcept override;
		SChatConfig m_config;
		bool m_isReady = false;
		bool m_isLogin = false;
	};
}
