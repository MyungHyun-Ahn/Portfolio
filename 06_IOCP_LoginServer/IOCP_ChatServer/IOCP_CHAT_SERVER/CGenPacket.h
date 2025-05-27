#pragma once
class CGenPacket
{
public:
	// Chat Server
	static CSerializableBuffer<SERVER_TYPE::WAN> *makePacketResLogin(BYTE status, INT64 accountNo);
	static CSerializableBuffer<SERVER_TYPE::WAN> *makePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY);
	static CSerializableBuffer<SERVER_TYPE::WAN> *makePacketResMessage(INT64 accountNo, WCHAR *id, WCHAR *nickname, WORD messageLen, WCHAR *message);

	// Monitor Client
	static CSerializableBuffer<SERVER_TYPE::LAN> *makePacketReqMonitorLogin(const INT serverNo);
	static CSerializableBuffer<SERVER_TYPE::LAN> *makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp);
};

