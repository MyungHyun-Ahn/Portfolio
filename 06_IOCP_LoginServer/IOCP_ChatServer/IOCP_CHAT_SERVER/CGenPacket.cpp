#include "pch.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<SERVER_TYPE::WAN> *CGenPacket::makePacketResLogin(BYTE status, INT64 accountNo)
{
    CSerializableBuffer<SERVER_TYPE::WAN> *sBuffer = CSerializableBuffer<SERVER_TYPE::WAN>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_LOGIN << status << accountNo;
    return sBuffer;
}

CSerializableBuffer<SERVER_TYPE::WAN> *CGenPacket::makePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY)
{
    CSerializableBuffer<SERVER_TYPE::WAN> *sBuffer = CSerializableBuffer<SERVER_TYPE::WAN>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE << accountNo << sectorX << sectorY;
    return sBuffer;
}

CSerializableBuffer<SERVER_TYPE::WAN> *CGenPacket::makePacketResMessage(INT64 accountNo, WCHAR *id, WCHAR *nickname, WORD messageLen, WCHAR *message)
{
    CSerializableBuffer<SERVER_TYPE::WAN> *sBuffer = CSerializableBuffer<SERVER_TYPE::WAN>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE << accountNo;
    sBuffer->Enqueue((char *)id, 20 * sizeof(WCHAR));
    sBuffer->Enqueue((char *)nickname, 20 * sizeof(WCHAR));
    *sBuffer << messageLen;
    sBuffer->Enqueue((char *)message, messageLen * sizeof(WCHAR));
    return sBuffer;
}

CSerializableBuffer<SERVER_TYPE::LAN> *CGenPacket::makePacketReqMonitorLogin(const INT serverNo)
{
    CSerializableBuffer<SERVER_TYPE::LAN> *sBuffer = CSerializableBuffer<SERVER_TYPE::LAN>::Alloc();
    *sBuffer << (WORD)en_PACKET_SS_MONITOR_LOGIN << serverNo;
    return sBuffer;
}

CSerializableBuffer<SERVER_TYPE::LAN> *CGenPacket::makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp)
{
	CSerializableBuffer<SERVER_TYPE::LAN> *sBuffer = CSerializableBuffer<SERVER_TYPE::LAN>::Alloc();
    *sBuffer << (WORD)en_PACKET_SS_MONITOR_DATA_UPDATE << dataType << dataValue << timeStamp;
	return sBuffer;
}
