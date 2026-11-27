#include "pch.h"
#include "NetworkCommonLibPch.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "ClientPacketDispatch.g.h"
#include "../../Database/Generated/Server/IncludeDbAll.h"
#include "../../Database/Generated/Server/DbPacketConversions.h"

namespace ServerNetworkLib
{
	// 이 시험은 실제 CSession을 링크하지 않고 기존 View 초기화 경계에 본문을 공급한다.
	class CSession
	{
	public:
		[[nodiscard]]
		static bool LoadTestBody(NetworkCommonLib::CSerializableBufferView &view,
			const NetworkCommonLib::CSerializableBuffer &source, int bodyBytes) noexcept
		{
			return view.tryInitHeapStorage(source.GetFullSize()) &&
				view.copy(source.GetContentBufferPtr(), source.GetHeaderSize(), bodyBytes);
		}
	};
}

namespace
{
	using Database::GameDB::Player::SDbCharacter;
	using Database::GameDB::Player::ToCharacterData;
	using NetworkCommonLib::CSerializableBuffer;
	using NetworkCommonLib::CSerializableBufferView;

	constexpr std::uint64_t kCharacterId = 0x123456789ABCDEF0ULL;
	constexpr std::uint32_t kSavedMap = 0x10203040U;
	constexpr std::uint16_t kSuffix = 0x5678;
	constexpr int kBodyBytes = 17; // opcode, 결과, 캐릭터 ID, 맵, 뒤따르는 시험 필드

	class CLoginHandler final : public Game::IClientPacketHandler
	{
	public:
		bool HandleResLogin(const Game::SResLogin &packet) noexcept override
		{
			++m_calls;
			m_packet = packet;
			return true;
		}

		int m_calls = 0;
		Game::SResLogin m_packet;
	};

	[[nodiscard]]
	bool Matches(const Game::SResLogin &packet, std::uint32_t expectedMap) noexcept
	{
		return packet.m_result == 0x7F && packet.m_characterData.m_characterId == kCharacterId &&
			packet.m_characterData.m_lastLogoutMap == expectedMap && packet.m_suffix == kSuffix;
	}

	[[nodiscard]]
	bool CheckRoundTrip(std::optional<std::uint32_t> savedMap, bool checkBadBodies) noexcept
	{
		Game::SResLogin outgoing;
		outgoing.m_result = 0x7F;
		outgoing.m_characterData = ToCharacterData(SDbCharacter{kCharacterId, 77, savedMap});
		outgoing.m_suffix = kSuffix;
		auto spWire = CSerializableBuffer::MakeShared();
		if (spWire == nullptr)
		{
			return false;
		}
		*spWire << static_cast<std::uint16_t>(Game::SResLogin::kPacketType) << outgoing;
		if (!spWire->IsValid() || spWire->GetDataSize() != kBodyBytes)
		{
			return false;
		}
		constexpr std::array<unsigned char, kBodyBytes> kSavedMapBytes{
			0x02, 0x00, 0x7F, 0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0x40, 0x30, 0x20, 0x10, 0x78, 0x56};
		if (savedMap.has_value() && std::memcmp(spWire->GetContentBufferPtr(), kSavedMapBytes.data(), kBodyBytes) != 0)
		{
			return false;
		}

		const auto expectedMap = savedMap.value_or(0);
		CSerializableBufferView view;
		std::uint16_t opcode = 0;
		Game::SResLogin fromView;
		if (!ServerNetworkLib::CSession::LoadTestBody(view, *spWire, kBodyBytes))
		{
			return false;
		}
		view >> opcode;
		if (opcode != 2 || !fromView.Deserialize(view) || view.GetDataSize() != 0 || !Matches(fromView, expectedMap))
		{
			return false;
		}

		CLoginHandler handler;
		if (checkBadBodies)
		{
			// 중첩 레코드 내부에서 자른 응답은 뒤 필드를 읽거나 핸들러를 호출하지 않는다.
			constexpr int kTruncatedBytes = kBodyBytes - 3;
			if (!ServerNetworkLib::CSession::LoadTestBody(view, *spWire, kTruncatedBytes))
			{
				return false;
			}
			Game::SResLogin truncated;
			view >> opcode;
			if (truncated.Deserialize(view) || view.IsValid() || truncated.m_suffix != 0)
			{
				return false;
			}
			auto spBad = CSerializableBuffer::MakeShared();
			if (spBad == nullptr || !spBad->Enqueue(spWire->GetContentBufferPtr(), kTruncatedBytes) ||
				Game::DispatchClientPacket(*spBad, handler) || handler.m_calls != 0)
			{
				return false;
			}
			spBad->Clear();
			if (!spBad->Enqueue(spWire->GetContentBufferPtr(), kBodyBytes))
			{
				return false;
			}
			*spBad << std::uint8_t{1};
			if (Game::DispatchClientPacket(*spBad, handler) || handler.m_calls != 0)
			{
				return false;
			}
		}
		return Game::DispatchClientPacket(*spWire, handler) && handler.m_calls == 1 && Matches(handler.m_packet, expectedMap);
	}
}

int main(void)
{
	return CheckRoundTrip(kSavedMap, true) && CheckRoundTrip(std::nullopt, false) ? 0 : 1;
}
