#pragma once
enum en_PACKET_TYPE
{
	////////////////////////////////////////////////////////
	//
	//	Client & Server Protocol
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------
	// Chatting Server
	//------------------------------------------------------
	en_PACKET_CS_CHAT_SERVER = 0,

	//------------------------------------------------------------
	// 채팅서버 로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//		char	SessionKey[64];
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_LOGIN,

	//------------------------------------------------------------
	// 채팅서버 로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_LOGIN,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_SECTOR_MOVE,

	//------------------------------------------------------------
	// 채팅서버 섹터 이동 결과
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_SECTOR_MOVE,

	//------------------------------------------------------------
	// 채팅서버 채팅보내기 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_REQ_MESSAGE,

	//------------------------------------------------------------
	// 채팅서버 채팅보내기 응답  (다른 클라가 보낸 채팅도 이걸로 받음)
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]						// null 포함
	//		WCHAR	Nickname[20]				// null 포함
	//		
	//		WORD	MessageLen
	//		WCHAR	Message[MessageLen / 2]		// null 미포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_CHAT_RES_MESSAGE,

	//------------------------------------------------------------
	// 하트비트
	//
	//	{
	//		WORD		Type
	//	}
	//
	//
	// 클라이언트는 이를 30초마다 보내줌.
	// 서버는 40초 이상동안 메시지 수신이 없는 클라이언트를 강제로 끊어줘야 함.
	//------------------------------------------------------------	
	en_PACKET_CS_CHAT_REQ_HEARTBEAT,






	//------------------------------------------------------
	// Login Server
	//------------------------------------------------------
	en_PACKET_CS_LOGIN_SERVER = 100,

	//------------------------------------------------------------
	// 로그인 서버로 클라이언트 로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		char	SessionKey[64]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_LOGIN_REQ_LOGIN,

	//------------------------------------------------------------
	// 로그인 서버에서 클라이언트로 로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		BYTE	Status				// 0 (세션오류) / 1 (성공) ...  하단 defines 사용
	//
	//		WCHAR	ID[20]				// 사용자 ID		. null 포함
	//		WCHAR	Nickname[20]		// 사용자 닉네임	. null 포함
	//
	//		WCHAR	GameServerIP[16]	// 접속대상 게임,채팅 서버 정보
	//		USHORT	GameServerPort
	//		WCHAR	ChatServerIP[16]
	//		USHORT	ChatServerPort
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_LOGIN_RES_LOGIN,








	//------------------------------------------------------
	// Game Server
	//------------------------------------------------------
	en_PACKET_CS_GAME_SERVER = 1000,





	////////////////////////////////////////////////////////
	//
	//   Server & Server Protocol  / LAN 통신은 기본으로 응답을 받지 않음.
	//
	////////////////////////////////////////////////////////
	en_PACKET_SS_LAN = 10000,
	//------------------------------------------------------
	// GameServer & LoginServer & ChatServer Protocol
	//------------------------------------------------------

	//------------------------------------------------------------
	// 다른 서버가 로그인 서버로 로그인.
	// 이는 응답이 없으며, 그냥 로그인 됨.  
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	ServerType			// dfSERVER_TYPE_GAME / dfSERVER_TYPE_CHAT
	//
	//		WCHAR	ServerName[32]		// 해당 서버의 이름.  
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_LOGINSERVER_LOGIN,



	//------------------------------------------------------------
	// 로그인서버에서 게임.채팅 서버로 새로운 클라이언트 접속을 알림.
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		CHAR	SessionKey[64]
	//	}
	//
	//------------------------------------------------------------
	// en_PACKET_SS_NEW_CLIENT_LOGIN,	// 신규 접속자의 세션키 전달패킷을 요청,응답구조로 변경 2017.01.05


	//------------------------------------------------------------
	// 로그인서버에서 게임.채팅 서버로 새로운 클라이언트 접속을 알림.
	//
	// 마지막의 Parameter 는 세션키 공유에 대한 고유값 확인을 위한 어떤 값. 이는 응답 결과에서 다시 받게 됨.
	// 채팅서버와 게임서버는 Parameter 에 대한 처리는 필요 없으며 그대로 Res 로 돌려줘야 합니다.
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		CHAR	SessionKey[64]
	//		INT64	Parameter
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_REQ_NEW_CLIENT_LOGIN,

	//------------------------------------------------------------
	// 게임.채팅 서버가 새로운 클라이언트 접속패킷 수신결과를 돌려줌.
	// 게임서버용, 채팅서버용 패킷의 구분은 없으며, 로그인서버에 타 서버가 접속 시 CHAT,GAME 서버를 구분하므로 
	// 이를 사용해서 알아서 구분 하도록 함.
	//
	// 플레이어의 실제 로그인 완료는 이 패킷을 Chat,Game 양쪽에서 다 받았을 시점임.
	//
	// 마지막 값 Parameter 는 이번 세션키 공유에 대해 구분할 수 있는 특정 값
	// ClientID 를 쓰던, 고유 카운팅을 쓰던 상관 없음.
	//
	// 로그인서버에 접속과 재접속을 반복하는 경우 이전에 공유응답이 새로 접속한 뒤의 응답으로
	// 오해하여 다른 세션키를 들고 가는 문제가 생김.
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		INT64	Parameter
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_RES_NEW_CLIENT_LOGIN,


	////////////////////////////////////////////////////////
	//
	//   MonitorServer & MoniterTool Protocol / 응답을 받지 않음.
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------
	// Monitor Server  Protocol
	//------------------------------------------------------
		en_PACKET_SS_MONITOR = 20000,
		//------------------------------------------------------
		// Server -> Monitor Protocol
		//------------------------------------------------------
		//------------------------------------------------------------
		// LoginServer, GameServer , ChatServer  가 모니터링 서버에 로그인 함
		//
		// 
		//	{
		//		WORD	Type
		//
		//		int		ServerNo		//  각 서버마다 고유 번호를 부여하여 사용
		//	}
		//
		//------------------------------------------------------------
		en_PACKET_SS_MONITOR_LOGIN,

		//------------------------------------------------------------
		// 서버가 모니터링서버로 데이터 전송
		// 각 서버는 자신이 모니터링중인 수치를 1초마다 모니터링 서버로 전송.
		//
		// 서버의 다운 및 기타 이유로 모니터링 데이터가 전달되지 못할떄를 대비하여 TimeStamp 를 전달한다.
		// 이는 모니터링 클라이언트에서 계산,비교 사용한다.
		// 
		//	{
		//		WORD	Type
		//
		//		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
		//		int		DataValue				// 해당 데이터 수치.
		//		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
		//										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
		//										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
		//	}
		//
		//------------------------------------------------------------
		en_PACKET_SS_MONITOR_DATA_UPDATE,

};



enum en_PACKET_CS_LOGIN_RES_LOGIN
{
	dfLOGIN_STATUS_NONE = -1,		// 미인증상태
	dfLOGIN_STATUS_FAIL = 0,		// 세션오류
	dfLOGIN_STATUS_OK = 1,		// 성공
	dfLOGIN_STATUS_GAME = 2,		// 게임중
	dfLOGIN_STATUS_ACCOUNT_MISS = 3,		// account 테이블에 AccountNo 없음
	dfLOGIN_STATUS_SESSION_MISS = 4,		// Session 테이블에 AccountNo 없음
	dfLOGIN_STATUS_STATUS_MISS = 5,		// Status 테이블에 AccountNo 없음
	dfLOGIN_STATUS_NOSERVER = 6,		// 서비스중인 서버가 없음.
};


enum en_PACKET_CS_GAME_RES_LOGIN
{
	dfGAME_LOGIN_FAIL = 0,		// 세션키 오류 또는 Account 데이블상의 오류
	dfGAME_LOGIN_OK = 1,		// 성공
	dfGAME_LOGIN_NOCHARACTER = 2,		// 성공 / 캐릭터 없음 > 캐릭터 선택화면으로 전환. 
	dfGAME_LOGIN_VERSION_MISS = 3,		// 서버,클라 버전 다름
};



enum en_PACKET_SS_LOGINSERVER_LOGIN
{
	dfSERVER_TYPE_GAME = 1,
	dfSERVER_TYPE_CHAT = 2,
	dfSERVER_TYPE_MONITOR = 3,
};

enum en_PACKET_SS_HEARTBEAT
{
	dfTHREAD_TYPE_WORKER = 1,
	dfTHREAD_TYPE_DB = 2,
	dfTHREAD_TYPE_GAME = 3,
};

// en_PACKET_SS_MONITOR_LOGIN
enum en_PACKET_CS_MONITOR_TOOL_SERVER_CONTROL
{
	dfMONITOR_SERVER_TYPE_LOGIN = 1,
	dfMONITOR_SERVER_TYPE_GAME = 2,
	dfMONITOR_SERVER_TYPE_CHAT = 3,
	dfMONITOR_SERVER_TYPE_AGENT = 4,

	dfMONITOR_SERVER_CONTROL_SHUTDOWN = 1,		// 서버 정상종료 (게임서버 전용)
	dfMONITOR_SERVER_CONTROL_TERMINATE = 2,		// 서버 프로세스 강제종료
	dfMONITOR_SERVER_CONTROL_RUN = 3,		// 서버 프로세스 생성 & 실행
};


enum en_PACKET_SS_MONITOR_DATA_UPDATE
{
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN = 1,		// 로그인서버 실행여부 ON / OFF
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU = 2,		// 로그인서버 CPU 사용률
	dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM = 3,		// 로그인서버 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_LOGIN_SESSION = 4,		// 로그인서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS = 5,		// 로그인서버 인증 처리 초당 횟수
	dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL = 6,		// 로그인서버 패킷풀 사용량


	dfMONITOR_DATA_TYPE_GAME_SERVER_RUN = 10,		// GameServer 실행 여부 ON / OFF
	dfMONITOR_DATA_TYPE_GAME_SERVER_CPU = 11,		// GameServer CPU 사용률
	dfMONITOR_DATA_TYPE_GAME_SERVER_MEM = 12,		// GameServer 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_GAME_SESSION = 13,		// 게임서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER = 14,		// 게임서버 AUTH MODE 플레이어 수
	dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER = 15,		// 게임서버 GAME MODE 플레이어 수
	dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS = 16,		// 게임서버 Accept 처리 초당 횟수
	dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS = 17,		// 게임서버 패킷처리 초당 횟수
	dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS = 18,		// 게임서버 패킷 보내기 초당 완료 횟수
	dfMONITOR_DATA_TYPE_GAME_DB_WRITE_TPS = 19,		// 게임서버 DB 저장 메시지 초당 처리 횟수
	dfMONITOR_DATA_TYPE_GAME_DB_WRITE_MSG = 20,		// 게임서버 DB 저장 메시지 큐 개수 (남은 수)
	dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS = 21,		// 게임서버 AUTH 스레드 초당 프레임 수 (루프 수)
	dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS = 22,		// 게임서버 GAME 스레드 초당 프레임 수 (루프 수)
	dfMONITOR_DATA_TYPE_GAME_PACKET_POOL = 23,		// 게임서버 패킷풀 사용량

	dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN = 30,		// 채팅서버 ChatServer 실행 여부 ON / OFF
	dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU = 31,		// 채팅서버 ChatServer CPU 사용률
	dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM = 32,		// 채팅서버 ChatServer 메모리 사용 MByte
	dfMONITOR_DATA_TYPE_CHAT_SESSION = 33,		// 채팅서버 세션 수 (컨넥션 수)
	dfMONITOR_DATA_TYPE_CHAT_PLAYER = 34,		// 채팅서버 인증성공 사용자 수 (실제 접속자)
	dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS = 35,		// 채팅서버 UPDATE 스레드 초당 초리 횟수
	dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL = 36,		// 채팅서버 패킷풀 사용량
	dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL = 37,		// 채팅서버 UPDATE MSG 풀 사용량


	dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL = 40,				// 서버컴퓨터 CPU 전체 사용률
	dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY = 41,		// 서버컴퓨터 논페이지 메모리 MByte
	dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV = 42,			// 서버컴퓨터 네트워크 수신량 KByte
	dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND = 43,			// 서버컴퓨터 네트워크 송신량 KByte
	dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY = 44,		// 서버컴퓨터 사용가능 메모리
};


enum en_PACKET_CS_MONITOR_TOOL_RES_LOGIN
{
	dfMONITOR_TOOL_LOGIN_OK = 1,		// 로그인 성공
	dfMONITOR_TOOL_LOGIN_ERR_NOSERVER = 2,		// 서버이름 오류 (매칭미스)
	dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY = 3,		// 로그인 세션키 오류
};