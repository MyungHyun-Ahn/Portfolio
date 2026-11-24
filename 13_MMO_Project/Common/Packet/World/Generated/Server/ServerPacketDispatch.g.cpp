// PacketGenerator generated file. Do not edit.
#include "pch.h"
#include "NetworkCommonLibPch.h"
#include "MHLib/serialization/ISerializable.h"
#include <functional>
#include <utility>
#include <vector>
#include "IncludePacketAll.g.h"
#include "ServerPacketDispatch.g.h"
#include "IServerPacketHandler.g.h"

namespace WorldServer
{
	namespace
	{
		using PacketFunction = std::function<bool(UINT64, NetworkCommonLib::CSerializableBufferView &, IServerPacketHandler &)>;

		// 본문 검증에 성공하면 등록된 수신 핸들러를 호출한다.
		template<typename TPacket, bool (IServerPacketHandler::*Handler)(UINT64, const TPacket &) noexcept>
		bool HandlePacket(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView &message, IServerPacketHandler &handler) noexcept
		{
			TPacket packet;
			return packet.Deserialize(message) && message.GetDataSize() == 0 && (handler.*Handler)(sessionId, packet);
		}

		// 수신 방향의 패킷 핸들러를 opcode별로 한 번 등록한다.
		std::vector<PacketFunction> MakePacketFunctionTable()
		{
			std::vector<PacketFunction> funcTable(2);
			funcTable[static_cast<std::uint16_t>(EPacketType::ReqAdd)] =
				HandlePacket<SReqAdd, &IServerPacketHandler::HandleReqAdd>;
			return funcTable;
		}

		const auto kFuncTable = MakePacketFunctionTable();
	}

	bool DispatchServerPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView &message,
		IServerPacketHandler &handler, std::uint16_t *outOpcode) noexcept
	{
		std::uint16_t opcode = 0;
		message >> opcode;
		if (!message.IsValid())
			return false;
		if (outOpcode != nullptr)
			*outOpcode = opcode;
		if (opcode >= kFuncTable.size() || !kFuncTable[opcode])
			return false;
		return kFuncTable[opcode](sessionId, message, handler);
	}
}
