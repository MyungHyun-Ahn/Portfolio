#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <Windows.h>

#include "GameData/IGameDataTable.h"
#include "Common/GameDataEnums.h"
#include "Server/PlayerMovement.h"
#include "Client/PlayerMovement.h"

using SServerRow = GameData::Server::SPlayerMovementRow;
using SClientRow = GameData::Client::SPlayerMovementRow;
namespace ServerFormat = GameData::Server::PlayerMovementFormat;
namespace ClientFormat = GameData::Client::PlayerMovementFormat;

static_assert(std::has_virtual_destructor_v<GameData::IGameDataTable>);
static_assert(std::is_abstract_v<GameData::IGameDataTable>);
static_assert(std::is_same_v<decltype(&GameData::IGameDataTable::Load), bool (GameData::IGameDataTable::*)(const std::filesystem::path&)>);
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

// 양쪽 행에서 값과 원본 멤버 참조를 돌려주는지 확인한다.
int main(void)
{
	const SServerRow serverRow{ 7, { GameData::DamageType::Fire }, std::nullopt };
	const SClientRow clientRow{ 9, std::vector<float>{ 2.25f } };
	return serverRow.GetPlayerMovementDataId() == 7 && clientRow.GetPlayerMovementDataId() == 9 &&
		&serverRow.GetMoveSpeedCmPerSecond() == &serverRow.m_moveSpeedCmPerSecond &&
		&serverRow.GetCollisionRadiusCm() == &serverRow.m_collisionRadiusCm && !serverRow.GetCollisionRadiusCm().has_value() &&
		&clientRow.GetVisualScalePermille() == &clientRow.m_visualScalePermille && clientRow.GetVisualScalePermille().has_value() ? 0 : 1;
}
