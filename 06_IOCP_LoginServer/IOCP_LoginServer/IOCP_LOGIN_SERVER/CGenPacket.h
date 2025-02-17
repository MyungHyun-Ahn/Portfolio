#pragma once
class CGenPacket
{
public:
	static CSerializableBuffer<FALSE> *makePacketResLogin(const INT64 accountNo, const BYTE status, const WCHAR *id, const WCHAR *nickname, const WCHAR *gameServerIp, const USHORT gameServerPort, const WCHAR *chatServerIp, const USHORT chatServerPort);
};

