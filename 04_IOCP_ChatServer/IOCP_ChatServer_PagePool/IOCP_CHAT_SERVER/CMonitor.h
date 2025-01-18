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

public:
	// CPU 모니터링 정보
	HANDLE m_hProcess;
	INT m_iNumberOfProcessors;
	FLOAT m_fProcessorTotal = 0;
	FLOAT m_fProcessorUser = 0;
	FLOAT m_fProcessorKernel = 0;

	LONG m_lAcceptTPS = 0;

	FLOAT m_fProcessTotal = 0;
	FLOAT m_fProcessUser = 0;
	FLOAT m_fProcessKernel = 0;

	LONG m_lRecvTPS = 0;

	ULARGE_INTEGER m_ftProcessor_LastKernel;
	ULARGE_INTEGER m_ftProcessor_LastUser;
	ULARGE_INTEGER m_ftProcessor_LastIdle;

	LONG m_lSendTPS = 0;

	ULARGE_INTEGER m_ftProcess_LastKernel;
	ULARGE_INTEGER m_ftProcess_LastUser;
	ULARGE_INTEGER m_ftProcess_LastTime;

	LONG m_lUpdateTPS = 0;

	// PDH
	PDH_HQUERY  m_PDHQuery;

	PDH_HCOUNTER m_ProcessNPMemoryCounter;
	PDH_HCOUNTER m_SystemNPMemoryCounter;
	PDH_HCOUNTER m_SystemAvailableMemoryCounter;
	PDH_HCOUNTER m_NetworkSendKB1Counter;
	PDH_HCOUNTER m_NetworkSendKB2Counter;
	PDH_HCOUNTER m_NetworkRecvKB1Counter;
	PDH_HCOUNTER m_NetworkRecvKB2Counter;
	PDH_HCOUNTER m_NetworkRetransmissionCounter;

	LONG m_lMaxSendCount = 0;

	PDH_FMT_COUNTERVALUE m_ProcessNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_SystemNPMemoryVal;
	PDH_FMT_COUNTERVALUE m_SystemAvailableMemoryVal;
	PDH_FMT_COUNTERVALUE m_NetworkSendKB1Val;
	PDH_FMT_COUNTERVALUE m_NetworkSendKB2Val;
	PDH_FMT_COUNTERVALUE m_NetworkRecvKB1Val;
	PDH_FMT_COUNTERVALUE m_NetworkRecvKB2Val;
	PDH_FMT_COUNTERVALUE m_NetworkRetransmissionVal;

	// 메모리 사용량 모니터링 정보
	PROCESS_MEMORY_COUNTERS_EX m_stPmc;

	// Server 모니터링 정보
	DWORD m_dwTime = 0;
	INT64 m_lAcceptTotal = 0;
	LONG m_lServerFrame = 0;
};

extern CMonitor g_monitor;

