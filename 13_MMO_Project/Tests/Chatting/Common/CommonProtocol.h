#pragma once

namespace Chatting
{
	constexpr int kNameLength = 20; // null 포함
	constexpr int kSessionKeyBytes = 64;
	constexpr int kMaxMessageBytes = 512; // null 미포함
	constexpr int kMaxSectorX = 50;
	constexpr int kMaxSectorY = 50;
	constexpr int kSectorViewCount = 3;
	constexpr int kSectorViewStart = 1;
	constexpr BYTE kPacketCode = 0x77;
	constexpr BYTE kPacketKey = 0xa9;
	constexpr USHORT kDefaultPort = 6000;
	constexpr DWORD kLoginTimeoutMilliseconds = 10000;
	constexpr DWORD kHeartbeatMilliseconds = 30000;
	constexpr DWORD kReceiveTimeoutMilliseconds = 40000;
	// 로컬 테스트 전용. 운영 인증으로 사용하지 않는다.
	constexpr char kTestSessionKey[kSessionKeyBytes] = "LOCAL_CHAT_TEST_ONLY";

	static_assert(sizeof(WCHAR) == 2);
}
