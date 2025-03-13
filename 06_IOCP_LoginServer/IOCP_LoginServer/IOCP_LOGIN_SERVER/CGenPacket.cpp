#include "pch.h"
#include "CommonProtocol.h"
#include "CGenPacket.h"

CSerializableBuffer<FALSE> *CGenPacket::makePacketResLogin(const INT64 accountNo
    , const BYTE status, const WCHAR *id, const WCHAR *nickname
    , const WCHAR *gameServerIp, const USHORT gameServerPort
    , const WCHAR *chatServerIp, const USHORT chatServerPort)
{
    CSerializableBuffer<FALSE> *sBuffer = CSerializableBuffer<FALSE>::Alloc();
    *sBuffer << (WORD)en_PACKET_CS_LOGIN_RES_LOGIN << accountNo << status;

	sBuffer->Enqueue((char *)id, 20 * sizeof(WCHAR));
	sBuffer->Enqueue((char *)nickname, 20 * sizeof(WCHAR));

	sBuffer->Enqueue((char *)gameServerIp, 16 * sizeof(WCHAR));
    *sBuffer << gameServerPort;

    sBuffer->Enqueue((char *)chatServerIp, 16 * sizeof(WCHAR));
    *sBuffer << chatServerPort;

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

