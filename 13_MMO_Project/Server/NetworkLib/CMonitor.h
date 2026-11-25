#pragma once

namespace ServerNetworkLib
{
struct SNicRate
{
	std::string m_interfaceName;
	double m_totalBytesPerSecond = 0;
};

struct SMonitorSnapshot
{
	// 수집 순서가 바뀌어도 게시할 지표의 의미와 단위가 뒤섞이지 않도록 이름 있는 필드로 전달한다.
	std::optional<double>			m_nonpagedPoolBytes;
	std::optional<double>			m_availableMemoryBytes;
	std::optional<double>			m_tcpV4RetransmittedSegmentsPerSecond;
	std::vector<SNicRate> 			m_nicRates;
	std::string 					m_processName;
	PROCESS_MEMORY_COUNTERS_EX 		m_memory{};
	bool 							m_hasMemory = false;
	double 							m_hostCpu[3]{};
	double 							m_processCpu[3]{};
	bool 							m_hasHostCpu = false;
	bool 							m_hasProcessCpu = false;
	LONG 							m_sessions = 0;
	UINT64 							m_pageFaults = 0;
};

class CMonitor
{
public:
	CMonitor(HANDLE hProcess = INVALID_HANDLE_VALUE) noexcept;
	~CMonitor();

	CMonitor(const CMonitor &) = delete;
	CMonitor &operator=(const CMonitor &) = delete;

	void Update(LONG sessions) noexcept;
	[[nodiscard]] 
	std::shared_ptr<const SMonitorSnapshot> GetSnapshot() noexcept;

private:
	void updateCpuTime(SMonitorSnapshot &outSnapshot) noexcept;
	void updatePdh(SMonitorSnapshot &outSnapshot) noexcept;
	void updateNicRates(SMonitorSnapshot &outSnapshot) noexcept;

private:
	SRWLOCK m_snapshotRWLock = SRWLOCK_INIT;
	std::shared_ptr<const SMonitorSnapshot> m_spSnapshot;
	DWORD m_lastPageFaults = 0;
	UINT64 m_pageFaults = 0;

public:
	LONG m_delWork = 0;
	LONG m_stealWork = 0;

private:
	HANDLE 			m_process;
	std::string 	m_processName;
	DWORD 			m_numberOfProcessors = 0;
	ULARGE_INTEGER 	m_ftProcessorLastKernel{};
	ULARGE_INTEGER 	m_ftProcessorLastUser{};
	ULARGE_INTEGER 	m_ftProcessorLastIdle{};
	ULARGE_INTEGER 	m_ftProcessLastKernel{};
	ULARGE_INTEGER 	m_ftProcessLastUser{};
	ULARGE_INTEGER 	m_ftProcessLastTime{};
	PDH_HQUERY 		m_pdhQuery = nullptr;
	PDH_HCOUNTER 	m_nonpagedPoolCounter = nullptr;
	PDH_HCOUNTER 	m_availableMemoryCounter = nullptr;
	PDH_HCOUNTER 	m_tcpV4RetransmittedSegmentsCounter = nullptr;
	PDH_HCOUNTER 	m_nicCounter = nullptr;
	std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> m_nicBuffer;
	bool m_hasPdhSample = false;
};

extern CMonitor g_monitor;
}
