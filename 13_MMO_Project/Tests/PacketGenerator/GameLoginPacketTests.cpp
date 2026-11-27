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
	// 이 시험 타깃은 게임 서버를 링크하지 않고 실제 수신 View에 바이트를 공급한다.
	class CSession
	{
	public:
		[[nodiscard]]
		static bool LoadView(NetworkCommonLib::CSerializableBufferView &view,
			const NetworkCommonLib::CSerializableBuffer &buffer) noexcept
		{
			return view.tryInitHeapStorage(buffer.GetFullSize()) &&
				view.copy(buffer.GetContentBufferPtr(), buffer.GetHeaderSize(), buffer.GetDataSize());
		}
	};
}

namespace
{
	using GameData::EResultCode;
	using NetworkCommonLib::CSerializableBuffer;
	using NetworkCommonLib::CSerializableBufferView;
	using Database::GameDB::Player::SDbCharacter;
	using Database::GameDB::Player::ToCharacterData;

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
		Game::SResLogin m_packet{};
	};

	[[nodiscard]]
	bool CheckRequest() noexcept
	{
		Game::SReqLogin outgoing;
		outgoing.m_loginId = L"123";
		auto spBuffer = CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return false;
		}
		*spBuffer << static_cast<std::uint16_t>(Game::SReqLogin::kPacketType) << outgoing;
		constexpr std::array<unsigned char, 12> kExpected{1, 0, 3, 0, 0, 0, '1', 0, '2', 0, '3', 0};
		CSerializableBufferView view;
		if (!spBuffer->IsValid() || spBuffer->GetDataSize() != static_cast<int>(kExpected.size()) ||
			std::memcmp(spBuffer->GetContentBufferPtr(), kExpected.data(), kExpected.size()) != 0 ||
			!ServerNetworkLib::CSession::LoadView(view, *spBuffer))
		{
			return false;
		}
		std::uint16_t opcode{};
		view >> opcode;
		Game::SReqLogin decoded;
		return opcode == 1 && decoded.Deserialize(view) && view.GetDataSize() == 0 && decoded.m_loginId == L"123";
	}

	[[nodiscard]]
	bool CheckResponse(EResultCode code, const Game::SCharacterData &characterData) noexcept
	{
		auto spBuffer = CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return false;
		}
		Game::SResLogin outgoing;
		outgoing.m_result = code;
		outgoing.m_characterData = characterData;
		*spBuffer << static_cast<std::uint16_t>(Game::SResLogin::kPacketType) << outgoing;
		CLoginHandler handler;
		return spBuffer->IsValid() && spBuffer->GetDataSize() == 15 && Game::DispatchClientPacket(*spBuffer, handler) &&
			handler.m_calls == 1 && handler.m_packet.m_result == code &&
			handler.m_packet.m_characterData.m_characterId == characterData.m_characterId &&
			handler.m_packet.m_characterData.m_lastLogoutMap == characterData.m_lastLogoutMap;
	}

	[[nodiscard]]
	bool CheckUndefinedResult() noexcept
	{
		auto spBuffer = CSerializableBuffer::MakeShared();
		if (spBuffer == nullptr)
		{
			return false;
		}
		Game::SResLogin outgoing;
		outgoing.m_result = static_cast<EResultCode>(2);
		*spBuffer << std::uint16_t{2} << outgoing;
		if (spBuffer->IsValid() || spBuffer->GetDataSize() != 2)
		{
			return false;
		}
		spBuffer->Clear();
		*spBuffer << std::uint16_t{2} << std::uint8_t{2} << std::uint64_t{1001} << std::uint32_t{12};
		CSerializableBufferView view;
		if (!ServerNetworkLib::CSession::LoadView(view, *spBuffer))
		{
			return false;
		}
		std::uint16_t opcode{};
		view >> opcode;
		Game::SResLogin decoded;
		CLoginHandler handler;
		return opcode == 2 && !decoded.Deserialize(view) && view.GetDataSize() == 12 &&
			!Game::DispatchClientPacket(*spBuffer, handler) && handler.m_calls == 0;
	}
}

int main(void)
{
	return CheckRequest() && CheckResponse(EResultCode::Success, ToCharacterData(SDbCharacter{1001, 91, 12})) &&
		CheckResponse(EResultCode::Success, ToCharacterData(SDbCharacter{1002, 92, std::nullopt})) &&
		CheckResponse(EResultCode::Rejected, Game::SCharacterData{}) && CheckUndefinedResult() ? 0 : 1;
}
