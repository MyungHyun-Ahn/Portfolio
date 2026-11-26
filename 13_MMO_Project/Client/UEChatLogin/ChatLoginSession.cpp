#include "pch.h"
#include <condition_variable>
#include "CommonProtocol.h"
#include "PacketFraming.h"
#include "CClientManager.h"
#include "CClient.h"
#include "SClientNetworkSettings.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CChatNetClient.g.h"
#include "IChatLoginSession.h"

namespace Chatting
{
	class CLoginClient final : public CChatNetClient
	{
	public:
		CLoginClient(ClientNetworkLib::CClientManager& network, long long account) noexcept
			: CChatNetClient(network)
			, m_account(account)
		{
		}

		SLoginTickResult TickLogin() noexcept
		{
			m_result = {};
			Tick();
			return m_result;
		}

	private:
		bool onConnect() noexcept override
		{
			std::array<char, kSessionKeyBytes> key{};
			std::copy_n(kTestSessionKey, key.size(), key.begin());
			m_result.m_submitted = ReqLogin(m_account, L"UEClient", L"UEClient", key);
			return m_result.m_submitted;
		}
		void onLoginAccepted(const SResLogin& packet) noexcept override { m_result.m_acceptedAccount = packet.m_accountNo; }
		void onSectorEntered(const SResSectorMove&) noexcept override {}
		void onChatMessageEcho(const SResMessage&) noexcept override {}
		void onChatMessageNoti(const SMessageNoti&) noexcept override {}
		void onChatDisconnected(bool, DWORD error) noexcept override
		{
			m_result.m_disconnected = true;
			m_result.m_error = error;
		}

	private:
		long long m_account;
		SLoginTickResult m_result;
	};

	class CLoginSession final : public IChatLoginSession
	{
	public:
		explicit CLoginSession(long long account) noexcept : m_client(m_network, account) {}
		~CLoginSession() override
		{
			m_client.Disconnect();
			m_network.Stop();
		}
		bool Start(unsigned short port) noexcept
		{
			// 공유 프레이밍 키는 IOCP 워커를 시작하기 전에 프로세스당 한 번만 설정한다.
			static std::once_flag keyOnce;
			std::call_once(keyOnce, [] { MHLib::security::CEncryption::sm_packetKey = kPacketKey; });
			return m_network.Init({}) && m_client.Connect("127.0.0.1", port, kPacketCode, kLoginTimeoutMilliseconds);
		}
		SLoginTickResult TickLogin() noexcept override { return m_client.TickLogin(); }

	private:
		ClientNetworkLib::CClientManager m_network;
		CLoginClient m_client;
	};

	IChatLoginSession* StartLocalLogin(unsigned short port, long long account) noexcept
	{
		std::unique_ptr<CLoginSession> session(new (std::nothrow) CLoginSession(account));
		return session && session->Start(port) ? session.release() : nullptr;
	}
}
