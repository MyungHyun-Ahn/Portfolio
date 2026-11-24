// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "NetworkCommonLibPch.h"
#include "MHLib/serialization/ISerializable.h"
#include <functional>
#include <utility>
#include <vector>
#include "IncludePacketAll.g.h"
#include "ClientPacketDispatch.g.h"
#include "IClientPacketHandler.g.h"

namespace WorldServer
{
	namespace
	{
		using PacketFunction = std::function<bool(NetworkCommonLib::CSerializableBuffer &, IClientPacketHandler &)>;

		// 본문 검증에 성공하면 등록된 수신 핸들러를 호출한다.
		template<typename TPacket, bool (IClientPacketHandler::*Handler)(const TPacket &) noexcept>
		bool HandlePacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept
		{
			TPacket packet;
			message >> packet;
			return message.IsValid() && message.GetDataSize() == 0 && (handler.*Handler)(packet);
		}

		// 수신 방향의 패킷 핸들러를 opcode별로 한 번 등록한다.
		std::vector<PacketFunction> MakePacketFunctionTable()
		{
			std::vector<PacketFunction> funcTable(7);
			funcTable[static_cast<std::uint16_t>(EPacketType::ResAdd)] =
				HandlePacket<SResAdd, &IClientPacketHandler::HandleResAdd>;
			funcTable[static_cast<std::uint16_t>(EPacketType::ResChange)] =
				HandlePacket<SResChange, &IClientPacketHandler::HandleResChange>;
			funcTable[static_cast<std::uint16_t>(EPacketType::ResMove)] =
				HandlePacket<SResMove, &IClientPacketHandler::HandleResMove>;
			return funcTable;
		}

		const auto kFuncTable = MakePacketFunctionTable();
	}

	bool DispatchClientPacket(NetworkCommonLib::CSerializableBuffer &message, IClientPacketHandler &handler) noexcept
	{
		std::uint16_t opcode = 0;
		message >> opcode;
		if (!message.IsValid())
			return false;
		if (opcode >= kFuncTable.size() || !kFuncTable[opcode])
			return false;
		return kFuncTable[opcode](message, handler);
	}
}
