#pragma once
class CGenPacket
{
public:
	// NetMonitor
	static CSerializableBuffer<FALSE> *makePacketResMonitorToolLogin(const BYTE status) noexcept;
	static CSerializableBuffer<FALSE> *makePacketResMonitorToolDataUpdate(const BYTE serverNo, const BYTE dataType, const int dataValue, const int timeStamp) noexcept;
};

