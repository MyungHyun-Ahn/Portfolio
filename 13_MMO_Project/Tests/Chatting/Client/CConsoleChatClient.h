#pragma once

namespace Chatting
{
	class CConsoleChatClient : public CChatNetClient
	{
	public:
		CConsoleChatClient(ClientNetworkLib::CClientManager &network, const SChatClientConfig &config)
			: CChatNetClient(network)
			, m_config(config)
		{
		}

	private:
		// Console 클라만의 코드를 여기에 작성
		bool 	onConnect() noexcept override;
		void 	onChatDisconnected(bool wasConnected, DWORD error) noexcept override;
		void 	onLoginAccepted(const SResLogin &packet) noexcept override;
		void 	onSectorEntered(const SResSectorMove &packet) noexcept override;
		void 	onChatMessageEcho(const SResMessage &packet) noexcept override;
		void 	onChatMessageNoti(const SMessageNoti &packet) noexcept override;
		
		SChatClientConfig m_config;
	};
}
