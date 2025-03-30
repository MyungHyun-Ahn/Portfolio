#pragma once

namespace NET_SERVER
{
	class CNetServer;
}

class CContentThread
{
public:
	friend class NET_SERVER::CNetServer;
	CContentThread() { InitializeSRWLock(&m_lockTimerEventQ); }
	~CContentThread() { CloseHandle(m_ThreadHandle); }

	bool Start() noexcept
	{
		m_RunningFlag = TRUE;
		m_ThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, &CContentThread::ThreadFunc, this, CREATE_SUSPENDED, nullptr);
		s_arrContentThreads.push_back(this);
		return m_ThreadHandle != nullptr;
	}

	void Resume() noexcept
	{
		if (m_ThreadHandle != INVALID_HANDLE_VALUE)
		{
			ResumeThread(m_ThreadHandle);
		}
	}

	static void RunAll() noexcept
	{
		for (CContentThread *thread : s_arrContentThreads)
		{
			if (thread != nullptr && thread->m_ThreadHandle != INVALID_HANDLE_VALUE)
			{
				thread->Resume();
			}
		}
	}

	static void EnqueueEvent(BaseEvent *pEvent)
	{
		// 가장 Sleep 시간이 길었던 스레드를 찾아 Enqueue
		CContentThread *targetThread = nullptr;
		DWORD maxDTime = 0;
		for (CContentThread *thread : s_arrContentThreads)
		{
			if (thread->m_PrevSleepTime >= maxDTime)
			{
				maxDTime = thread->m_PrevSleepTime;
				targetThread = thread;
			}
		}

		if (targetThread != nullptr)
			targetThread->EnqueueEventMy(pEvent);
	}

	// 자기 자신 스레드에게 Enqueue Event
	void EnqueueEventMy(BaseEvent *pEvent)
	{
		m_EnqueueEventQ.Enqueue(pEvent);
		InterlockedExchange(&m_EnqueueFlag, TRUE);
		WakeByAddressSingle(&m_EnqueueFlag);
	}

private:
	static unsigned __stdcall ThreadFunc(void *pThis) noexcept
	{
		CContentThread *pThread = static_cast<CContentThread *>(pThis);
		pThread->Run();
		return 0;
	}

	// Enqueue 이벤트 큐를 확인하고 바로 수행가능한 이벤트면 수행
	// 타이머 이벤트면 타이머 이벤트 큐에 인큐
	void CheckEnqueueEventQ() noexcept
	{
		LONG eventQSize = m_EnqueueEventQ.GetUseSize();
		if (eventQSize != 0)
		{
			for (int i = 0; i < eventQSize; i++)
			{
				BaseEvent *pEvent;
				// 꺼내고
				m_EnqueueEventQ.Dequeue(&pEvent);

				if (pEvent->isTimerEvent)
				{
					AcquireSRWLockExclusive(&m_lockTimerEventQ);
					m_TimerEventQueue.push(static_cast<TimerEvent *>(pEvent));
					ReleaseSRWLockExclusive(&m_lockTimerEventQ);
				}
				else
				{
					// 일회성 이벤트는 바로 실행하고
					pEvent->execute(1);
					delete pEvent;
				}
			}
		}
	}

	bool CheckTimerEventQ() noexcept
	{
		if (m_TimerEventQueue.empty())
		{
			InterlockedExchange(&m_EnqueueFlag, FALSE); // 먼저 바꿔놓고
			WaitOnAddress(&m_EnqueueFlag, &m_EnqueueComparand, sizeof(LONG), INFINITE); // 값이 같으면 블락

			// 일감 얻어오기에 실패하면
			// if (!WorkStealing())
			// {
			// 	// 중간에 다른 Event가 들어와서 m_EnqueueFlag가 TRUE로 바뀌면 블락 안됨
			// 	// 블락될 상황
			// 	WaitOnAddress(&m_EnqueueFlag, &m_EnqueueComparand, sizeof(LONG), INFINITE); // 값이 같으면 블락
			// }
			
			// 일관성 있는 처리를 위해 continue;
			// WorkStealing 성공과 Event Enqueue와 구분하지 않기 위해
			return false;
		}

		return true;
	}

	// 할일이 없는 경우 WaitOnAddress로 빠지기 전에 수행
	// 가져온 일이 있으면 WaitOnAddress로 빠지지 않고 자기 자신의 일로 만들고 수행됨
	bool WorkStealing() noexcept
	{
		// 다른 스레드 pq를 락걸고 확인해서 job 빼오기
		for (CContentThread *thread : s_arrContentThreads)
		{
			if (thread == this)
				continue;

			AcquireSRWLockExclusive(&thread->m_lockTimerEventQ);
			if (thread->m_TimerEventQueue.size() != 0)
			{
				TimerEvent *pTimerEvent = thread->m_TimerEventQueue.top();
				thread->m_TimerEventQueue.pop();
				ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);

				// 내 큐 락 걸고 Enqueue
				AcquireSRWLockExclusive(&m_lockTimerEventQ);
				m_TimerEventQueue.push(pTimerEvent);
				ReleaseSRWLockExclusive(&m_lockTimerEventQ);
				return true; // 하나 빼왔으면 break;
			}
			ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);
		}

		return false;
	}

	// 자기 자신이 바쁠 때 다른 스레드로 일을 넘겨주는 함수
	bool DelegateWork() noexcept
	{
		AcquireSRWLockExclusive(&m_lockTimerEventQ);
		if (m_TimerEventQueue.empty())
		{
			// 큐가 비어있으면 실패
			ReleaseSRWLockExclusive(&m_lockTimerEventQ);
			return false;
		}

		TimerEvent *pTimerEvent = m_TimerEventQueue.top();
		m_TimerEventQueue.pop();
		ReleaseSRWLockExclusive(&m_lockTimerEventQ);

		// 가장 SleepTime이 긴 스레드를 찾아 Enqueue
		EnqueueEvent(pTimerEvent);
		return true;
	}

	bool ConsumeTimerEvent() noexcept
	{
		TimerEvent *pTimerEvent;
		AcquireSRWLockExclusive(&m_lockTimerEventQ);
		if (m_TimerEventQueue.empty())
		{
			ReleaseSRWLockExclusive(&m_lockTimerEventQ);
			return false;
		}
		pTimerEvent = m_TimerEventQueue.top();

		LONG dTime = pTimerEvent->nextExecuteTime - timeGetTime();
		if (dTime > 0) // 0보다 작으면 수행 가능
		{
			m_PrevSleepTime = dTime;
			ReleaseSRWLockExclusive(&m_lockTimerEventQ);
			Sleep(dTime);
			return false;
		}

		m_TimerEventQueue.pop();
		ReleaseSRWLockExclusive(&m_lockTimerEventQ);

		// dTime이 음수거나 0
		// 얼마나 프레임이 밀렸는지 판단
		// if ((dTime * -1) > ((1000 / SERVER_SETTING::MAX_CONTENT_FPS) * SERVER_SETTING::DELAY_FRAME))
		// {
		// 	// 자기 자신이 가지고 있는 일감 중 하나를 다른 스레드로 넘겨줌
		// 	// DelegateWork();
		// }

		// 종료해야할 이벤트면
		// if (!pTimerEvent->isRunning)
		// {
		// 	delete pTimerEvent;
		// 	return false;
		// }

		INT delayFrame;
		if (pTimerEvent->timeMs == 0)
			delayFrame = 0;
		else
			delayFrame = ((dTime * -1) / pTimerEvent->timeMs) + 1;

		pTimerEvent->execute(delayFrame);
		pTimerEvent->nextExecuteTime += pTimerEvent->timeMs * delayFrame;

		// 수행했으면 sleepTime = 0;
		m_PrevSleepTime = 0;

		AcquireSRWLockExclusive(&m_lockTimerEventQ);
		m_TimerEventQueue.push(pTimerEvent);
		ReleaseSRWLockExclusive(&m_lockTimerEventQ);

		return true;
	}

	void Run() noexcept
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		while (m_RunningFlag)
		{
			CheckEnqueueEventQ();

			if (!CheckTimerEventQ())
				continue;

			ConsumeTimerEvent();
		}
	}

private:
	HANDLE					m_ThreadHandle = INVALID_HANDLE_VALUE;
	BOOL					m_RunningFlag = FALSE;

	CLFQueue<BaseEvent *>	m_EnqueueEventQ;
	LONG					m_EnqueueFlag = FALSE;
	LONG					m_EnqueueComparand = FALSE;

	// TimerEvent Queue
	std::priority_queue<TimerEvent *, std::vector<TimerEvent *>, TimerEventComparator>	m_TimerEventQueue;
	SRWLOCK					m_lockTimerEventQ;

	// SleepTime이 모자르면 다른 스레드에 일감 넘김
	DWORD					m_PrevSleepTime = INFINITE;

public:
	inline static std::vector<CContentThread *> s_arrContentThreads;
};

