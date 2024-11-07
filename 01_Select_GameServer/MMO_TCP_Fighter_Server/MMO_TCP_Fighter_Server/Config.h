#pragma once
/*
	Config.h
		* 각종 서버 설정 옵션과 게임 설정 옵션 작성
		* 추후 몇몇 옵션은 config 파일 파서를 만든 후 옮길 것
*/

// 서버 설정
// 서버 IP, Port
constexpr const char *SERVER_IP = "0.0.0.0";
constexpr short SERVER_PORT = 10611;

// 서버 프레임 설정
constexpr int FRAME_PER_SECOND = 25;
constexpr int TICK_PER_FRAME = (1000 / FRAME_PER_SECOND);

// 패킷 식별자 - 이것이 들어가지 않으면 잘못된 패킷으로 간주
constexpr BYTE PACKET_IDENTIFIER = 0x89;

// 30초 이상이 되도록 아무런 메시지 수신도 없는경우 접속 끊음
constexpr int NETWORK_PACKET_RECV_TIMEOUT = 30000;


// 게임 설정
// 화면 이동 범위
constexpr int RANGE_MOVE_TOP = 0;
constexpr int RANGE_MOVE_LEFT = 0;
constexpr int RANGE_MOVE_RIGHT = 6400;
constexpr int RANGE_MOVE_BOTTOM = 6400;

// 섹터 크기
constexpr int SECTOR_SIZE = 160;

// 주변 몇칸까지 보이게할 것인지
constexpr int SECTOR_VIEW_START = 1; // 대각선 1칸 위에서 부터
constexpr int SECTOR_VIEW_COUNT = 3; // 3 x 3

constexpr int SECTOR_MAX_X = (RANGE_MOVE_RIGHT - RANGE_MOVE_LEFT) / SECTOR_SIZE;
constexpr int SECTOR_MAX_Y = (RANGE_MOVE_BOTTOM - RANGE_MOVE_TOP) / SECTOR_SIZE;

// 최대 체력
constexpr int MAX_PLAYER_HP = 100;

// 공격범위
constexpr int ATTACK1_RANGE_X = 80;
constexpr int ATTACK2_RANGE_X = 90;
constexpr int ATTACK3_RANGE_X = 100;
constexpr int ATTACK1_RANGE_Y = 10;
constexpr int ATTACK2_RANGE_Y = 10;
constexpr int ATTACK3_RANGE_Y = 20;


// 공격 데미지
constexpr int ATTACK1_DAMAGE = 1;
constexpr int ATTACK2_DAMAGE = 2;
constexpr int ATTACK3_DAMAGE = 3;


// 캐릭터 이동 속도   // 25fps 기준 이동속도
constexpr int SPEED_PLAYER_X = 3 * (50 / FRAME_PER_SECOND);	// 3   50fps;
constexpr int SPEED_PLAYER_Y = 2 * (50 / FRAME_PER_SECOND);	// 2   50fps;


// 이동 오류체크 범위
constexpr int ERROR_RANGE = 50;