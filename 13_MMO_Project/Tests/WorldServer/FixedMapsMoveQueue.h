#pragma once

namespace WorldServer
{
	struct SCharacter
	{
		const UINT64 characterId;
		UINT64 value = 0;
		UINT64 targetMapId = 0;
		std::uint32_t pendingChange = 0;
		bool isChangePosted = false;
	};

	struct SMoveJob
	{
		UINT64 sessionId = 0;
		std::unique_ptr<SCharacter> upCharacter;
	};

	// 신규 접속과 이동 예약은 맵 접수 종료와 같은 락에서 순서를 결정한다.
	class CMoveQueue
	{
	public:
		UINT64 TryRegister(UINT64 sessionId, UINT64 mapId)
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			if (!m_inboxes[mapId - kFirstMapId].isOpen)
				return 0;
			m_routes.emplace(sessionId, mapId);
			return ++m_nextCharacterId;
		}

		UINT64 Leave(UINT64 sessionId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			const auto route = m_routes.find(sessionId);
			if (route == m_routes.end())
				return 0;
			const auto mapId = route->second;
			m_routes.erase(route);
			return mapId;
		}

		// 성공 시 출발 워커는 같은 실행 경계에서 분리·게시를 끝낸다.
		EMoveResult Reserve(UINT64 sessionId, UINT64 targetMapId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			const auto route = m_routes.find(sessionId);
			if (route == m_routes.end())
				return EMoveResult::Disconnected;
			auto &inbox = m_inboxes[targetMapId - kFirstMapId];
			if (!inbox.isOpen)
				return EMoveResult::Closed;
			if (inbox.isReserved)
				return EMoveResult::Busy;
			inbox.isReserved = true;
			route->second = 0; // 이후 연결 종료는 게시된 인계를 목적지에서 정리한다.
			return EMoveResult::Moved;
		}

		bool IsOnMap(UINT64 sessionId, UINT64 mapId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			const auto route = m_routes.find(sessionId);
			return route != m_routes.end() && route->second == mapId;
		}

		void Publish(UINT64 targetMapId, SMoveJob job) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			m_inboxes[targetMapId - kFirstMapId].job = std::move(job);
		}

		SMoveJob Take(UINT64 mapId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			return std::exchange(m_inboxes[mapId - kFirstMapId].job, SMoveJob{});
		}

		// Take 이후에도 예약을 유지하고, 연결 종료와 등록의 순서를 이 락에서 결정한다.
		bool Finish(UINT64 mapId, UINT64 sessionId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			m_inboxes[mapId - kFirstMapId].isReserved = false;
			const auto route = m_routes.find(sessionId);
			if (route == m_routes.end())
				return false;
			route->second = mapId;
			return true;
		}

		bool TryClose(UINT64 mapId) noexcept
		{
			std::lock_guard moveQueueLock(m_moveQueueLock);
			auto &inbox = m_inboxes[mapId - kFirstMapId];
			if (inbox.isReserved)
				return false;
			// 아직 맵에 반영되지 않은 최초 접속도 경로를 먼저 등록한다.
			for (const auto &route : m_routes)
			{
				if (route.second == mapId)
					return false;
			}
			inbox.isOpen = false;
			return true;
		}

	private:
		struct SInbox
		{
			bool isOpen = true;
			bool isReserved = false;
			SMoveJob job;
		};

	private:
		std::mutex m_moveQueueLock;
		std::array<SInbox, 2> m_inboxes{};
		std::unordered_map<UINT64, UINT64> m_routes;
		UINT64 m_nextCharacterId = 0;
	};
}
