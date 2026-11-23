#include "NetworkCommonLibPch.h"
#include "PacketFraming.h"

namespace NetworkCommonLib
{
	void PreparePacket(CSerializableBuffer &message, BYTE packetCode, EPacketMode mode) noexcept
	{
		if (message.GetIsEnqueueHeader())
			return;

		auto *pHeader = reinterpret_cast<SPacketHeader *>(message.GetBufferPtr());
		pHeader->code = packetCode;
		pHeader->len = static_cast<USHORT>(message.GetDataSize());
		pHeader->randKey = 0;
		pHeader->checkSum = 0;
		// CheckSum부터 인코딩. 여러 세션에 넣기 전에 한 번만 수행한다.
		if (mode == EPacketMode::Net)
		{
			pHeader->checkSum = MHLib::security::CEncryption::CalCheckSum(message.GetContentBufferPtr(), message.GetDataSize());
			MHLib::security::CEncryption::Encoding(message.GetBufferPtr() + 4, message.GetDataSize() + 1, pHeader->randKey);
		}
		message.m_isEnqueueHeader = TRUE;
	}
}
