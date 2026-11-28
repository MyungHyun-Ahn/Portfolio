#include "pch.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "CClientManager.h"
#include "CClient.h"
#include "GameClient.g.h"
#include "GameLoginId.h"
#include "SClientNetworkSettings.h"
#include "GameLoginFacade.h"

namespace GameLoginBridge
{
	namespace
	{
		constexpr BYTE kPacketCode = 0x77;
		constexpr DWORD kTimeoutMilliseconds = 10000;
		using Clock = std::chrono::steady_clock;

		class CAttempt final : public Game::GameClient
		{
		public:
			explicit CAttempt(ClientNetworkLib::CClientManager& network) noexcept : GameClient(network) {}

			std::wstring m_loginId;
			bool m_failed = false;

		private:
			bool onConnect() noexcept override
			{
				m_failed = !ReqLogin(m_loginId);
				return !m_failed;
			}
			void onRecvRejected() noexcept override { m_failed = true; }
			void onGameDisconnected(bool, DWORD) noexcept override { m_failed = true; }
		};

		class CLogin final : public ILogin
		{
		public:
			explicit CLogin(unsigned short port) noexcept : m_port(port) {}
			~CLogin() override { close(); }

			bool Submit(const wchar_t* pId) noexcept override
			{
				if (!pId || (m_phase != Phase::Idle && m_phase != Phase::Rejected && m_phase != Phase::Failed))
					return false;
				size_t length = 0;
				while (length <= Game::kMaxLoginIdUnits && pId[length] != 0)
					++length;
				if (!Game::IsValidLoginId({pId, length}))
					return false;
				try
				{
					if (!m_client)
					{
						ClientNetworkLib::SClientNetworkSettings settings;
						settings.packetMode = NetworkCommonLib::EPacketMode::Lan;
						if (!m_network.Init(settings))
							return fail();
						m_client = std::make_unique<CAttempt>(m_network);
					}
					m_client->m_loginId.assign(pId, length);
				}
				catch (const std::bad_alloc&)
				{
					return fail();
				}
				if (m_phase == Phase::Rejected)
				{
					if (!m_client->ReqLogin(m_client->m_loginId))
						return fail();
					m_phase = Phase::Waiting;
				}
				else
				{
					if (!m_client->Connect("127.0.0.1", m_port, kPacketCode, kTimeoutMilliseconds))
						return fail();
					m_phase = Phase::Connecting;
				}
				m_deadline = Clock::now() + std::chrono::milliseconds(kTimeoutMilliseconds);
				return true;
			}

			void Tick() noexcept override
			{
				if (!m_client)
					return;
				if ((m_phase == Phase::Connecting || m_phase == Phase::Waiting) && Clock::now() >= m_deadline)
				{
					fail();
					return;
				}
				m_client->Tick();
				if (m_client->m_failed)
				{
					fail();
					return;
				}
				using State = Game::GameClient::ELoginState;
				switch (m_client->GetLoginState())
				{
				case State::Idle: break;
				case State::Waiting: m_phase = Phase::Waiting; break;
				case State::Rejected: m_phase = Phase::Rejected; break;
				case State::Accepted: m_phase = Phase::Accepted; break;
				}
			}

			Phase GetPhase() const noexcept override { return m_phase; }
			unsigned long long GetCharacterId() const noexcept override { return m_client ? m_client->GetCharacterData().m_characterId : 0; }
			unsigned int GetLastLogoutMap() const noexcept override { return m_client ? m_client->GetCharacterData().m_lastLogoutMap : 0; }

		private:
			void close() noexcept
			{
				if (m_client)
					m_client->Disconnect();
				m_network.Stop();
				m_client.reset();
			}
			bool fail() noexcept
			{
				close();
				m_phase = Phase::Failed;
				return false;
			}

		private:
			ClientNetworkLib::CClientManager m_network;
			std::unique_ptr<CAttempt> m_client;
			unsigned short m_port;
			Phase m_phase = Phase::Idle;
			Clock::time_point m_deadline{};
		};
	}

	ILogin* Create(unsigned short port) noexcept
	{
		return port ? new (std::nothrow) CLogin(port) : nullptr;
	}

	void Destroy(ILogin* pLogin) noexcept { delete pLogin; }
}
