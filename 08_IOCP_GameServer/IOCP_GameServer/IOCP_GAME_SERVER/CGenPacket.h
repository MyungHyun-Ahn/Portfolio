#pragma once
class CGenPacket
{
public:
	static CSerializableBuffer<FALSE> *makePacketResLogin(BYTE status, INT64 accountNo);
	static CSerializableBuffer<FALSE> *makePacketResEcho(INT64 accountNo, LONGLONG sendTick);
};

