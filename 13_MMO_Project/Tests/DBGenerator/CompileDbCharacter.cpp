#include <Windows.h>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "../../Database/Generated/Server/GameDB/Player/SDbCharacter.h"

using Database::GameDB::Player::SDbCharacter;

static_assert(std::is_aggregate_v<SDbCharacter>);
static_assert(std::is_same_v<decltype(SDbCharacter::m_characterId), std::uint64_t>);
static_assert(std::is_same_v<decltype(SDbCharacter::m_accountId), std::uint64_t>);
static_assert(std::is_same_v<decltype(SDbCharacter::m_lastLogoutMap), std::optional<std::uint32_t>>);
static_assert(!SDbCharacter{}.m_lastLogoutMap.has_value());

constexpr SDbCharacter kSavedCharacter{7, 11, 123};
static_assert(kSavedCharacter.m_characterId == 7);
static_assert(kSavedCharacter.m_accountId == 11);
static_assert(kSavedCharacter.m_lastLogoutMap.value() == 123);
