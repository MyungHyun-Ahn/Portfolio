#include "pch.h"
#include "SNetworkCounters.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CNetServer.h"
#include "SystemEvent.h"
#include "CBaseContents.h"
#include "CContentsThread.h"

namespace NetworkLib
{
CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess) noexcept
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_process = GetCurrentProcess();
	}

	// 콘솔 출력에 맞는 크기로 변경
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r);
	MoveWindow(console, r.left, r.top, 700, 900, TRUE);

	// 프로세서 개수 확인
	//  * 프로세스 실행률 계산시 cpu 개수로 나누어 실제 사용률을 구함
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	m_numberOfProcessors = sysInfo.dwNumberOfProcessors;

	m_ftProcessorLastKernel.QuadPart = 0;
	m_ftProcessorLastUser.QuadPart = 0;
	m_ftProcessorLastIdle.QuadPart = 0;

	m_ftProcessLastKernel.QuadPart = 0;
	m_ftProcessLastUser.QuadPart = 0;
	m_ftProcessLastTime.QuadPart = 0;

	PdhOpenQuery(NULL, NULL, &m_pdhQuery);

	// TODO: 런타임에 PDH 프로세스명과 NIC명을 로드할 수 있는지 알아보고 개선한다.
	PdhAddCounter(m_pdhQuery, L"\\Process(IOCP_CHAT_SERVER)\\Pool Nonpaged Bytes", NULL, &m_processNPMemoryCounter);
	PdhAddCounter(m_pdhQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_systemNPMemoryCounter);
	PdhAddCounter(m_pdhQuery, L"\\Memory\\Available MBytes", NULL, &m_systemAvailableMemoryCounter);
	PdhAddCounter(m_pdhQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Sent/sec", NULL, &m_networkSendKB1Counter);
	PdhAddCounter(m_pdhQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Sent/sec",
		NULL, &m_networkSendKB2Counter);
	PdhAddCounter(m_pdhQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Received/sec",
		NULL, &m_networkRecvKB1Counter);
	PdhAddCounter(m_pdhQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Received/sec",
		NULL, &m_networkRecvKB2Counter);
	PdhAddCounter(m_pdhQuery, L"\\TCPv4\\Segments Retransmitted/sec", NULL, &m_networkRetransmissionCounter);
	
	// 첫 갱신
	PdhCollectQueryData(m_pdhQuery);

	UpdateCpuTime();
	UpdatePdh();
}

void CMonitor::Update(INT sessionCount) noexcept
{
	UpdateCpuTime();
	UpdateMemory();
	UpdatePdh();

	// 모니터링 콘솔
	MonitoringConsole(sessionCount);

	UpdateServer();
}

void CMonitor::UpdateCpuTime() noexcept
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

	ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessorLastKernel.QuadPart;
	ULONGLONG userDiff = user.QuadPart - m_ftProcessorLastUser.QuadPart;
	ULONGLONG idleDiff = idle.QuadPart - m_ftProcessorLastIdle.QuadPart;

	ULONGLONG total = kernelDiff + userDiff;
	ULONGLONG timeDiff;

	if (total == 0)
	{
		m_processorUser = 0.0f;
		m_processorKernel = 0.0f;
		m_processorTotal = 0.0f;
	}
	else
	{
		m_processorTotal = (float)((double)(total - idleDiff) / total * 100.0f);
		m_processorUser = (float)((double)(userDiff) / total * 100.0f);
		m_processorKernel = (float)((double)(kernelDiff - idleDiff) / total * 100.0f);
	}

	m_ftProcessorLastKernel = kernel;
	m_ftProcessorLastUser = user;
	m_ftProcessorLastIdle = idle;

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
	GetProcessTimes(m_process, (LPFILETIME)&none, (LPFILETIME)&none, (LPFILETIME)&kernel, (LPFILETIME)&user);

	// 이전에 저장된 프로세스 시간과의 차를 구해 실제로 얼마의 시간이 지났는지 확인
	// 실제 지나온 시간으로 나누면 사용률
	timeDiff = nowTime.QuadPart - m_ftProcessLastTime.QuadPart;
	userDiff = user.QuadPart - m_ftProcessLastUser.QuadPart;
	kernelDiff = kernel.QuadPart - m_ftProcessLastKernel.QuadPart;

	total = kernelDiff + userDiff;

	m_processTotal = (float)(total / (double)m_numberOfProcessors / (double)timeDiff * 100.0f);
	m_processKernel = (float)(kernelDiff / (double)m_numberOfProcessors / (double)timeDiff * 100.0f);
	m_processUser = (float)(userDiff / (double)m_numberOfProcessors / (double)timeDiff * 100.0f);

	m_ftProcessLastTime = nowTime;
	m_ftProcessLastKernel = kernel;
	m_ftProcessLastUser = user;
}

void CMonitor::UpdateServer() noexcept
{
	m_loopCount++;
	InterlockedExchange(&m_delWork, 0);
	InterlockedExchange(&m_stealWork, 0);
	m_acceptTpsTotal += m_acceptTps;
	InterlockedExchange(&m_acceptTps, 0);
	m_recvTpsTotal += g_networkCounters.m_recvTps;
	InterlockedExchange(&g_networkCounters.m_recvTps, 0);
	m_sendTpsTotal += g_networkCounters.m_sendTps;
	InterlockedExchange(&g_networkCounters.m_sendTps, 0);
	m_updateTpsTotal += m_updateTps;
	InterlockedExchange(&m_updateTps, 0);

	InterlockedExchange(&m_loginReq, 0);
	InterlockedExchange(&m_sectorMoveReq, 0);
	InterlockedExchange(&m_chatMsgReq, 0);
	InterlockedExchange(&m_chatMsgRes, 0);

	InterlockedExchange(&g_networkCounters.m_maxSendCount, 0);
	InterlockedExchange(&m_serverFrame, 0);
}

void CMonitor::UpdateMemory() noexcept
{
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_pmc, sizeof(m_pmc));
}

void CMonitor::UpdatePdh() noexcept
{
	PdhCollectQueryData(m_pdhQuery);

	PdhGetFormattedCounterValue(m_processNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_processNPMemoryVal);
	PdhGetFormattedCounterValue(m_systemNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_systemNPMemoryVal);
	PdhGetFormattedCounterValue(m_systemAvailableMemoryCounter, PDH_FMT_LARGE, NULL, &m_systemAvailableMemoryVal);
	PdhGetFormattedCounterValue(m_networkSendKB1Counter, PDH_FMT_LARGE, NULL, &m_networkSendKB1Val);
	PdhGetFormattedCounterValue(m_networkSendKB2Counter, PDH_FMT_LARGE, NULL, &m_networkSendKB2Val);
	PdhGetFormattedCounterValue(m_networkRecvKB1Counter, PDH_FMT_LARGE, NULL, &m_networkRecvKB1Val);
	PdhGetFormattedCounterValue(m_networkRecvKB2Counter, PDH_FMT_LARGE, NULL, &m_networkRecvKB2Val);
	PdhGetFormattedCounterValue(m_networkRetransmissionCounter, PDH_FMT_LARGE, NULL, &m_networkRetransmissionVal);
}

void CMonitor::MonitoringConsole(INT sessionCount) noexcept
{
	// Cpu Usage
	g_pLogger->WriteLogConsole(ELogLevel::System, L"----------------------------------------");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"              System info");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Process memory usage");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\ttotal \t: %u MB", m_pmc.PrivateUsage / (1024 * 1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tnon-paged pool \t: %u KB\n", m_processNPMemoryVal.largeValue / (1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"System memory usage");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tavailable \t: %u MB", m_systemAvailableMemoryVal.largeValue);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tnon-paged pool \t: %u MB\n", m_systemNPMemoryVal.largeValue / (1024 * 1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Processor CPU usage");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tTotal \t: %f", m_processorTotal);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tUser \t: %f", m_processorUser);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tKernel \t: %f\n", m_processorKernel);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Process CPU usage");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tTotal \t: %f", m_processTotal);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tUser \t: %f", m_processUser);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tKernel \t: %f\n", m_processKernel);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Network");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tSend1 \t: %u KB", m_networkSendKB1Val.largeValue / (1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tSend2 \t: %u KB", m_networkSendKB2Val.largeValue / (1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tRecv1 \t: %u KB", m_networkRecvKB1Val.largeValue / (1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tRecv2 \t: %u KB", m_networkRecvKB2Val.largeValue / (1024));
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tRetransmission \t: %u\n", m_networkRetransmissionVal.largeValue);
	

	g_pLogger->WriteLogConsole(ELogLevel::System, L"Send calls : %d, pending : %d", m_sendCallCount, m_sendPendingCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"DelWork : %d, StealWork : %d", m_delWork, m_stealWork);
	// Server Monitor
	g_pLogger->WriteLogConsole(ELogLevel::System, L"        Server monitoring info");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Info");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tAccept total \t: %lld", m_acceptTotal);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tSession count \t: %d", sessionCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"Pool capacity");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tSession pool capacity \t: %d, usage \t: %d",
		NetworkLib::CNetSession::GetPoolCapacity(), NetworkLib::CNetSession::GetPoolUsage());
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tBuffer pool capacity \t: %d, usage \t: %d",
		CSerializableBuffer<FALSE>::GetPoolCapacity(), CSerializableBuffer<FALSE>::GetPoolUsage());
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tView pool capacity \t: %d, usage \t: %d",
		CSerializableBufferView<FALSE>::GetPoolCapacity(), CSerializableBufferView<FALSE>::GetPoolUsage());
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tRecv pool capacity \t: %d, usage \t: %d",
		CRecvBuffer::GetPoolCapacity(), CRecvBuffer::GetPoolUsage());
	g_pLogger->WriteLogConsole(ELogLevel::System, L"TPS");
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tAccept\t : %d \t Avr : %lf", m_acceptTps, (double)m_acceptTpsTotal / m_loopCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tRecv\t : %d \t Avr : %lf",
		g_networkCounters.m_recvTps, (double)m_recvTpsTotal / m_loopCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tSend\t : %d \t Avr : %lf",
		g_networkCounters.m_sendTps, (double)m_sendTpsTotal / m_loopCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tUpdate\t : %d \t Avr : %lf", m_updateTps, (double)m_updateTpsTotal / m_loopCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tMaxSend\t : %d", g_networkCounters.m_maxSendCount);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tLoginReq\t : %d\t\tSectorMoveReq\t : %d", m_loginReq, m_sectorMoveReq);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"\tChatMsgReq\t : %d\t\tChatMsgRes\t : %d", m_chatMsgReq, m_chatMsgRes);
	g_pLogger->WriteLogConsole(ELogLevel::System, L"----------------------------------------");
}
}
