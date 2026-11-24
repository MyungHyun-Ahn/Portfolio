#include "pch.h"
#include "NetworkCommonLibPch.h"
#include "COverlappedAllocator.h"

namespace ServerNetworkLib
{
	using namespace NetworkCommonLib;
}

#include "ServerSetting.h"
#include "CServer.h"

#include "ClientEvent.h"
#include "CClientManager.h"
#include "CClient.h"
#include "MHLib/utils/CFileLoader.h"
#include "IContents.h"
#include "CContentsRuntime.h"
#include "FixedMapsProtocol.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ClientPacketDispatch.g.h"
#include "ServerPacketDispatch.g.h"
#include "IServerPacketHandler.g.h"
#include "WorldClient.g.h"
#include "ServerPacketSend.g.h"

namespace WorldServer
{
	using namespace std::chrono_literals;
	using namespace ServerNetworkLib;
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;
	using ContentsRuntimeLib::CContentsRuntime;
	using ContentsRuntimeLib::IContents;

	class CMapInstance final : public IContents, private CServerPacketSender, private IServerPacketHandler
	{
	public:
		CMapInstance(UINT64 mapId, CServer &network) noexcept
			: IContents(mapId)
			, m_network(network)
		{
		}

		void AddConnection(UINT64 sessionId)
		{
			m_connections.push_back(sessionId);
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=map_connection_entered map=%llu thread_id=%lu session_id=%llu | 연결을 맵에 등록했습니다.",
				GetInstanceId(), GetCurrentThreadId(), sessionId);
		}

		void RemoveConnection(UINT64 sessionId) noexcept
		{
			if (std::erase(m_connections, sessionId) != 0)
			{
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=map_connection_removed map=%llu thread_id=%lu session_id=%llu | 끊어진 연결을 맵에서 제거했습니다.",
					GetInstanceId(), GetCurrentThreadId(), sessionId);
			}
		}

	protected:
		// 내 맵에 등록된 연결의 기존 수신 큐만 읽는다.
		void OnFrame() override
		{
			for (UINT64 sessionId : m_connections)
			{
				RecvPacketPtr upPacket(nullptr, CSerializableBufferView::Free);
				for (int count = 0; count < 16; ++count)
				{
					if (!m_network.TryPopRecvPacket(sessionId, upPacket))
						break;
					if (!DispatchServerPacket(sessionId, *upPacket, *this))
						(void)SendResAdd(sessionId, GetInstanceId(), m_total, static_cast<std::uint16_t>(EAddResult::InvalidRequest));
				}
			}
		}

	private:
		bool HandleReqAdd(UINT64 sessionId, const SReqAdd &packet) noexcept override
		{
			if (packet.m_amount == 0 || packet.m_amount > UINT64_MAX - m_total)
				return false;

			const auto mapId = GetInstanceId();
			const bool isOwnMap = packet.m_mapId == mapId;
			if (isOwnMap)
				m_total += packet.m_amount;
			const auto result = isOwnMap ? EAddResult::Applied : EAddResult::WrongMap;
			(void)SendResAdd(sessionId, mapId, m_total, static_cast<std::uint16_t>(result));
			return true;
		}

		void sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept override
		{
			m_network.SendPacket(sessionId, &packet);
		}

	private:
		CServer &m_network;

		std::vector<UINT64> m_connections;
		UINT64 m_total = 0;
	};

	class CFixedMapServer final : public CServer
	{
	public:
		[[nodiscard]]
		bool Run(USHORT port)
		{
			for (std::size_t index = 0; index < m_maps.size(); ++index)
			{
				auto upMap = std::make_unique<CMapInstance>(index == 0 ? kFirstMapId : kSecondMapId, *this);
				m_maps[index] = upMap.get();
				if (!m_runtime.RegisterContents(std::move(upMap), true, index))
					return false;
			}

			if (!m_runtime.Start())
				return false;
			return Start("127.0.0.1", port, NetworkCommonLib::EPacketMode::Lan) != FALSE;
		}

	private:

		bool OnConnectionRequest(const WCHAR *pIp, USHORT) noexcept override
		{
			return wcscmp(pIp, L"127.0.0.1") == 0;
		}

		void OnAccept(UINT64 sessionId) noexcept override
		{
			auto *pMap = getFixedMap(sessionId);
			if (!m_runtime.PostCompletion(pMap->GetInstanceId(), [pMap, sessionId]
			{
				pMap->AddConnection(sessionId);
			}))
				Disconnect(sessionId);
		}

		void OnClientLeave(UINT64 sessionId) noexcept override
		{
			auto *pMap = getFixedMap(sessionId);
			(void)m_runtime.PostCompletion(pMap->GetInstanceId(), [pMap, sessionId]
			{
				pMap->RemoveConnection(sessionId);
			});
		}

		void OnRecv(UINT64, CSerializableBufferView *) noexcept override
		{
			// 수신은 맵의 OnFrame에서 처리한다.
		}

		void OnError(int errorCode, WCHAR *) noexcept override
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"event=fixed_maps_network_error code=%d | 네트워크 오류입니다.", errorCode);
		}

		void RegisterContentTimerEvent() noexcept override
		{
			// 맵은 기존 콘텐츠 타이머에 등록하지 않는다.
			g_pLogger->WriteLogConsole(ELogLevel::Info,
				L"event=fixed_maps_ready port=%u | 고정 맵 테스트 서버가 준비되었습니다.",
				static_cast<unsigned>(serverSetting::openPort));
		}

		// 이동이 없는 테스트에서는 접속 번호의 홀짝으로 담당 맵을 고정한다.
		CMapInstance *getFixedMap(UINT64 sessionId) const noexcept
		{
			return m_maps[(sessionId & 1u) != 0 ? 0 : 1];
		}

	private:

		CContentsRuntime m_runtime{2, 5ms};
		std::array<CMapInstance *, 2> m_maps{};
	};


	// 맵 응답의 결과 범위를 확인한 뒤 클라이언트 콜백을 실행한다.
	bool WorldClient::HandleResAdd(const SResAdd &packet) noexcept
	{
		if (packet.m_result > static_cast<std::uint16_t>(EAddResult::InvalidRequest))
			return false;
		return onAddResponse(packet);
	}

	class CHeadlessClient final : public WorldClient
	{
	public:
		explicit CHeadlessClient(ClientNetworkLib::CClientManager &network) noexcept
			: WorldClient(network)
		{
		}

		// 정상 송신은 WorldClient를 사용하고, 이 경로는 디스패치 거절 데이터만 만든다.
		[[nodiscard]]
		bool SendRejectedRequest(std::uint16_t opcode, bool hasAmount, bool hasTrailingByte = false) noexcept
		{
			auto spPacket = NetworkCommonLib::CSerializableBuffer::MakeShared();
			if (!spPacket)
				return false;
			*spPacket << opcode << kSecondMapId;
			if (hasAmount)
				*spPacket << std::uint32_t{1};
			if (hasTrailingByte)
				*spPacket << std::uint8_t{0};
			return sendPacket(std::move(spPacket));
		}

		std::vector<SAddResponse> m_responses;

	private:
		bool onConnect() noexcept override
		{
			return true;
		}

		bool onAddResponse(const SResAdd &packet) noexcept override
		{
			m_responses.push_back({packet.m_mapId, packet.m_total, static_cast<EAddResult>(packet.m_result)});
			return true;
		}

		void onDisconnect(bool, DWORD) noexcept override
		{
		}
	};

	bool WaitForResponses(CHeadlessClient &client, std::size_t count, CHeadlessClient *pPeer = nullptr)
	{
		const auto deadline = std::chrono::steady_clock::now() + 3s;
		do
		{
			client.Tick();
			if (pPeer != nullptr)
				pPeer->Tick();
			if (!client.IsActive() || (pPeer != nullptr && !pPeer->IsActive()))
				return false;
			if (client.IsConnected() && client.m_responses.size() >= count)
				return true;
			std::this_thread::sleep_for(1ms);
		} while (std::chrono::steady_clock::now() < deadline);
		return false;
	}

	bool Connect(CHeadlessClient &client, USHORT port)
	{
		return client.Connect("127.0.0.1", port, kFixedMapsPacketCode, 3000) && WaitForResponses(client, 0);
	}

	bool RunClients(USHORT port)
	{
		ClientNetworkLib::CClientManager network;
		if (!network.Init(2, 4096, NetworkCommonLib::EPacketMode::Lan))
			return false;
		CHeadlessClient first(network);
		CHeadlessClient second(network);
		if (!Connect(first, port) || !first.SendReqAdd(kFirstMapId, 1) || !WaitForResponses(first, 1) || !Connect(second, port))
			return false;
		if (!second.SendReqAdd(kSecondMapId, 10) || !first.SendReqAdd(kFirstMapId, 1) || !first.SendReqAdd(kFirstMapId, 5)
			|| !first.SendReqAdd(kSecondMapId, 100) || !second.SendReqAdd(kFirstMapId, 100)
			|| !first.SendReqAdd(kFirstMapId, 3) || !second.SendReqAdd(kSecondMapId, 4)
			|| !second.SendRejectedRequest(kInvalidOpcode, true) || !second.SendRejectedRequest(0, true)
			|| !second.SendRejectedRequest(static_cast<std::uint16_t>(SReqAdd::kPacketType), false)
			|| !second.SendRejectedRequest(static_cast<std::uint16_t>(SReqAdd::kPacketType), true, true)
			|| !WaitForResponses(first, 5, &second) || !WaitForResponses(second, 7, &first))
			return false;

		using enum EAddResult;
		const std::vector<SAddResponse> firstExpected{
			{1, 1, Applied}, {1, 2, Applied}, {1, 7, Applied}, {1, 7, WrongMap}, {1, 10, Applied}
		};
		const std::vector<SAddResponse> secondExpected{
			{2, 10, Applied}, {2, 10, WrongMap}, {2, 14, Applied}, {2, 14, InvalidRequest},
			{2, 14, InvalidRequest}, {2, 14, InvalidRequest}, {2, 14, InvalidRequest}
		};
		if (first.m_responses != firstExpected || second.m_responses != secondExpected)
			return false;

		first.Disconnect();
		const auto deadline = std::chrono::steady_clock::now() + 3s;
		while (first.IsActive() && std::chrono::steady_clock::now() < deadline)
		{
			first.Tick();
			std::this_thread::sleep_for(1ms);
		}
		if (first.IsActive())
			return false;

		CHeadlessClient reconnected(network);
		if (!Connect(reconnected, port) || !reconnected.SendReqAdd(kFirstMapId, 5) || !WaitForResponses(reconnected, 1)
			|| reconnected.m_responses != std::vector<SAddResponse>{{1, 15, Applied}})
			return false;

		network.Stop();
		g_pLogger->WriteLogConsole(ELogLevel::Info,
			L"event=fixed_maps_pass first=1,2,7,7,10 second=10,10,14,14,14,14,14 reconnected=15 | 루프백 검증을 통과했습니다.");
		return true;
	}
}

int main(void)
{
	using namespace WorldServer;
	g_pLogger = MHLib::utils::CLogger::GetInstance();
	MHLib::utils::CFileLoader loader;
	loader.Parse(L"FixedMaps.conf");
	INT64 configuredPort = -1;
	std::wstring role;
	if (!loader.Load(L"FixedMaps", L"Role", &role) || !loader.Load(L"FixedMaps", L"Port", &configuredPort)
		|| configuredPort <= 0 || configuredPort > USHRT_MAX || (role != L"Server" && role != L"Client"))
	{
		g_pLogger->WriteLogConsole(ELogLevel::Error, L"event=fixed_maps_bad_config | 테스트 설정이 잘못되었습니다.");
		return 2;
	}

	const auto port = static_cast<USHORT>(configuredPort);
	if (role == L"Client")
		return RunClients(port) ? 0 : 1;

	if (!g_pLogger->Start(L"LogFile", ELogLevel::Info, ELogLevel::Error, std::chrono::milliseconds{10}, 256))
		return 2;
	g_pLogger->RegisterLogProducer();
	MHLib::utils::g_pProfileManager = MHLib::utils::CProfileManager::GetInstance();
	serverSetting::maxSessionCount = 8;
	serverSetting::acceptexCount = 2;
	serverSetting::iocpWorkerThread = 2;
	serverSetting::iocpActiveThread = 2;
	serverSetting::contentsThreadCount = 1;
	serverSetting::openPort = port;
	serverSetting::packetCode = kFixedMapsPacketCode;
	CFixedMapServer server;
	g_pServer = &server;
	// 테스트 실행기가 이 프로세스만 회수한다. 기존 네트워크 Stop은 사용하지 않는다.
	ExitProcess(server.Run(port) ? 0 : 1);
}
