#include "pch.h"
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

namespace Chatting
{
	NetworkLib::CSerializableBuffer<FALSE> *CGenPacket::MakePacketResLogin(BYTE status, INT64 accountNo) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		*pBuffer << static_cast<WORD>(EPacketType::ResLogin) << status << accountNo;
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *CGenPacket::MakePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		*pBuffer << static_cast<WORD>(EPacketType::ResSectorMove) << accountNo << sectorX << sectorY;
		return pBuffer;
	}

	NetworkLib::CSerializableBuffer<FALSE> *CGenPacket::MakePacketResMessage(INT64 accountNo, const WCHAR *pId,
		const WCHAR *pNickname, WORD messageBytes, const WCHAR *pMessage) noexcept
	{
		auto *pBuffer = NetworkLib::CSerializableBuffer<FALSE>::Alloc();
		*pBuffer << static_cast<WORD>(EPacketType::ResMessage) << accountNo;
		pBuffer->Enqueue((char *)pId, kNameLength * sizeof(WCHAR));
		pBuffer->Enqueue((char *)pNickname, kNameLength * sizeof(WCHAR));
		*pBuffer << messageBytes;
		// MessageLen은 글자 수가 아니라 바이트 수다.
		pBuffer->Enqueue((char *)pMessage, messageBytes);
		return pBuffer;
	}
}
