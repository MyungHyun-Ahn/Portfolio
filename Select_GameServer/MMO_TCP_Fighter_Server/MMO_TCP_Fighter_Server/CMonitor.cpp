#include "pch.h"
#include "Config.h"
#include "CMonitor.h"

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess)
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_hProcess = GetCurrentProcess();
	}

	// 프로세서 개수 확인
	//  * 프로세스 실행률 계산시 cpu 개수로 나누어 실제 사용률을 구함
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

	m_ftProcessor_LastKernel.QuadPart = 0;
	m_ftProcessor_LastUser.QuadPart = 0;
	m_ftProcessor_LastIdle.QuadPart = 0;

	m_ftProcess_LastKernel.QuadPart = 0;
	m_ftProcess_LastUser.QuadPart = 0;
	m_ftProcess_LastTime.QuadPart = 0;

	UpdateCpuTime();
}

void CMonitor::Update(INT sessionCount, INT playerCount)
{
	UpdateCpuTime();
	UpdateMemory();

	if (m_lFPS != FRAME_PER_SECOND)
		g_Logger->WriteLog(L"ServerFPS", LOG_LEVEL::SYSTEM, L"FPS %d", m_lFPS);

	// FPS가 5으로 떨어지면 Crash 유도
	if (m_lFPS < 5)
	{
		MonitoringFile(sessionCount, playerCount);
		CCrashDump::Crash();
	}

	// 모니터링 콘솔
	MonitoringConsole(sessionCount, playerCount);

	UpdateServer();
}

void CMonitor::UpdateCpuTime()
{
	// 프로세서 사용률을 갱신
	// FILETIME 구조체를 사용하지만 ULARGE_INTEGER와 구조가 같으므로 이를 사용
	ULARGE_INTEGER idle;
	ULARGE_INTEGER kernel;
	ULARGE_INTEGER user;

	// 시스템 사용 시간을 구함
	// 아이들 타임 / 커널 사용 타임(아이들 포함) / 유저 사용 타임
	if (GetSystemTimes((PFILETIME)&idle, (PFILETIME)&kernel, (PFILETIME)&user) == false)
		return;

	ULONGLONG kernelDiff	= kernel.QuadPart	- m_ftProcessor_LastKernel.QuadPart;
	ULONGLONG userDiff		= user.QuadPart		- m_ftProcessor_LastUser.QuadPart;
	ULONGLONG idleDiff		= idle.QuadPart		- m_ftProcessor_LastIdle.QuadPart;

	ULONGLONG total = kernelDiff + userDiff;
	ULONGLONG timeDiff;

	if (total == 0)
	{
		m_fProcessorUser = 0.0f;
		m_fProcessorKernel = 0.0f;
		m_fProcessorTotal = 0.0f;
	}
	else
	{
		m_fProcessorTotal = (float)((double)(total - idleDiff) / total * 100.0f);
		m_fProcessorUser = (float)((double)(userDiff) / total * 100.0f);
		m_fProcessorKernel = (float)((double)(kernelDiff - idleDiff) / total * 100.0f);
	}

	m_ftProcessor_LastKernel = kernel;
	m_ftProcessor_LastUser = user;
	m_ftProcessor_LastIdle = idle;

	// 지정된 프로세스 사용률을 갱신
	ULARGE_INTEGER none;
	ULARGE_INTEGER nowTime;

	// 100 나노세컨드 단위 시간을 구함. UTC
	// 프로세스 사용률 판단 공식
	// a = 샘플 간격의 시스템 시간을 구함 (실제 흐른 시간)
	// b = 프로세스의 CPU 사용 시간을 구함
	// a : 100 = b : 사용률 공식을 통해 사용률을 계산


	// 100 나노세컨드 단위의 시간 구함
	GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

	// 해당 프로세스가 사용한 시간을 구함
	// 두번째, 세번째는 실행, 종료 시간으로 미사용
	GetProcessTimes(m_hProcess, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);

	// 이전에 저장된 프로세스 시간과의 차를 구해 실제로 얼마의 시간이 지났는지 확인
	// 실제 지나온 시간으로 나누면 사용률
	timeDiff = nowTime.QuadPart - m_ftProcess_LastTime.QuadPart;
	userDiff = user.QuadPart - m_ftProcess_LastUser.QuadPart;
	kernelDiff = kernel.QuadPart - m_ftProcess_LastKernel.QuadPart;

	total = kernelDiff + userDiff;

	m_fProcessTotal = (float)(total / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);
	m_fProcessKernel = (float)(kernelDiff / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);
	m_fProcessUser = (float)(userDiff / (double)m_iNumberOfProcessors / (double)timeDiff * 100.0f);

	m_ftProcess_LastTime = nowTime;
	m_ftProcess_LastKernel = kernel;
	m_ftProcess_LastUser = user;
}

void CMonitor::UpdateServer()
{
	InterlockedExchange(&m_lLoopCount, 0);
	InterlockedExchange(&m_lFPS, 0);
	InterlockedExchange(&m_lAcceptTPS, 0);
	InterlockedExchange(&m_lRecvTPS, 0);
	InterlockedExchange(&m_lSendTPS, 0);
}

void CMonitor::UpdateMemory()
{
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_stPmc, sizeof(m_stPmc));
}

void CMonitor::MonitoringConsole(INT sessionCount, INT playerCount)
{
	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"              System info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Memory usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tMemory \t: %u MB\n", m_stPmc.PrivateUsage / (1024 * 1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Processor CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);

	// Server Monitor
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"        Server monitoring info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer count  \t: %d\n", playerCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Frame");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tFPS \t : %d\n\tLoop\t : %d\n", m_lFPS, m_lLoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept\t : %d", m_lAcceptTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}

void CMonitor::MonitoringFile(INT sessionCount, INT playerCount)
{
	// Cpu Usage
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"             Cpu Usage");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Processor usage");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Process Usage");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tKernel \t: %f", m_fProcessKernel);
					  
	// Server Monitor 
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\n\n\n        Server monitoring info");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tPlayer count  \t: %d\n", playerCount);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"Frame");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tFPS \t : %d\n\tLoop\t : %d\n", m_lFPS, m_lLoopCount);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tAccept\t : %d", m_lAcceptTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tRecv\t : %d", m_lRecvTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"\tSend\t : %d", m_lSendTPS);
	g_Logger->WriteLog(L"LowerFPS", LOG_LEVEL::SYSTEM, L"----------------------------------------");
}