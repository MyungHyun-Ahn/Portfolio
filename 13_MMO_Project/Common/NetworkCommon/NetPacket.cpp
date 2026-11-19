#include "pch.h"
#include "NetHeaders.h"
#include "CSerializableBuffer.h"
#include "NetPacket.h"

namespace NetworkLib
{
	void PrepareNetPacket(CSerializableBuffer<FALSE> &message, BYTE packetCode) noexcept
	{
		if (message.GetIsEnqueueHeader())
			return;

		auto *pHeader = reinterpret_cast<SNetHeader *>(message.GetBufferPtr());
		pHeader->code = packetCode;
		pHeader->len = static_cast<USHORT>(message.GetDataSize());
		pHeader->randKey = 0;
		pHeader->checkSum = MHLib::security::CEncryption::CalCheckSum(message.GetContentBufferPtr(), message.GetDataSize());
		// CheckSum부터 인코딩. 여러 세션에 넣기 전에 한 번만 수행한다.
		MHLib::security::CEncryption::Encoding(message.GetBufferPtr() + 4, message.GetDataSize() + 1, pHeader->randKey);
		message.m_isEnqueueHeader = TRUE;
	}
}
