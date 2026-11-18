#pragma once

namespace Chatting
{
	class CGenPacket
	{
	public:
		static NetworkLib::CSerializableBuffer<FALSE> *MakePacketResLogin(BYTE status, INT64 accountNo) noexcept;
		static NetworkLib::CSerializableBuffer<FALSE> *MakePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY) noexcept;
		static NetworkLib::CSerializableBuffer<FALSE> *MakePacketResMessage(INT64 accountNo, const WCHAR *pId,
			const WCHAR *pNickname, WORD messageBytes, const WCHAR *pMessage) noexcept;
	};
}
