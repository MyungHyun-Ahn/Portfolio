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
#include "FixedMapsMoveQueue.h"
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

	class CMapHost;

	class CMapInstance final : private CServerPacketSender, private IServerPacketHandler
	{
	public:
		CMapInstance(UINT64 mapId, CServer &network, CMoveQueue &moveQueue, CMapHost &host, bool usesTestControls) noexcept
			: m_mapId(mapId)
			, m_network(network)
			, m_moveQueue(moveQueue)
			, m_host(host)
			, m_usesTestControls(usesTestControls)
		{
		}

		~CMapInstance() noexcept
		{
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=map_destroyed map=%llu thread_id=%lu | 게임플레이 맵 객체를 파괴했습니다.", m_mapId, GetCurrentThreadId());
		}

		UINT64 GetInstanceId() const noexcept
		{
			return m_mapId;
		}

		void AddConnection(UINT64 sessionId, UINT64 characterId)
		{
			if (!m_moveQueue.IsOnMap(sessionId, m_mapId))
				return;
			m_characters.emplace(sessionId, std::make_unique<SCharacter>(characterId));
			m_emptySince = {};
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=map_connection_entered map=%llu thread_id=%lu session_id=%llu | 연결을 맵에 등록했습니다.",
				GetInstanceId(), GetCurrentThreadId(), sessionId);
		}

		void RemoveConnection(UINT64 sessionId) noexcept
		{
			if (m_characters.erase(sessionId) != 0)
			{
				noteDeparture();
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=map_connection_removed map=%llu thread_id=%lu session_id=%llu | 끊어진 연결을 맵에서 제거했습니다.",
					GetInstanceId(), GetCurrentThreadId(), sessionId);
			}
		}

		bool HasIdleGraceElapsed(std::chrono::milliseconds idleGrace) const noexcept
		{
			return m_characters.empty() && m_emptySince != std::chrono::steady_clock::time_point{}
				&& std::chrono::steady_clock::now() - m_emptySince >= idleGrace;
		}

		// 내 맵에 등록된 연결의 기존 수신 큐만 읽는다.
		void RunFrame()
		{
			if (m_usesTestControls && GetInstanceId() == kSecondMapId)
				tryTestClose();
			if (!m_usesTestControls || GetInstanceId() != kSecondMapId || hasTestSignal(L"release-arrival"))
				attachMove();
			for (auto entry = m_characters.begin(); entry != m_characters.end();)
			{
				const auto &[sessionId, upCharacter] = *entry++;
				if (upCharacter->pendingChange != 0 && !upCharacter->isChangePosted
					&& (!m_usesTestControls || hasTestSignal(L"release-change")))
				{
					upCharacter->isChangePosted = postChange(sessionId);
				}
				if (upCharacter->targetMapId != 0)
				{
					if (upCharacter->pendingChange == 0)
						advanceMove(sessionId, *upCharacter);
					continue;
				}
				RecvPacketPtr upPacket(nullptr, CSerializableBufferView::Free);
				for (int count = 0; count < 16; ++count)
				{
					if (!m_network.TryPopRecvPacket(sessionId, upPacket))
						break;
					std::uint16_t opcode = 0;
					if (!DispatchServerPacket(sessionId, *upPacket, *this, &opcode))
					{
						if (opcode == static_cast<std::uint16_t>(EPacketType::ReqMove))
							sendMoveResult(sessionId, *upCharacter, EMoveResult::InvalidRequest);
						else if (opcode == static_cast<std::uint16_t>(EPacketType::ReqChange))
							sendChangeResult(sessionId, *upCharacter, EAddResult::InvalidRequest);
						else
							(void)SendResAdd(sessionId, GetInstanceId(), m_total, static_cast<std::uint16_t>(EAddResult::InvalidRequest));
					}
					if (upCharacter->targetMapId != 0)
						break;
				}
			}
		}

	private:
		friend class CMapHost;


		void noteDeparture() noexcept
		{
			if (m_characters.empty())
				m_emptySince = std::chrono::steady_clock::now();
		}

		bool postChange(UINT64 sessionId);

		static bool hasTestSignal(const WCHAR *pFileName) noexcept
		{
			return GetFileAttributesW(pFileName) != INVALID_FILE_ATTRIBUTES;
		}

		void tryTestClose() noexcept
		{
			if (m_isAdmissionClosed || !m_characters.empty() || !hasTestSignal(L"close-map-2"))
				return;
			if (m_moveQueue.TryClose(GetInstanceId()))
			{
				m_isAdmissionClosed = true;
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_admission_closed map=%llu | 목적지 접수를 닫았습니다.", GetInstanceId());
			}
			else if (!m_hasLoggedCloseDeferred)
			{
				m_hasLoggedCloseDeferred = true;
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_close_deferred map=%llu | 예약이 남아 목적지 접수를 유지합니다.", GetInstanceId());
			}
		}

		void completeChange(UINT64 sessionId) noexcept
		{
			const auto entry = m_characters.find(sessionId);
			if (entry == m_characters.end() || !entry->second->isChangePosted || !m_moveQueue.IsOnMap(sessionId, GetInstanceId()))
				return;
			auto &character = *entry->second;
			character.value += std::exchange(character.pendingChange, 0);
			character.isChangePosted = false;
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=character_change_completed map=%llu thread_id=%lu session_id=%llu character_id=%llu value=%llu"
				L" | 상태 변경을 완료했습니다.", GetInstanceId(), GetCurrentThreadId(), sessionId, character.characterId, character.value);
			sendChangeResult(sessionId, character, EAddResult::Applied);
		}

		bool HandleReqChange(UINT64 sessionId, const SReqChange &packet) noexcept override
		{
			auto &character = *m_characters.find(sessionId)->second;
			if (packet.m_mapId != GetInstanceId())
			{
				sendChangeResult(sessionId, character, EAddResult::WrongMap);
				return true;
			}
			if (character.pendingChange != 0 || packet.m_amount == 0 || packet.m_amount > UINT64_MAX - character.value)
				return false;
			character.pendingChange = packet.m_amount;
			return true;
		}

		void sendChangeResult(UINT64 sessionId, const SCharacter &character, EAddResult result) noexcept
		{
			(void)SendResChange(sessionId, GetInstanceId(), character.value, static_cast<std::uint16_t>(result));
		}

		void advanceMove(UINT64 sessionId, SCharacter &character)
		{
			const auto targetMapId = std::exchange(character.targetMapId, 0);
			const auto result = m_moveQueue.Reserve(sessionId, targetMapId);
			if (result != EMoveResult::Moved)
			{
				if (result != EMoveResult::Disconnected)
					sendMoveResult(sessionId, character, result);
				return;
			}

			// 테스트에서만 예약 직후 출발 워커를 멈춘다. 예약 락은 이미 풀렸다.
			if (m_usesTestControls && targetMapId == kSecondMapId && hasTestSignal(L"hold-before-move-publish"))
			{
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_publish_held target=%llu session_id=%llu | 게시 전 이동 예약을 유지합니다.", targetMapId, sessionId);
				const auto deadline = std::chrono::steady_clock::now() + 5s;
				while (!hasTestSignal(L"release-move-publish") && std::chrono::steady_clock::now() < deadline)
					std::this_thread::sleep_for(1ms);
			}

			auto entry = m_characters.extract(sessionId);
			noteDeparture();
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=move_detached map=%llu thread_id=%lu session_id=%llu character_id=%llu | 캐릭터를 분리했습니다.",
				GetInstanceId(), GetCurrentThreadId(), sessionId, character.characterId);
			m_moveQueue.Publish(targetMapId, {sessionId, std::move(entry.mapped())});
			if (m_usesTestControls)
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_published session_id=%llu | 예약된 인계를 게시했습니다.", sessionId);
		}

		void attachMove()
		{
			auto job = m_moveQueue.Take(GetInstanceId());
			if (!job.upCharacter)
				return;
			if (!m_moveQueue.Finish(GetInstanceId(), job.sessionId))
			{
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_discarded map=%llu thread_id=%lu session_id=%llu character_id=%llu | 끊어진 인계를 정리했습니다.",
					GetInstanceId(), GetCurrentThreadId(), job.sessionId, job.upCharacter->characterId);
				return;
			}

			const auto entry = m_characters.emplace(job.sessionId, std::move(job.upCharacter)).first;
			m_emptySince = {};
			g_pLogger->WriteLogConsole(ELogLevel::Debug,
				L"event=move_attached map=%llu thread_id=%lu session_id=%llu character_id=%llu | 캐릭터를 등록했습니다.",
				GetInstanceId(), GetCurrentThreadId(), job.sessionId, entry->second->characterId);
			sendMoveResult(job.sessionId, *entry->second, EMoveResult::Moved);
		}

		void sendMoveResult(UINT64 sessionId, const SCharacter &character, EMoveResult result) noexcept
		{
			(void)SendResMove(sessionId, GetInstanceId(), character.value, static_cast<std::uint16_t>(result));
		}

		bool HandleReqMove(UINT64 sessionId, const SReqMove &packet) noexcept override
		{
			auto &character = *m_characters.find(sessionId)->second;
			const auto targetMapId = packet.m_targetMapId;
			if ((targetMapId != kFirstMapId && targetMapId != kSecondMapId) || targetMapId == GetInstanceId())
				sendMoveResult(sessionId, character, EMoveResult::InvalidTarget);
			else
			{
				character.targetMapId = targetMapId;
				if (character.pendingChange != 0)
					g_pLogger->WriteLogConsole(ELogLevel::Debug,
						L"event=move_waiting_for_change map=%llu thread_id=%lu session_id=%llu character_id=%llu"
						L" | 상태 변경 완료를 기다립니다.", GetInstanceId(), GetCurrentThreadId(), sessionId, character.characterId);
			}
			return true;
		}

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
		const UINT64 m_mapId;
		CServer &m_network;
		CMoveQueue &m_moveQueue;
		CMapHost &m_host;
		const bool m_usesTestControls;
		std::unordered_map<UINT64, std::unique_ptr<SCharacter>> m_characters;
		std::chrono::steady_clock::time_point m_emptySince{};
		UINT64 m_total = 0;
		bool m_isAdmissionClosed = false;
		bool m_hasLoggedCloseDeferred = false;
	};

	// 런타임에는 호스트만 남는다. 실제 맵은 담당 워커에서만 접근하고 파괴한다.
	class CMapHost final : public IContents
	{
	public:
		CMapHost(UINT64 mapId, CServer &network, CMoveQueue &moveQueue, CContentsRuntime &runtime, bool usesTestControls,
			std::chrono::milliseconds idleGrace)
			: IContents(mapId)
			, m_moveQueue(moveQueue)
			, m_runtime(runtime)
			, m_idleGrace(idleGrace)
			, m_upMap(std::make_unique<CMapInstance>(mapId, network, moveQueue, *this, usesTestControls))
		{
		}

		void AddConnection(UINT64 sessionId, UINT64 characterId)
		{
			if (m_upMap)
				m_upMap->AddConnection(sessionId, characterId);
		}

		void RemoveConnection(UINT64 sessionId) noexcept
		{
			if (m_upMap)
				m_upMap->RemoveConnection(sessionId);
		}

		bool PostChange(UINT64 sessionId)
		{
			return m_runtime.PostCompletion(GetInstanceId(), [this, sessionId]
			{
				if (m_upMap)
					m_upMap->completeChange(sessionId);
				else
					g_pLogger->WriteLogConsole(ELogLevel::Debug,
						L"event=map_late_change_ignored map=%llu thread_id=%lu | 파괴된 맵의 상태 변경 완료를 무시했습니다.",
						GetInstanceId(), GetCurrentThreadId());
			});
		}

	protected:
		void OnFrame() override
		{
			if (!m_upMap)
				return;
			m_upMap->RunFrame();
			if (m_idleGrace == 0ms || !m_upMap->HasIdleGraceElapsed(m_idleGrace))
			{
				m_hasLoggedDestroyDeferred = false;
				return;
			}
			if (m_moveQueue.TryClose(GetInstanceId()))
			{
				m_upMap.reset();
				// 파괴 직후 기존 완료 경로에 테스트 콜백을 한 건 접수한다.
				if (GetFileAttributesW(L"post-change-after-destroy") != INVALID_FILE_ATTRIBUTES)
					(void)PostChange(0);
			}
			else if (!m_hasLoggedDestroyDeferred)
			{
				m_hasLoggedDestroyDeferred = true;
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=map_destroy_deferred map=%llu | 예약 또는 접속 경로가 남아 파괴를 미룹니다.", GetInstanceId());
			}
		}

	private:
		CMoveQueue &m_moveQueue;
		CContentsRuntime &m_runtime;
		const std::chrono::milliseconds m_idleGrace;
		std::unique_ptr<CMapInstance> m_upMap;
		bool m_hasLoggedDestroyDeferred = false;
	};

	bool CMapInstance::postChange(UINT64 sessionId)
	{
		return m_host.PostChange(sessionId);
	}

	class CFixedMapServer final : public CServer
	{
	public:
		[[nodiscard]]
		bool Run(USHORT port, bool usesTestControls, std::chrono::milliseconds idleGrace)
		{
			for (std::size_t index = 0; index < m_mapHosts.size(); ++index)
			{
				const auto mapId = index + kFirstMapId;
				auto upHost = std::make_unique<CMapHost>(mapId, *this, m_moveQueue, m_runtime, usesTestControls,
					mapId == kSecondMapId ? idleGrace : 0ms);
				m_mapHosts[index] = upHost.get();
				if (!m_runtime.RegisterContents(std::move(upHost), true, index))
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
			// 최초 배정에만 접속 번호의 홀짝을 사용한다.
			const auto mapId = (sessionId & 1u) != 0 ? kFirstMapId : kSecondMapId;
			const auto characterId = m_moveQueue.TryRegister(sessionId, mapId);
			if (characterId == 0)
			{
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=map_admission_rejected map=%llu session_id=%llu | 접수가 종료된 맵의 연결을 거절했습니다.",
					mapId, sessionId);
				Disconnect(sessionId);
				return;
			}
			auto *pHost = m_mapHosts[mapId - kFirstMapId];
			if (!m_runtime.PostCompletion(mapId, [pHost, sessionId, characterId]
			{
				pHost->AddConnection(sessionId, characterId);
			}))
			{
				(void)m_moveQueue.Leave(sessionId);
				Disconnect(sessionId);
			}
		}

		void OnClientLeave(UINT64 sessionId) noexcept override
		{
			const auto mapId = m_moveQueue.Leave(sessionId);
			if (mapId == 0)
			{
				g_pLogger->WriteLogConsole(ELogLevel::Debug,
					L"event=move_connection_left session_id=%llu | 활성 맵이 없는 연결이 종료되었습니다.", sessionId);
				return;
			}
			auto *pHost = m_mapHosts[mapId - kFirstMapId];
			(void)m_runtime.PostCompletion(mapId, [pHost, sessionId]
			{
				pHost->RemoveConnection(sessionId);
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

	private:

		CMoveQueue m_moveQueue;
		CContentsRuntime m_runtime{2, 5ms};
		std::array<CMapHost *, 2> m_mapHosts{};
	};


	// 맵 응답의 결과 범위를 확인한 뒤 클라이언트 콜백을 실행한다.
	bool WorldClient::HandleResAdd(const SResAdd &packet) noexcept
	{
		if (packet.m_result > static_cast<std::uint16_t>(EAddResult::InvalidRequest))
			return false;
		return onAddResponse(packet);
	}

	bool WorldClient::HandleResMove(const SResMove &packet) noexcept
	{
		return packet.m_result <= static_cast<std::uint16_t>(EMoveResult::InvalidRequest);
	}

	bool WorldClient::HandleResChange(const SResChange &packet) noexcept
	{
		return packet.m_result <= static_cast<std::uint16_t>(EAddResult::InvalidRequest);
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
	INT64 controls = 0;
	INT64 destroyIdleMilliseconds = 0;
	std::wstring role;
	loader.Load(L"FixedMaps", L"Controls", &controls);
	loader.Load(L"FixedMaps", L"DestroyIdleMilliseconds", &destroyIdleMilliseconds);
	if (!loader.Load(L"FixedMaps", L"Role", &role) || !loader.Load(L"FixedMaps", L"Port", &configuredPort)
		|| configuredPort <= 0 || configuredPort > USHRT_MAX || (role != L"Server" && role != L"Client")
		|| (controls != 0 && controls != 1) || destroyIdleMilliseconds < 0 || destroyIdleMilliseconds > 60'000
		|| (destroyIdleMilliseconds != 0 && (controls != 1 || role != L"Server")))
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
	ExitProcess(server.Run(port, controls == 1, std::chrono::milliseconds{destroyIdleMilliseconds}) ? 0 : 1);
}
