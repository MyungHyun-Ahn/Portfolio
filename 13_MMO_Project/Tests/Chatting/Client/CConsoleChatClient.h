#pragma once

namespace Chatting
{
	class CConsoleChatClient : public NetworkLib::CNetClient, private IClientPacketHandler
	{
	public:
		explicit CConsoleChatClient(const SChatConfig &config)
			: m_config(config)
		{
		}
		~CConsoleChatClient() override
		{
			Disconnect();
		}
		bool Login() noexcept;
		bool SendMessage(const std::wstring &text) noexcept;
		bool SendHeartbeat() noexcept;
		bool IsReady() const noexcept
		{
			return m_isReady.load();
		}

	protected:
		bool OnRecv(NetworkLib::CSerializableBuffer<FALSE> &message) noexcept override;
		void OnDisconnect() noexcept override;

	private:
		bool HandleResLogin(const SResLogin &packet) noexcept override;
		bool HandleResSectorMove(const SResSectorMove &packet) noexcept override;
		bool HandleResMessage(const SResMessage &packet) noexcept override;
		bool sendOwned(NetworkLib::CSerializableBuffer<FALSE> *pMessage) noexcept;
		SChatConfig m_config;
		std::atomic<bool> m_isReady = false;
		bool m_isLogin = false;
	};
}
