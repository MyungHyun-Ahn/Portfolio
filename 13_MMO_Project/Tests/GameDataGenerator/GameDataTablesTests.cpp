#include "MHLib/MHLibPch.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	int g_hookOrder = 0;
	int g_failAt = 0;
	int g_destroyedTables = 0;
	bool g_foundRowsOutsideLookupWindow = false;
	void CheckRowsUnavailable();
}

#include "Server/GameDataAll.g.h"
#include "Client/GameDataAll.g.h"

namespace
{
	// 수기 테이블의 생성·소멸 시점에는 묶음의 행을 조회할 수 없어야 한다.
	void CheckRowsUnavailable()
	{
		if (GameData::Server::Player::CPlayerMovementTableBase::Find(1) != nullptr ||
			GameData::Server::STableEntry::CServerOnlyTableBase::Find(1) != nullptr)
		{
			g_foundRowsOutsideLookupWindow = true;
		}
	}
}

// 파일 목록을 별도로 전달하지 않고 대상별 묶음을 읽는다.
static int RunTableCases()
{
	using CServerTables = GameData::Server::CGameDataTables;
	using CClientTables = GameData::Client::CGameDataTables;
	using CServerBase = GameData::Server::Player::CPlayerMovementTableBase;
	using CServerOnlyBase = GameData::Server::STableEntry::CServerOnlyTableBase;
	using CClientBase = GameData::Client::Player::CPlayerMovementTableBase;
	static_assert(std::is_same_v<decltype(CServerBase::Find(1)), const GameData::Server::Player::SPlayerMovementRow*>);
	static_assert(std::is_same_v<decltype(&CServerTables::Load),
		std::unique_ptr<CServerTables> (*)(const std::filesystem::path&)>);
	const std::filesystem::path serverDirectory = "Server data";
	const std::filesystem::path clientDirectory = "Client data";
	const auto secondFile = serverDirectory / "STableEntry/ServerOnly.sdat";
	std::error_code error;
	if (CServerBase::Find(1) != nullptr || CServerOnlyBase::Find(1) != nullptr || CClientBase::Find(1) != nullptr)
	{
		return 1;
	}
	if (CClientTables::Load({}) != nullptr)
	{
		return 15;
	}
	// 두 번째 파일의 로드 실패는 어느 테이블의 Init도 호출하지 않는다.
	const auto backup = serverDirectory / "ServerOnly.backup";
	std::filesystem::rename(secondFile, backup, error);
	if (error || CServerTables::Load(serverDirectory) != nullptr ||
		g_hookOrder != 0 || CServerBase::Find(1) != nullptr)
	{
		return 2;
	}
	std::filesystem::rename(backup, secondFile, error);
	if (error)
	{
		return 3;
	}
	// 두 번째 Init 또는 마지막 Validate가 실패하면 정적 조회도 모두 해제된다.
	for (int failAt : { 2, 4 })
	{
		g_hookOrder = 0;
		g_failAt = failAt;
		if (CServerTables::Load(serverDirectory) != nullptr ||
			g_hookOrder != failAt || CServerBase::Find(1) != nullptr || CServerOnlyBase::Find(1) != nullptr)
		{
			return 4 + failAt;
		}
	}
	g_hookOrder = 0;
	g_failAt = 0;
	auto pServer = CServerTables::Load(serverDirectory);
	auto pClient = CClientTables::Load(clientDirectory);
	if (pServer == nullptr || pClient == nullptr || g_hookOrder != 4)
	{
		return 9;
	}
	const auto* pSharedRow = CServerBase::Find(1);
	const auto* pServerOnlyRow = CServerOnlyBase::Find(1);
	const auto* pClientRow = CClientBase::Find(1);
	if (pSharedRow == nullptr || pSharedRow->GetMoveSpeedCmPerSecond() != 450 ||
		pServerOnlyRow == nullptr || pServerOnlyRow->GetMoveSpeedCmPerSecond() != 725 ||
		pClientRow == nullptr || pClientRow->GetVisualScalePermille() != 1000)
	{
		return 10;
	}
	if (CServerBase::Find(2) != nullptr || CServerTables::Load(serverDirectory) != nullptr || CServerBase::Find(1) != pSharedRow)
	{
		return 11;
	}
	pServer.reset();
	if (CServerBase::Find(1) != nullptr || CServerOnlyBase::Find(1) != nullptr || CClientBase::Find(1) != pClientRow)
	{
		return 12;
	}
	if (g_foundRowsOutsideLookupWindow || g_destroyedTables != 4)
	{
		return 13;
	}
	g_hookOrder = 0;
	pServer = CServerTables::Load(serverDirectory);
	if (!pServer || g_hookOrder != 4 || CServerOnlyBase::Find(1) == nullptr)
	{
		return 14;
	}
	return 0;
}

// 실제 로거의 콘솔·파일 레벨 분리와 종료 전 큐 비우기를 함께 확인한다.
int main(void)
{
	using MHLib::utils::CLogger;
	using MHLib::utils::ELogLevel;
	auto* pLogger = CLogger::GetInstance();
	if (!pLogger->Start(L"GameDataLogs", ELogLevel::Info, ELogLevel::Debug))
	{
		return 20;
	}
	pLogger->RegisterLogProducer();
	const int result = RunTableCases();
	pLogger->Stop();
	return result;
}