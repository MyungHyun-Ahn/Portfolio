#pragma once

namespace MetricsLib
{
	struct SMetricsSnapshot;

	class CSystemMetrics
	{
	public:
		// 프로세스 핸들은 빌려서 쓰며, 생략하면 현재 프로세스를 수집한다.
		explicit CSystemMetrics(HANDLE hProcess = INVALID_HANDLE_VALUE) noexcept;
		~CSystemMetrics();

		CSystemMetrics(const CSystemMetrics &) = delete;
		CSystemMetrics &operator=(const CSystemMetrics &) = delete;

		// 한 스레드에서 호출하며, 유효한 호스트·프로세스 지표만 표본에 추가한다.
		void Collect(SMetricsSnapshot &outSnapshot);

	private:
		void collectCpuTime(SMetricsSnapshot &outSnapshot);
		void collectProcessMemory(SMetricsSnapshot &outSnapshot);
		void collectPdh(bool hasInterval, SMetricsSnapshot &outSnapshot);
		void collectNicRates(SMetricsSnapshot &outSnapshot);

	private:
		HANDLE 										m_process;
		std::string 								m_processName;
		DWORD 										m_numberOfProcessors = 0;
		ULARGE_INTEGER 								m_ftProcessorLastKernel{};
		ULARGE_INTEGER 								m_ftProcessorLastUser{};
		ULARGE_INTEGER 								m_ftProcessorLastIdle{};
		ULARGE_INTEGER 								m_ftProcessLastKernel{};
		ULARGE_INTEGER 								m_ftProcessLastUser{};
		ULARGE_INTEGER 								m_ftProcessLastTime{};
		DWORD 										m_lastPageFaults = 0;
		UINT64 										m_pageFaults = 0;
		PDH_HQUERY 									m_pdhQuery = nullptr;
		PDH_HCOUNTER 								m_nonpagedPoolCounter = nullptr;
		PDH_HCOUNTER 								m_availableMemoryCounter = nullptr;
		PDH_HCOUNTER 								m_tcpV4RetransmittedSegmentsCounter = nullptr;
		PDH_HCOUNTER 								m_nicCounter = nullptr;
		std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> 	m_nicBuffer;
		ULONGLONG 									m_lastSampleMilliseconds = 0;
		bool 										m_hasCollected = false;
	};
}
