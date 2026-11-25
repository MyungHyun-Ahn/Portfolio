#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <Windows.h>

#include "GameData/IGameDataTable.h"
#include "GameData/CGameDataReader.h"
#include "Common/GameDataEnums.h"
#include "Server/Player/PlayerMovement.h"
#include "Client/Player/PlayerMovement.h"
#include "Server/Player/PlayerMovement.TableBase.g.h"
#include "Client/Player/PlayerMovement.TableBase.g.h"
#include "CTestGameDataTable.h"

using SServerRow = GameData::Server::Player::SPlayerMovementRow;
using SClientRow = GameData::Client::Player::SPlayerMovementRow;
namespace ServerFormat = GameData::Server::Player::PlayerMovementFormat;
namespace ClientFormat = GameData::Client::Player::PlayerMovementFormat;

static_assert(std::has_virtual_destructor_v<GameData::IGameDataTable>);
static_assert(std::is_abstract_v<GameData::IGameDataTable>);
static_assert(std::is_same_v<decltype(&GameData::IGameDataTable::Load), bool (GameData::IGameDataTable::*)(const std::filesystem::path&)>);
static_assert(std::is_same_v<decltype(&GameData::IGameDataTable::Init), bool (GameData::IGameDataTable::*)()>);
static_assert(std::is_same_v<decltype(&GameData::IGameDataTable::Validate), bool (GameData::IGameDataTable::*)() const>);
static_assert(std::is_aggregate_v<SServerRow> && std::is_aggregate_v<SClientRow>);
static_assert(ServerFormat::kFormatVersion == 1 && ClientFormat::kFormatVersion == 1);
static_assert(ServerFormat::kTarget == 1 && ClientFormat::kTarget == 2);
static_assert(sizeof(ServerFormat::kSchemaHash) == 32 && sizeof(ClientFormat::kSchemaHash) == 32);
static_assert(std::is_same_v<decltype(SServerRow::m_playerMovementDataId), std::uint32_t>);
static_assert(std::is_same_v<decltype(SServerRow::m_moveSpeedCmPerSecond), std::vector<GameData::DamageType>>);
static_assert(std::is_same_v<decltype(SServerRow::m_collisionRadiusCm), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(SClientRow::m_visualScalePermille), std::optional<std::vector<float>>>);
static_assert(static_cast<std::uint32_t>(GameData::DamageType::Fire) == 9);
static_assert(std::is_same_v<decltype(&SServerRow::GetPlayerMovementDataId), std::uint32_t (SServerRow::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&SClientRow::GetPlayerMovementDataId), std::uint32_t (SClientRow::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&SServerRow::GetMoveSpeedCmPerSecond),
	const std::vector<GameData::DamageType>& (SServerRow::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&SServerRow::GetCollisionRadiusCm), const std::optional<std::string>& (SServerRow::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&SClientRow::GetVisualScalePermille),
	const std::optional<std::vector<float>>& (SClientRow::*)() const noexcept>);

// 데이터가 섞인 테이블을 생성 Load로 읽고 선택값 및 배열의 상태를 확인한다.
[[nodiscard]]
bool CheckGeneratedLoad()
{
	CTestGameDataTable<GameData::Server::Player::CPlayerMovementTableBase> server;
	CTestGameDataTable<GameData::Client::Player::CPlayerMovementTableBase> client;
	if (!server.Load("Generated/Data/Server/Player/PlayerMovement.sdat") || !client.Load("Generated/Data/Client/Player/PlayerMovement.cdat"))
	{
		return false;
	}
	const auto* pServer = server.FindLocal(1);
	const auto* pClientEmpty = client.FindLocal(1);
	const auto* pClientMissing = client.FindLocal(2);
	const auto* pClientPopulated = client.FindLocal(3);
	if (pServer == nullptr || pClientEmpty == nullptr || pClientMissing == nullptr || pClientPopulated == nullptr)
	{
		return false;
	}
	if (pServer->GetSignedDelta() != -2147483647 ||
		pServer->GetMoveSpeedCmPerSecond() != std::vector{ GameData::DamageType::Fire, GameData::DamageType::Ice } ||
		pServer->GetCollisionRadiusCm() != "검, \"방패\"\n2행" || !pClientEmpty->GetVisualScalePermille().has_value() ||
		!pClientEmpty->GetVisualScalePermille()->empty() || pClientMissing->GetVisualScalePermille().has_value() ||
		pClientPopulated->GetVisualScalePermille() != std::vector{ -1.5f, 2.25f })
	{
		return false;
	}
	return !server.Load("invalid-utf8.sdat") && server.GetLastReadResult() == GameData::EGameDataReadResult::InvalidValue &&
		!server.Load("invalid-string-size.sdat") && server.GetLastReadResult() == GameData::EGameDataReadResult::Truncated &&
		!client.Load("invalid-presence.cdat") && client.GetLastReadResult() == GameData::EGameDataReadResult::InvalidValue &&
		!client.Load("invalid-array-size.cdat") && client.GetLastReadResult() == GameData::EGameDataReadResult::Truncated &&
		!client.Load("invalid-float.cdat") && client.GetLastReadResult() == GameData::EGameDataReadResult::InvalidValue &&
		server.FindLocal(1) == pServer && client.FindLocal(3) == pClientPopulated;
}

// 양쪽 행에서 값과 원본 멤버 참조를 돌려주는지 확인한다.
int main(void)
{
	const SServerRow serverRow{ 7, { GameData::DamageType::Fire }, std::nullopt, -2 };
	const SClientRow clientRow{ 9, std::vector<float>{ 2.25f } };
	return CheckGeneratedLoad() && serverRow.GetPlayerMovementDataId() == 7 && clientRow.GetPlayerMovementDataId() == 9 &&
		&serverRow.GetMoveSpeedCmPerSecond() == &serverRow.m_moveSpeedCmPerSecond &&
		&serverRow.GetCollisionRadiusCm() == &serverRow.m_collisionRadiusCm && !serverRow.GetCollisionRadiusCm().has_value() &&
		&clientRow.GetVisualScalePermille() == &clientRow.m_visualScalePermille && clientRow.GetVisualScalePermille().has_value() ? 0 : 1;
}
