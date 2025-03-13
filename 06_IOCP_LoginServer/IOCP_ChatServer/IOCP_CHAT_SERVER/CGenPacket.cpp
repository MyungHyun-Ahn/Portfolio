#include "pch.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<FALSE> *CGenPacket::makePacketResLogin(BYTE status, INT64 accountNo)
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << status << accountNo;
    return sBuffer;
}

CSerializableBuffer<FALSE> *CGenPacket::makePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY)
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << accountNo << sectorX << sectorY;
    return sBuffer;
}

CSerializableBuffer<FALSE> *CGenPacket::makePacketResMessage(INT64 accountNo, WCHAR *id, WCHAR *nickname, WORD messageLen, WCHAR *message)
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << accountNo;
    sBuffer->Enqueue((char *)id, 20 * sizeof(WCHAR));
    sBuffer->Enqueue((char *)nickname, 20 * sizeof(WCHAR));
    *sBuffer << messageLen;
    sBuffer->Enqueue((char *)message, messageLen * sizeof(WCHAR));
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
