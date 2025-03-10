#include "pch.h"
#include "MyInclude.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<FALSE> *CGenPacket::makePacketResLogin(BYTE status, INT64 accountNo)
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_GAME_RES_LOGIN << status << accountNo;
    return sBuffer;
}

CSerializableBuffer<FALSE> *CGenPacket::makePacketResEcho(INT64 accountNo, LONGLONG sendTick)
{
	CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
	*sBuffer << (WORD)en_PACKET_CS_GAME_RES_ECHO << accountNo << sendTick;
	return sBuffer;
}


