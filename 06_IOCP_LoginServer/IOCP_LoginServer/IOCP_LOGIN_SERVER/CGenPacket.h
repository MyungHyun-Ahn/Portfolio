#pragma once
class CGenPacket
{
public:
	// Login Server
	static CSerializableBuffer<FALSE> *makePacketResLogin(const INT64 accountNo, const BYTE status, const WCHAR *id, const WCHAR *nickname, const WCHAR *gameServerIp, const USHORT gameServerPort, const WCHAR *chatServerIp, const USHORT chatServerPort);

	// Monitor Client
	static CSerializableBuffer<TRUE> *makePacketReqMonitorLogin(const INT serverNo);
	static CSerializableBuffer<TRUE> *makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp);
};

