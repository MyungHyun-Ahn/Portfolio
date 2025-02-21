#include "pch.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<FALSE> *CGenPacket::makePacketReqLoginServer(INT64 accountNo, char *sessionKey)
{
	CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
	*sBuffer << (WORD)en_PACKET_CS_LOGIN_REQ_LOGIN << accountNo;

	sBuffer->Enqueue(sessionKey, 64);

	return sBuffer;
}
