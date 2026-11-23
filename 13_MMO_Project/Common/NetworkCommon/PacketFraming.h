#pragma once

namespace NetworkCommonLib
{
	class CSerializableBuffer;

	// 패킷 키는 기존 CEncryption의 프로세스 공용 값을 사용한다.
	void PreparePacket(CSerializableBuffer &message, BYTE packetCode, EPacketMode mode = EPacketMode::Net) noexcept;
}
