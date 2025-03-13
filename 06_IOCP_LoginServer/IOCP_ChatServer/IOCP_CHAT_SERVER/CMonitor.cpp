#include "pch.h"
#include "CMonitor.h"
#include "CSector.h"
#include "CPlayer.h"
#include "CNetServer.h"
#include "CLanClient.h"
#include "CMonitorClient.h"
#include "CGenPacket.h"
#include "CommonProtocol.h"
#include "ChatSetting.h"
#include "CChatServer.h"

CMonitor g_monitor;

CMonitor::CMonitor(HANDLE hProcess) noexcept
{
	if (hProcess == INVALID_HANDLE_VALUE)
	{
		m_hProcess = GetCurrentProcess();
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
	m_iNumberOfProcessors = sysInfo.dwNumberOfProcessors;

	m_ftProcessor_LastKernel.QuadPart = 0;
	m_ftProcessor_LastUser.QuadPart = 0;
	m_ftProcessor_LastIdle.QuadPart = 0;

	m_ftProcess_LastKernel.QuadPart = 0;
	m_ftProcess_LastUser.QuadPart = 0;
	m_ftProcess_LastTime.QuadPart = 0;

	PdhOpenQuery(NULL, NULL, &m_PDHQuery);

	PdhAddCounter(m_PDHQuery, L"\\Process(IOCP_CHAT_SERVER)\\Pool Nonpaged Bytes", NULL, &m_ProcessNPMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &m_SystemNPMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Memory\\Available MBytes", NULL, &m_SystemAvailableMemoryCounter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Sent/sec", NULL, &m_NetworkSendKB1Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Sent/sec", NULL, &m_NetworkSendKB2Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller)\\Bytes Received/sec", NULL, &m_NetworkRecvKB1Counter);
	PdhAddCounter(m_PDHQuery, L"\\Network Interface(Realtek PCIe GBE Family Controller _2)\\Bytes Received/sec", NULL, &m_NetworkRecvKB2Counter);
	PdhAddCounter(m_PDHQuery, L"\\TCPv4\\Segments Retransmitted/sec", NULL, &m_NetworkRetransmissionCounter);
	
	// 첫 갱신
	PdhCollectQueryData(m_PDHQuery);

	UpdateCpuTime();
	UpdatePDH();
}

void CMonitor::Update(INT sessionCount, INT playerCount) noexcept
{
	UpdateCpuTime();
	UpdateMemory();
	UpdatePDH();

	// 모니터링 콘솔
	MonitoringConsole(sessionCount, playerCount);
	SendMonitoringServer(sessionCount, playerCount);

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

	ULONGLONG kernelDiff = kernel.QuadPart - m_ftProcessor_LastKernel.QuadPart;
	ULONGLONG userDiff = user.QuadPart - m_ftProcessor_LastUser.QuadPart;
	ULONGLONG idleDiff = idle.QuadPart - m_ftProcessor_LastIdle.QuadPart;

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

void CMonitor::UpdateServer() noexcept
{
	m_LoopCount++;
	m_AcceptTPSTotal += m_lAcceptTPS;
	InterlockedExchange(&m_lAcceptTPS, 0);
	m_RecvTPSTotal += m_lRecvTPS;
	InterlockedExchange(&m_lRecvTPS, 0);
	m_SendTPSTotal += m_lSendTPS;
	InterlockedExchange(&m_lSendTPS, 0);
	m_UpdateTPSTotal += m_lUpdateTPS;
	InterlockedExchange(&m_lUpdateTPS, 0);

	InterlockedExchange(&m_loginReq, 0);
	InterlockedExchange(&m_sectorMoveReq, 0);
	InterlockedExchange(&m_chatMsgReq, 0);
	InterlockedExchange(&m_chatMsgRes, 0);
	InterlockedExchange(&m_sendQEnqueueCount, 0);

	InterlockedExchange(&m_lMaxSendCount, 0);
	InterlockedExchange(&m_lServerFrame, 0);
}

void CMonitor::UpdateMemory() noexcept
{
	GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&m_stPmc, sizeof(m_stPmc));
}

void CMonitor::UpdatePDH() noexcept
{
	PdhCollectQueryData(m_PDHQuery);

	PdhGetFormattedCounterValue(m_ProcessNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_ProcessNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemNPMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemNPMemoryVal);
	PdhGetFormattedCounterValue(m_SystemAvailableMemoryCounter, PDH_FMT_LARGE, NULL, &m_SystemAvailableMemoryVal);
	PdhGetFormattedCounterValue(m_NetworkSendKB1Counter, PDH_FMT_LARGE, NULL, &m_NetworkSendKB1Val);
	PdhGetFormattedCounterValue(m_NetworkSendKB2Counter, PDH_FMT_LARGE, NULL, &m_NetworkSendKB2Val);
	PdhGetFormattedCounterValue(m_NetworkRecvKB1Counter, PDH_FMT_LARGE, NULL, &m_NetworkRecvKB1Val);
	PdhGetFormattedCounterValue(m_NetworkRecvKB2Counter, PDH_FMT_LARGE, NULL, &m_NetworkRecvKB2Val);
	PdhGetFormattedCounterValue(m_NetworkRetransmissionCounter, PDH_FMT_LARGE, NULL, &m_NetworkRetransmissionVal);
}

void CMonitor::MonitoringConsole(INT sessionCount, INT playerCount) noexcept
{
	// Cpu Usage
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"              System info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process memory usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\ttotal \t: %u MB", m_stPmc.PrivateUsage / (1024 * 1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u KB\n", m_ProcessNPMemoryVal.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"System memory usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tavailable \t: %u MB", m_SystemAvailableMemoryVal.largeValue);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tnon-paged pool \t: %u MB\n", m_SystemNPMemoryVal.largeValue / (1024 * 1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Processor CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessorTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessorUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessorKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Process CPU usage");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tTotal \t: %f", m_fProcessTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUser \t: %f", m_fProcessUser);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tKernel \t: %f\n", m_fProcessKernel);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Network");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend1 \t: %u KB", m_NetworkSendKB1Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend2 \t: %u KB", m_NetworkSendKB2Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv1 \t: %u KB", m_NetworkRecvKB1Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv2 \t: %u KB", m_NetworkRecvKB2Val.largeValue / (1024));
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRetransmission \t: %u\n", m_NetworkRetransmissionVal.largeValue);
	

	// Server Monitor
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"        Server monitoring info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Info");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tServer FPS \t: %lld", m_lServerFrame);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept total \t: %lld", m_lAcceptTotal);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession count \t: %d", sessionCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer count \t: %d\n", playerCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tMsg queue \t: %d\n", ((CChatServer *)NET_SERVER::g_NetServer)->m_RecvJobQ.GetUseSize());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"Pool capacity");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSession pool capacity \t: %d, usage \t: %d", NET_SERVER::CNetSession::GetPoolCapacity(), NET_SERVER::CNetSession::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tBuffer pool capacity \t: %d, usage \t: %d", CSerializableBuffer<FALSE>::GetPoolCapacity(), CSerializableBuffer<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tView pool capacity \t: %d, usage \t: %d", CSerializableBufferView<FALSE>::GetPoolCapacity(), CSerializableBufferView<FALSE>::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv pool capacity \t: %d, usage \t: %d", CRecvBuffer::GetPoolCapacity(), CRecvBuffer::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tPlayer pool capacity \t: %d, usage \t: %d\n", CPlayer::GetPoolCapacity(), CPlayer::GetPoolUsage());
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"TPS");
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tAccept\t : %d \t Avr : %lf", m_lAcceptTPS, (DOUBLE)m_AcceptTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tRecv\t : %d \t Avr : %lf", m_lRecvTPS, (DOUBLE)m_RecvTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSend\t : %d \t Avr : %lf", m_lSendTPS, (DOUBLE)m_SendTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tUpdate\t : %d \t Avr : %lf", m_lUpdateTPS, (DOUBLE)m_UpdateTPSTotal / m_LoopCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tMaxSend\t : %d", m_lMaxSendCount);
	// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tLoginReq\t : %d\t\tSectorMoveReq\t : %d", m_loginReq, m_sectorMoveReq);
	// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tChatMsgReq\t : %d\t\tChatMsgRes\t : %d", m_chatMsgReq, m_chatMsgRes);
	// g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"\tSendQEnqueue\t : %d", m_sendQEnqueueCount);
	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"----------------------------------------");
}

void CMonitor::SendMonitoringServer(INT sessionCount, INT playerCount) noexcept
{
	int currentTime = time(NULL);
	CSerializableBuffer<TRUE> *pChatServerRunBuffer 
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN
			, TRUE, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerRunBuffer);

	CSerializableBuffer<TRUE> *pCpuTotalBuffer 
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL
			, m_fProcessorTotal, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pCpuTotalBuffer);

	CSerializableBuffer<TRUE> *pNonpagedMemBuffer 
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY
			, m_SystemNPMemoryVal.largeValue / (1024 * 1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNonpagedMemBuffer);

	CSerializableBuffer<TRUE> *pNetworkRecvBuffer 
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV
			, m_NetworkRecvKB1Val.largeValue / (1024) + m_NetworkRecvKB2Val.largeValue / (1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNetworkRecvBuffer);

	CSerializableBuffer<TRUE> *pNetworkSendBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND
			, m_NetworkSendKB1Val.largeValue / (1024) + m_NetworkSendKB2Val.largeValue / (1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pNetworkSendBuffer);

	CSerializableBuffer<TRUE> *pAvailableMemBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY
			, m_SystemAvailableMemoryVal.largeValue, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pAvailableMemBuffer);

	CSerializableBuffer<TRUE> *pChatServerCpuBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU
			, m_fProcessTotal, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerCpuBuffer);

	CSerializableBuffer<TRUE> *pChatServerMemBuffer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM
			, m_stPmc.PrivateUsage / (1024 * 1024), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerMemBuffer);
	
	CSerializableBuffer<TRUE> *pChatServerSession
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_SESSION
			, sessionCount, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerSession);

	CSerializableBuffer<TRUE> *pChatServerPlayer
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_PLAYER
			, playerCount, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerPlayer);

	CSerializableBuffer<TRUE> *pChatServerUpdateTPS
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS
			, m_lUpdateTPS, currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerUpdateTPS);

	CSerializableBuffer<TRUE> *pChatServerPacketPool
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL
			, CSerializableBuffer<FALSE>::GetPoolCapacity(), currentTime);
	g_MonitorClient->EnqueuePacket(m_MonitorClientSessionId, pChatServerPacketPool);

	CSerializableBuffer<TRUE> *pChatServerViewPool
		= CGenPacket::makePacketReqMonitorUpdate(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL
			, CSerializableBufferView<FALSE>::GetPoolCapacity(), currentTime);
	g_MonitorClient->SendPacket(m_MonitorClientSessionId, pChatServerViewPool);
}
