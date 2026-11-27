// DBGenerator 생성 파일. 직접 수정하지 않는다.
// 원본: YamlSchema/
#pragma once

// 원본: YamlSchema/GameDB/Player/Character.yaml
namespace Database::GameDB::Player
{
[[nodiscard]]
inline ::Game::SCharacterData ToCharacterData(const SDbCharacter &record) noexcept
{
	::Game::SCharacterData data{};
	data.m_characterId = record.m_characterId;
	data.m_lastLogoutMap = record.m_lastLogoutMap.value_or(0);
	return data;
}
}
