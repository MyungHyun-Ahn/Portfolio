#pragma once

namespace NetworkLib
{
class CNetServer;


class CContentsThread
{
public:
	friend class NetworkLib::CNetServer;
	CContentsThread()
	{
		InitializeSRWLock(&m_timerEventQueueRWLock);
	}
	~CContentsThread()
	{
		CloseHandle(m_threadHandle);
	}

	bool Create() noexcept
	{
		m_runningFlag = TRUE;
		m_threadHandle = (HANDLE)_beginthreadex(nullptr, 0, &CContentsThread::threadFunc, this, CREATE_SUSPENDED, nullptr);
		if (m_threadHandle == nullptr)
		{
			const int workerError = errno;
			g_pLogger->WriteEmergencyLogToStderr(
				"[Error][NetworkLib] event=contents_worker_start_failed error_code=%d | 콘텐츠 워커를 생성하지 못했습니다.\n",
				workerError);
		}
		sm_arrContentsThreads.push_back(this);
		return m_threadHandle != nullptr;
	}

	void Resume() noexcept
	{
		if (m_threadHandle != INVALID_HANDLE_VALUE)
		{
			ResumeThread(m_threadHandle);
		}
	}

	static void RunAll() noexcept
	{
		for (CContentsThread *pThread : sm_arrContentsThreads)
		{
			if (pThread != nullptr && pThread->m_threadHandle != INVALID_HANDLE_VALUE)
			{
				pThread->Resume();
			}
		}
	}

	static void EnqueueEvent(SBaseEvent *pEvent)
	{
		CContentsThread *pTargetThread = nullptr;
		DWORD maxDTime = 0;
		for (CContentsThread *pThread : sm_arrContentsThreads)
		{
			if (pThread->m_prevSleepTime >= maxDTime)
			{
				maxDTime = pThread->m_prevSleepTime;
				pTargetThread = pThread;
			}
		}
		pTargetThread->EnqueueEventMy(pEvent);
	}

	// 자기 자신 스레드에게 Enqueue Event
	void EnqueueEventMy(SBaseEvent *pEvent)
	{
		m_enqueueEventQ.Enqueue(pEvent);
		InterlockedExchange(&m_enqueueFlag, TRUE);
		WakeByAddressSingle(&m_enqueueFlag);
	}

private:
	static unsigned __stdcall threadFunc(void *pThis) noexcept
	{
		CContentsThread *pThread = static_cast<CContentsThread *>(pThis);
		g_pLogger->RegisterLogProducer();
		g_pLogger->WriteLog(MHLib::utils::ELogLevel::Info, L"NetworkLib", 0, std::source_location::current(),
			L"event=contents_worker_started | 콘텐츠 워커를 시작했습니다.");
		pThread->run();
		return 0;
	}

	// Enqueue 이벤트 큐를 확인하고 바로 수행가능한 이벤트면 수행
	// 타이머 이벤트면 타이머 이벤트 큐에 인큐
	void checkEnqueueEventQ() noexcept
	{
		LONG eventQSize = m_enqueueEventQ.GetUseSize();
		if (eventQSize != 0)
		{
			for (int i = 0; i < eventQSize; i++)
			{
				SBaseEvent *pEvent;
				if (!m_enqueueEventQ.Dequeue(&pEvent))
					break;

				if (pEvent->m_isTimerEvent)
				{
					CLockGuard<ELockType::Exclusive> timerEventQueueLock(m_timerEventQueueRWLock);
					m_timerEventQueue.push(static_cast<STimerEvent *>(pEvent));
				}
				else
				{
					// 일회성 이벤트는 바로 실행하고
					pEvent->Execute(1);
					delete pEvent;
				}
			}
		}
	}

	bool checkTimerEventQ() noexcept
	{
		if (m_timerEventQueue.empty())
		{
			InterlockedExchange(&m_enqueueFlag, FALSE);
			if (!workStealing())
			{
				m_prevSleepTime = INFINITE;
				WaitOnAddress(&m_enqueueFlag, &m_enqueueComparand
					, sizeof(LONG), INFINITE);
			}
			return false;
		}
		return true;
	}

	// 할일이 없는 경우 WaitOnAddress로 빠지기 전에 수행
	// 가져온 일이 있으면 WaitOnAddress로 빠지지 않고 자기 자신의 일로 만들고 수행됨

	bool workStealing() noexcept
	{
		for (CContentsThread *pThread : sm_arrContentsThreads)
		{
			if (pThread == this)
				continue;

			AcquireSRWLockExclusive(&pThread->m_timerEventQueueRWLock);
			if (pThread->m_timerEventQueue.size() != 0)
			{
				STimerEvent *pTimerEvent = pThread->m_timerEventQueue.top();
				pThread->m_timerEventQueue.pop();
				ReleaseSRWLockExclusive(&pThread->m_timerEventQueueRWLock);

				AcquireSRWLockExclusive(&m_timerEventQueueRWLock);
				m_timerEventQueue.push(pTimerEvent);
				ReleaseSRWLockExclusive(&m_timerEventQueueRWLock);
				
				InterlockedIncrement(&g_monitor.m_stealWork);

				return true;
			}
			ReleaseSRWLockExclusive(&pThread->m_timerEventQueueRWLock);
		}

		return false;
	}

	// 자기 자신이 바쁠 때 다른 스레드로 일을 넘겨주는 함수

	bool delegateWork() noexcept
	{
		STimerEvent *pTimerEvent;
		{
			CLockGuard<ELockType::Exclusive> timerEventQueueLock(m_timerEventQueueRWLock);
			if (m_timerEventQueue.empty())
			{
				return false;
			}

			pTimerEvent = m_timerEventQueue.top();
			m_timerEventQueue.pop();
		}

		EnqueueEvent(pTimerEvent);
		InterlockedIncrement(&g_monitor.m_delWork);
		return true;
	}

	void processTimerEvent() noexcept
	{
		STimerEvent *pTimerEvent;
		LONG dTime;
		{
			CLockGuard<ELockType::Exclusive> timerEventQueueLock(m_timerEventQueueRWLock);

			if (m_timerEventQueue.empty())
			{
				return;
			}

			pTimerEvent = m_timerEventQueue.top();
			dTime = pTimerEvent->m_nextExecuteTime - timeGetTime();
			if (dTime > 0)
			{
				m_prevSleepTime = dTime;
				Sleep(dTime);  
				return;
			}

			m_timerEventQueue.pop();
		}

		// 종료해야할 이벤트면
		if (!pTimerEvent->m_isRunning)
		{
			delete pTimerEvent;
			return;
		}

		INT delayFrame;
		if (pTimerEvent->m_timeMilliseconds == 0)
			delayFrame = 0;
		else
			delayFrame = ((dTime * -1) / pTimerEvent->m_timeMilliseconds) + 1;

		pTimerEvent->Execute(delayFrame);
		pTimerEvent->m_nextExecuteTime += pTimerEvent->m_timeMilliseconds * delayFrame;

		if ((dTime * -1) > 
			((1000 / serverSetting::maxContentsFps) * serverSetting::delayFrame))
		{
			delegateWork();
		}

		CLockGuard<ELockType::Exclusive> timerEventQueueLock(m_timerEventQueueRWLock);
		m_timerEventQueue.push(pTimerEvent);

		return;
	}

	void run() noexcept
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		while (m_runningFlag)
		{
			checkEnqueueEventQ();

			if (!checkTimerEventQ())
				continue;

			processTimerEvent();
		}
	}

private:
	HANDLE					m_threadHandle = INVALID_HANDLE_VALUE;
	BOOL					m_runningFlag = FALSE;

	CLfQueue<SBaseEvent *>	m_enqueueEventQ;
	LONG					m_enqueueFlag = FALSE;
	LONG					m_enqueueComparand = FALSE;

	// TimerEvent Queue
	std::priority_queue<STimerEvent *, std::vector<STimerEvent *>, STimerEventComparator>	m_timerEventQueue;
	SRWLOCK					m_timerEventQueueRWLock;

	// SleepTime이 모자르면 다른 스레드에 일감 넘김
	DWORD					m_prevSleepTime = INFINITE;

public:
	inline static std::vector<CContentsThread *> sm_arrContentsThreads;
};
}
