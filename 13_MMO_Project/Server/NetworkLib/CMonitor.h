#pragma once

namespace NetworkLib
{
class CMonitor
{
public:
	CMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE) noexcept;

	void Update() noexcept;
	void UpdateCpuTime() noexcept;
	void UpdateServer() noexcept;
	void UpdateMemory() noexcept;
	void UpdatePdh() noexcept;

public:
	// Interlocked 사용 변수들 띄워놓음

	LONG m_acceptTps = 0;
	FLOAT m_processTotal = 0; // 8

	// CPU 모니터링 정보
	HANDLE m_process;

	INT m_numberOfProcessors; // 16
	FLOAT m_processorTotal = 0;

	FLOAT m_processorUser = 0; // 24
	FLOAT m_processorKernel = 0;

	FLOAT m_processUser = 0; // 32
	FLOAT m_processKernel = 0; // 36

	ULARGE_INTEGER m_ftProcessorLastKernel; // 48
	ULARGE_INTEGER m_ftProcessorLastUser;   // 56
	ULARGE_INTEGER m_ftProcessorLastIdle;   // 64

	// 64

	INT m_dummy01; // 패딩 계산용

	ULARGE_INTEGER m_ftProcessLastKernel;
	ULARGE_INTEGER m_ftProcessLastUser;
	ULARGE_INTEGER m_ftProcessLastTime; // 24
	PDH_HQUERY  m_pdhQuery;  // 32

	PDH_HCOUNTER m_processNPMemoryCounter;
	PDH_HCOUNTER m_systemNPMemoryCounter;
	PDH_HCOUNTER m_systemAvailableMemoryCounter;
	
	// 128

	INT m_dummy02; // 패딩 계산용
	// PDH
	PDH_HCOUNTER m_networkSendKB1Counter;

	PDH_HCOUNTER m_networkSendKB2Counter;
	PDH_HCOUNTER m_networkRecvKB1Counter;
	PDH_HCOUNTER m_networkRecvKB2Counter;

	PDH_FMT_COUNTERVALUE m_processNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_systemNPMemoryVal;

	// 192

	LONG m_updateTps = 0;
	DWORD m_time = 0;
	PDH_HCOUNTER m_networkRetransmissionCounter; // 16
	PDH_FMT_COUNTERVALUE m_systemAvailableMemoryVal; // 32
	PDH_FMT_COUNTERVALUE m_networkSendKB1Val; // 48
	PDH_FMT_COUNTERVALUE m_networkSendKB2Val; // 64

	int m_dummy03;
	PDH_FMT_COUNTERVALUE m_networkRecvKB1Val;
	PDH_FMT_COUNTERVALUE m_networkRecvKB2Val;
	PDH_FMT_COUNTERVALUE m_networkRetransmissionVal;
	UINT64 m_dummy04;

	INT64 m_acceptTotal = 0;

	PROCESS_MEMORY_COUNTERS_EX m_pmc;

	LONG m_serverFrame = 0;

	//
	UINT64 m_loopCount = 0;
	UINT64 m_acceptTpsTotal = 0;
	UINT64 m_recvTpsTotal = 0;
	UINT64 m_sendTpsTotal = 0;
	UINT64 m_updateTpsTotal = 0;

	// 초당 메시지 발생 빈도 체크
	LONG m_loginReq = 0;
	LONG m_sectorMoveReq = 0;
	LONG m_chatMsgReq = 0;

	// SendPacket 호출 횟수
	LONG m_chatMsgRes = 0;
	LONG m_sendPendingCount = 0;
	LONG m_sendCallCount = 0;
	LONG m_delWork = 0;
	LONG m_stealWork = 0;
};

extern CMonitor g_monitor;
}
