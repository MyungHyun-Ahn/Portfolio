#pragma once
class CGenPacket
{
public:
	static CSerializableBuffer<FALSE> *makePacketReqLoginServer(const INT64 accountNo, char *sessionKey);
};

