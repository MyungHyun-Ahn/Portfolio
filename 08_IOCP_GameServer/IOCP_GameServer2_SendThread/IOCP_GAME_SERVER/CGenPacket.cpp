#include "pch.h"
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

CSerializableBuffer<TRUE> *CGenPacket::makePacketReqMonitorLogin(const INT serverNo)
{
	CSerializableBuffer<TRUE> *sBuffer = CSerializableBuffer<TRUE>::Alloc();
	*sBuffer << (WORD)en_PACKET_SS_MONITOR_LOGIN << serverNo;
	return sBuffer;
}

CSerializableBuffer<TRUE> *CGenPacket::makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp)
{
	CSerializableBuffer<TRUE> *sBuffer = CSerializableBuffer<TRUE>::Alloc();
	*sBuffer << (WORD)en_PACKET_SS_MONITOR_DATA_UPDATE << dataType << dataValue << timeStamp;
	return sBuffer;
}

