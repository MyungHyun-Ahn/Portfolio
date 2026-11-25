#include "MHLib/MHLibPch.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Server/GameDataAll.g.h"
#include "Client/GameDataAll.g.h"
#include "CTestGameDataTable.h"

namespace
{
	using GameData::EGameDataReadResult;
	using GameData::IGameDataTable;
	using CServerTable = CTestGameDataTable<GameData::Server::Player::CPlayerMovementTableBase>;
	using CClientTable = CTestGameDataTable<GameData::Client::Player::CPlayerMovementTableBase>;

	static_assert(std::is_abstract_v<GameData::Server::Player::CPlayerMovementTableBase>);
	static_assert(std::is_abstract_v<GameData::Client::Player::CPlayerMovementTableBase>);
	static_assert(std::is_same_v<decltype(&IGameDataTable::Init), bool (IGameDataTable::*)()>);

	struct SFailureCase
	{
		const char* m_pPath;
		EGameDataReadResult m_result;
	};

	constexpr SFailureCase kFailures[] =
	{
		{ "missing.sdat", EGameDataReadResult::OpenFailed },
		{ "magic.sdat", EGameDataReadResult::InvalidMagic },
		{ "version.sdat", EGameDataReadResult::UnsupportedVersion },
		{ "Data/Client/Player/PlayerMovement.cdat", EGameDataReadResult::WrongTarget },
		{ "table-name.sdat", EGameDataReadResult::WrongTable },
		{ "table-length.sdat", EGameDataReadResult::WrongTable },
		{ "schema.sdat", EGameDataReadResult::WrongSchema },
		{ "short-header.sdat", EGameDataReadResult::Truncated },
		{ "short-row.sdat", EGameDataReadResult::Truncated },
		{ "row-count.sdat", EGameDataReadResult::Truncated },
		{ "trailing.sdat", EGameDataReadResult::TrailingBytes },
		{ "zero-key.sdat", EGameDataReadResult::InvalidPrimaryKey },
		{ "duplicate-key.sdat", EGameDataReadResult::DuplicatePrimaryKey },
	};

	// 판독에 실패해도 이전에 읽은 키와 값은 그대로 조회된다.
	[[nodiscard]]
	bool HasUpdatedRows(const CServerTable& table)
	{
		const auto* pFirst = table.FindLocal(258);
		const auto* pSecond = table.FindLocal(1);
		return pFirst != nullptr && pFirst->GetMoveSpeedCmPerSecond() == 600 && pFirst->GetCollisionRadiusCm() == 70 &&
			pSecond != nullptr && pSecond->GetMoveSpeedCmPerSecond() == 451 && pSecond->GetCollisionRadiusCm() == 35 &&
			table.FindLocal(2) == nullptr;
	}
}

// 대상별 판독과 실패 시 기존 행 보존을 실제 파일로 확인한다.
int main(void)
{
	MHLib::utils::CLogger::GetInstance()->SetMainDirectory(L"ReaderLogs");
	constexpr const char* kServerPath = "Data/Server/Player/PlayerMovement.sdat";
	constexpr const char* kClientPath = "Data/Client/Player/PlayerMovement.cdat";
	CServerTable server;
	CClientTable client;
	IGameDataTable& serverTable = server;
	IGameDataTable& clientTable = client;
	if (!serverTable.Load(kServerPath) || !clientTable.Load(kClientPath))
	{
		return 1;
	}
	if (!serverTable.Init() || !clientTable.Init() || !serverTable.Validate() || !clientTable.Validate())
	{
		return 2;
	}
	const auto* pServerRow = server.FindLocal(1);
	const auto* pClientRow = client.FindLocal(1);
	if (pServerRow == nullptr || pServerRow->GetMoveSpeedCmPerSecond() != 450 || pServerRow->GetCollisionRadiusCm() != 35 ||
		pClientRow == nullptr || pClientRow->GetVisualScalePermille() != 1000 || server.FindLocal(2) != nullptr || client.FindLocal(2) != nullptr)
	{
		return 3;
	}
	if (clientTable.Load(kServerPath) || client.GetLastReadResult() != EGameDataReadResult::WrongTarget || client.FindLocal(1) != pClientRow)
	{
		return 4;
	}
	if (!serverTable.Load("updated.sdat") || !HasUpdatedRows(server))
	{
		return 5;
	}
	if (!clientTable.Load("updated.cdat"))
	{
		return 6;
	}
	const auto* pUpdatedClientFirst = client.FindLocal(258);
	const auto* pUpdatedClientSecond = client.FindLocal(1);
	if (pUpdatedClientFirst == nullptr || pUpdatedClientFirst->GetVisualScalePermille() != 1200 ||
		pUpdatedClientSecond == nullptr || pUpdatedClientSecond->GetVisualScalePermille() != 1100 || client.FindLocal(2) != nullptr)
	{
		return 7;
	}
	int caseNumber = 10;
	for (const SFailureCase& testCase : kFailures)
	{
		if (serverTable.Load(testCase.m_pPath) || server.GetLastReadResult() != testCase.m_result || !HasUpdatedRows(server))
		{
			return caseNumber;
		}
		++caseNumber;
	}
	GameData::Server::Player::CPlayerMovementTable candidate;
	IGameDataTable& candidateTable = candidate;
	if (!candidateTable.Load("empty.sdat") || !candidateTable.Init() || candidateTable.Validate() || !HasUpdatedRows(server))
	{
		return 8;
	}
	// 실제 수기 테이블을 사용해 묶음을 초기화하고 정적 조회가 해당 묶음의 행만 보는지 확인한다.
	const auto pServerTables = GameData::Server::CGameDataTables::Load("Data/Server");
	const auto pClientTables = GameData::Client::CGameDataTables::Load("Data/Client");
	const auto* pPublishedServer = GameData::Server::Player::CPlayerMovementTableBase::Find(1);
	const auto* pPublishedClient = GameData::Client::Player::CPlayerMovementTableBase::Find(1);
	if (!pServerTables || !pClientTables || pPublishedServer == nullptr || pPublishedServer->GetMoveSpeedCmPerSecond() != 450 ||
		pPublishedClient == nullptr || pPublishedClient->GetVisualScalePermille() != 1000 ||
		GameData::Server::Player::CPlayerMovementTableBase::Find(258) != nullptr)
	{
		return 9;
	}
	return 0;
}