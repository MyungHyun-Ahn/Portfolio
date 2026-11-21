#pragma once

namespace Chatting
{
	constexpr int kMaxNameCodeUnits = 19;
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

	[[nodiscard]] inline bool IsValidString(const std::wstring &value, std::size_t maxCodeUnits) noexcept
	{
		return !value.empty() && value.size() <= maxCodeUnits && value.find(L'\0') == std::wstring::npos;
	}

	static_assert(sizeof(WCHAR) == 2);
}
