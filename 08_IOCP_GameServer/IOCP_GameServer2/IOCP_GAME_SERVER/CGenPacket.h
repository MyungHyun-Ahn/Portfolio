#pragma once
class CGenPacket
{
public:
	static CSerializableBuffer<FALSE> *makePacketResLogin(BYTE status, INT64 accountNo);
	static CSerializableBuffer<FALSE> *makePacketResEcho(INT64 accountNo, LONGLONG sendTick);

	// Monitor Client
	static CSerializableBuffer<TRUE> *makePacketReqMonitorLogin(const INT serverNo);
	static CSerializableBuffer<TRUE> *makePacketReqMonitorUpdate(const BYTE dataType, const INT dataValue, const INT timeStamp);
};

