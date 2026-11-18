#pragma once

namespace Chatting
{
	enum class EPacketType : WORD
	{
		ChatServer = 0,
		ReqLogin = 1,
		ResLogin = 2,
		ReqSectorMove = 3,
		ResSectorMove = 4,
		ReqMessage = 5,
		ResMessage = 6,
		ReqHeartbeat = 7
	};

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

	// 본문 필드 순서는 원본과 동일, WCHAR은 UTF-16이다.
	// ReqLogin: INT64 AccountNo, WCHAR ID[20], WCHAR Nickname[20], char SessionKey[64]
	// ResLogin: BYTE Status, INT64 AccountNo
	// Req/ResSectorMove: INT64 AccountNo, WORD SectorX, WORD SectorY
	// ReqMessage: INT64 AccountNo, WORD MessageLen, WCHAR Message[MessageLen / 2]
	// ResMessage: INT64 AccountNo, WCHAR ID[20], WCHAR Nickname[20], WORD MessageLen, WCHAR Message[MessageLen / 2]
	// ReqHeartbeat: INT64 AccountNo (원본 처리 코드 기준)
	static_assert(sizeof(WCHAR) == 2);
}
