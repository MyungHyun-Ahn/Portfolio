#pragma once
class CGenPacket
{
public:
	// Chat Server
	static CSerializableBuffer<FALSE> *makePacketResLogin(BYTE status, INT64 accountNo);
	static CSerializableBuffer<FALSE> *makePacketResSectorMove(INT64 accountNo, WORD sectorX, WORD sectorY);
	static CSerializableBuffer<FALSE> *makePacketResMessage(INT64 accountNo, WCHAR *id, WCHAR *nickname, WORD messageLen, WCHAR *message);

	// Monitor Client
	static CSerializableBuffer<TRUE> *makePacketReqMonitorLogin(const INT serverNo);
	static CSerializableBuffer<TRUE> *makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp);
};

