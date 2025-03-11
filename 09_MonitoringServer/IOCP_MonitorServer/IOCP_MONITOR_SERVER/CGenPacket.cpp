#include "pch.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<FALSE> *CGenPacket::makePacketResMonitorToolLogin(const BYTE status) noexcept
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_MONITOR_TOOL_RES_LOGIN << status;
    return sBuffer;
}

CSerializableBuffer<FALSE> *CGenPacket::makePacketResMonitorToolDataUpdate(const BYTE serverNo, const BYTE dataType, const int dataValue, const int timeStamp) noexcept
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << serverNo << dataType << dataValue << timeStamp;
    return sBuffer;
}
