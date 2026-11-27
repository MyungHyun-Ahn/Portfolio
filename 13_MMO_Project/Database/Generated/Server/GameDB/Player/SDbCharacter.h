// DBGenerator 생성 파일. 직접 수정하지 않는다.
// 원본: YamlSchema/GameDB/Player/Character.yaml
#pragma once

namespace Database::GameDB::Player
{
struct SDbCharacter
{
	std::uint64_t m_characterId{};
	std::uint64_t m_accountId{};
	std::optional<std::uint32_t> m_lastLogoutMap{};
};
}
