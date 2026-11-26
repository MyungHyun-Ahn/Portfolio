#pragma once

namespace GameData::Server
{
	class CGameDataTables
	{
		friend class ::GameData::Server::World::CMapTableBase;
		friend class ::GameData::Server::Player::CPlayerMovementTableBase;

	public:
		// 소유한 테이블이 사라지기 전에 정적 조회를 해제한다.
		~CGameDataTables()
		{
			m_hasLoadedTables = false;
			m_tables = {};
			sm_pTables = nullptr;
		}

		// 워커 시작 전에 전체 Load, 전체 Init, 전체 Validate를 순서대로 실행한다.
		[[nodiscard]]
		static std::unique_ptr<CGameDataTables> Load(const std::filesystem::path& dataDirectory)
		{
			using ::MHLib::utils::CLogger;
			using ::MHLib::utils::ELogLevel;

			auto* pLogger = CLogger::GetInstance();
			if (dataDirectory.empty() || sm_pTables != nullptr)
			{
				pLogger->WriteLog(ELogLevel::Error, L"GameData", 0, std::source_location::current(),
					L"event=game_data_bundle_rejected target=Server reason=%ls | 게임 데이터 로드를 시작할 수 없습니다.",
					dataDirectory.empty() ? L"empty_directory" : L"bundle_already_exists");
				return nullptr;
			}

			auto pTables = std::unique_ptr<CGameDataTables>(new CGameDataTables());
			sm_pTables = pTables.get();
			pTables->m_tables =
			{{
				{ "World/Map.sdat",
					std::make_unique<::GameData::Server::World::CMapTable>() },
				{ "Player/PlayerMovement.sdat",
					std::make_unique<::GameData::Server::Player::CPlayerMovementTable>() },
			}};

			for (const auto& table : pTables->m_tables)
			{
				if (!table.m_pTable->Load(dataDirectory / table.m_pRelativePath))
				{
					pLogger->WriteLog(ELogLevel::Error, L"GameData", 0, std::source_location::current(),
						L"event=game_data_table_failed target=Server stage=load file=%hs | 게임 데이터 로드 실패.",
						table.m_pRelativePath);
					return nullptr;
				}
				pLogger->WriteLog(ELogLevel::Debug, L"GameData", 0, std::source_location::current(),
					L"event=game_data_table_completed target=Server stage=load file=%hs | 게임 데이터 로드 완료.",
					table.m_pRelativePath);
			}
			pTables->m_hasLoadedTables = true;
			pLogger->WriteLog(ELogLevel::Info, L"GameData", 0, std::source_location::current(),
				L"event=game_data_stage_completed target=Server stage=load tables=%zu | 전체 게임 데이터 로드 성공.",
				pTables->m_tables.size());

			for (const auto& table : pTables->m_tables)
			{
				if (!table.m_pTable->Init())
				{
					pLogger->WriteLog(ELogLevel::Error, L"GameData", 0, std::source_location::current(),
						L"event=game_data_table_failed target=Server stage=init file=%hs | 게임 데이터 초기화 실패.",
						table.m_pRelativePath);
					return nullptr;
				}
				pLogger->WriteLog(ELogLevel::Debug, L"GameData", 0, std::source_location::current(),
					L"event=game_data_table_completed target=Server stage=init file=%hs | 게임 데이터 초기화 완료.",
					table.m_pRelativePath);
			}
			pLogger->WriteLog(ELogLevel::Info, L"GameData", 0, std::source_location::current(),
				L"event=game_data_stage_completed target=Server stage=init tables=%zu | 전체 게임 데이터 초기화 성공.",
				pTables->m_tables.size());

			for (const auto& table : pTables->m_tables)
			{
				if (!table.m_pTable->Validate())
				{
					pLogger->WriteLog(ELogLevel::Error, L"GameData", 0, std::source_location::current(),
						L"event=game_data_table_failed target=Server stage=validate file=%hs | 게임 데이터 검증 실패.",
						table.m_pRelativePath);
					return nullptr;
				}
				pLogger->WriteLog(ELogLevel::Debug, L"GameData", 0, std::source_location::current(),
					L"event=game_data_table_completed target=Server stage=validate file=%hs | 게임 데이터 검증 완료.",
					table.m_pRelativePath);
			}
			pLogger->WriteLog(ELogLevel::Info, L"GameData", 0, std::source_location::current(),
				L"event=game_data_stage_completed target=Server stage=validate tables=%zu | 전체 게임 데이터 검증 성공.",
				pTables->m_tables.size());

			return pTables;
		}

	private:
		CGameDataTables() = default;

		// 전체 로드가 끝난 묶음에서만 생성된 배열 위치로 테이블을 찾는다.
		[[nodiscard]]
		static const IGameDataTable* getTable(std::size_t index) noexcept
		{
			if (sm_pTables == nullptr || !sm_pTables->m_hasLoadedTables)
			{
				return nullptr;
			}
			return sm_pTables->m_tables[index].m_pTable.get();
		}

	private:
		struct STableEntry
		{
			const char* m_pRelativePath;
			std::unique_ptr<IGameDataTable> m_pTable;
		};

		inline static const CGameDataTables* sm_pTables = nullptr;
		std::array<STableEntry, 2> m_tables{};
		bool m_hasLoadedTables = false;
	};
}

namespace GameData::Server::World
{
	inline const SMapRow* CMapTableBase::Find(std::uint32_t id)
	{
		const auto* pTable = static_cast<const CMapTableBase*>(
			::GameData::Server::CGameDataTables::getTable(0));
		if (pTable == nullptr)
		{
			return nullptr;
		}
		const auto iter = pTable->m_rows.find(id);
		return iter == pTable->m_rows.end() ? nullptr : &iter->second;
	}
}

namespace GameData::Server::Player
{
	inline const SPlayerMovementRow* CPlayerMovementTableBase::Find(std::uint32_t id)
	{
		const auto* pTable = static_cast<const CPlayerMovementTableBase*>(
			::GameData::Server::CGameDataTables::getTable(1));
		if (pTable == nullptr)
		{
			return nullptr;
		}
		const auto iter = pTable->m_rows.find(id);
		return iter == pTable->m_rows.end() ? nullptr : &iter->second;
	}
}
