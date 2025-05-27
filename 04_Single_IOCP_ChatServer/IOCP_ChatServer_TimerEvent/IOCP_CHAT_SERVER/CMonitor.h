#pragma once
class CMonitor
{
public:
	CMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE) noexcept;

	void Update(INT sessionCount, INT playerCount) noexcept;
	void UpdateCpuTime() noexcept;
	void UpdateServer() noexcept;
	void UpdateMemory() noexcept;
	void UpdatePDH() noexcept;

	void MonitoringConsole(INT sessionCount, INT playerCount) noexcept;
	void SendMonitoringServer(INT sessionCount, INT playerCount) noexcept;

	inline void SetMonitorClient(UINT64 sessionId) { m_MonitorClientSessionId = sessionId; }

public:
	// Interlocked 사용 변수들 띄워놓음

	LONG m_lAcceptTPS = 0;
	FLOAT m_fProcessTotal = 0; // 8

	// CPU 모니터링 정보
	HANDLE m_hProcess;

	INT m_iNumberOfProcessors; // 16
	FLOAT m_fProcessorTotal = 0;

	FLOAT m_fProcessorUser = 0; // 24
	FLOAT m_fProcessorKernel = 0;

	FLOAT m_fProcessUser = 0; // 32
	FLOAT m_fProcessKernel = 0; // 36

	ULARGE_INTEGER m_ftProcessor_LastKernel; // 48
	ULARGE_INTEGER m_ftProcessor_LastUser;   // 56
	ULARGE_INTEGER m_ftProcessor_LastIdle;   // 64

	// 64

	LONG m_lRecvTPS = 0;
	INT dummy01; // 패딩 계산용

	ULARGE_INTEGER m_ftProcess_LastKernel;
	ULARGE_INTEGER m_ftProcess_LastUser;
	ULARGE_INTEGER m_ftProcess_LastTime; // 24
	PDH_HQUERY  m_PDHQuery;  // 32

	PDH_HCOUNTER m_ProcessNPMemoryCounter;
	PDH_HCOUNTER m_SystemNPMemoryCounter;
	PDH_HCOUNTER m_SystemAvailableMemoryCounter;
	
	// 128

	LONG m_lSendTPS = 0;
	INT dummy02; // 패딩 계산용
	// PDH
	PDH_HCOUNTER m_NetworkSendKB1Counter;

	PDH_HCOUNTER m_NetworkSendKB2Counter;
	PDH_HCOUNTER m_NetworkRecvKB1Counter;
	PDH_HCOUNTER m_NetworkRecvKB2Counter;

	PDH_FMT_COUNTERVALUE m_ProcessNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_SystemNPMemoryVal;

	// 192

	LONG m_lUpdateTPS = 0;
	DWORD m_dwTime = 0;
	PDH_HCOUNTER m_NetworkRetransmissionCounter; // 16
	PDH_FMT_COUNTERVALUE m_SystemAvailableMemoryVal; // 32
	PDH_FMT_COUNTERVALUE m_NetworkSendKB1Val; // 48
	PDH_FMT_COUNTERVALUE m_NetworkSendKB2Val; // 64

	LONG m_lMaxSendCount = 0;
	int dummy03;
	PDH_FMT_COUNTERVALUE m_NetworkRecvKB1Val;
	PDH_FMT_COUNTERVALUE m_NetworkRecvKB2Val;
	PDH_FMT_COUNTERVALUE m_NetworkRetransmissionVal;
	UINT64 dummy04;

	INT64 m_lAcceptTotal = 0;

	PROCESS_MEMORY_COUNTERS_EX m_stPmc;

	LONG m_lServerFrame = 0;

	//
	UINT64 m_LoopCount = 0;
	UINT64 m_AcceptTPSTotal = 0;
	UINT64 m_RecvTPSTotal = 0;
	UINT64 m_SendTPSTotal = 0;
	UINT64 m_UpdateTPSTotal = 0;

	// 초당 메시지 발생 빈도 체크
	LONG m_loginReq = 0;
	LONG m_sectorMoveReq = 0;
	LONG m_chatMsgReq = 0;

	LONG m_sendQEnqueueCount = 0;

	// SendPacket 호출 횟수
	LONG m_chatMsgRes = 0;

	UINT64 m_MonitorClientSessionId = 0;

	tm m_startTime;
	tm m_currentTime;

	LONG m_WSASendCall = 0;
};

extern CMonitor g_monitor;