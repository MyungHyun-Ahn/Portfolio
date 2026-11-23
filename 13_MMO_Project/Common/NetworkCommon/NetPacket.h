#pragma once

namespace NetworkCommonLib
{
	template<bool isLanServer> class CSerializableBuffer;

	// 패킷 키는 기존 CEncryption의 프로세스 공용 값을 사용한다.
	void PrepareNetPacket(CSerializableBuffer<FALSE> &message, BYTE packetCode) noexcept;
}
