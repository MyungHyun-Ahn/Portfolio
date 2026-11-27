#include <Windows.h>
#include <array>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>

namespace MHLib::memory
{
	// 버퍼 헤더의 정적 풀 선언에 필요한 타입만 제공한다. 이 시험은 풀을 사용하지 않는다.
	template <typename T, int bucketSize, int bucketCount, bool useQueue = true>
	class CTlsMemoryPoolManager;

	template <int sizeBytes, int bucketCount, int pageLock>
	class CTlsPagePoolManager;
}

#include "MHLib/utils/CSmartPtr.h"
#include "MHLib/serialization/ISerializable.h"
#include "PacketHeaders.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "../../Common/Packet/Game/Generated/Common/Player/CharacterData.g.h"
#include "../../Database/Generated/Server/IncludeDbAll.h"
#include "../../Database/Generated/Server/DbPacketConversions.h"

using Database::GameDB::Player::SDbCharacter;
using Database::GameDB::Player::ToCharacterData;
using Game::SCharacterData;

template <typename T>
concept HasAccountId = requires(T data) { data.m_accountId; };

template <typename T>
concept HasPacketType = requires { T::kPacketType; };

static_assert(std::is_base_of_v<MHLib::serialization::ISerializable, SCharacterData>);
static_assert(std::is_same_v<decltype(SCharacterData::m_characterId), std::uint64_t>);
static_assert(std::is_same_v<decltype(SCharacterData::m_lastLogoutMap), std::uint32_t>);
static_assert(!HasAccountId<SCharacterData>);
static_assert(!HasPacketType<SCharacterData>);

int main(void)
{
	const SDbCharacter saved{0x123456789ABCDEF0ULL, 71, 0xFEDCBA98U};
	const SDbCharacter withoutMap{91, 73, std::nullopt};
	const SCharacterData present = ToCharacterData(saved);
	const SCharacterData absent = ToCharacterData(withoutMap);
	if (present.m_characterId != saved.m_characterId || present.m_lastLogoutMap != 0xFEDCBA98U)
	{
		return 1;
	}
	if (absent.m_characterId != withoutMap.m_characterId || absent.m_lastLogoutMap != 0)
	{
		return 2;
	}
	return 0;
}
