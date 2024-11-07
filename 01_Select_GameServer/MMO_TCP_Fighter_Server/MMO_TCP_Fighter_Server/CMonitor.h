#pragma once
class CMonitor
{
public:
	CMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void Update(INT sessionCount, INT playerCount);
	void UpdateCpuTime();
	void UpdateServer();
	void UpdateMemory();

	void MonitoringConsole(INT sessionCount, INT playerCount);
	void MonitoringFile(INT sessionCount, INT playerCount);

public:
	// CPU 모니터링 정보
	HANDLE m_hProcess;
	INT m_iNumberOfProcessors;
	FLOAT m_fProcessorTotal = 0;
	FLOAT m_fProcessorUser = 0;
	FLOAT m_fProcessorKernel = 0;

	FLOAT m_fProcessTotal = 0;
	FLOAT m_fProcessUser = 0;
	FLOAT m_fProcessKernel = 0;

	ULARGE_INTEGER m_ftProcessor_LastKernel;
	ULARGE_INTEGER m_ftProcessor_LastUser;
	ULARGE_INTEGER m_ftProcessor_LastIdle;

	ULARGE_INTEGER m_ftProcess_LastKernel;
	ULARGE_INTEGER m_ftProcess_LastUser;
	ULARGE_INTEGER m_ftProcess_LastTime;

	// 메모리 사용량 모니터링 정보
	PROCESS_MEMORY_COUNTERS_EX m_stPmc;

	// Server 모니터링 정보
	DWORD m_dwTime = 0;
	LONG m_lLoopCount = 0;
	LONG m_lFPS = 0;
	LONG m_lAcceptTPS = 0;
	LONG m_lRecvTPS = 0;
	LONG m_lSendTPS = 0;
};

extern CMonitor g_monitor;