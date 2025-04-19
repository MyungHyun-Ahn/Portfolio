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
		// ���� Sleep �ð��� ����� �����带 ã�� Enqueue
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

	// �ڱ� �ڽ� �����忡�� Enqueue Event
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

	// Enqueue �̺�Ʈ ť�� Ȯ���ϰ� �ٷ� ���డ���� �̺�Ʈ�� ����
	// Ÿ�̸� �̺�Ʈ�� Ÿ�̸� �̺�Ʈ ť�� ��ť
	void CheckEnqueueEventQ() noexcept
	{
		LONG eventQSize = m_EnqueueEventQ.GetUseSize();
		if (eventQSize != 0)
		{
			for (int i = 0; i < eventQSize; i++)
			{
				BaseEvent *pEvent;
				// ������
				m_EnqueueEventQ.Dequeue(&pEvent);

				if (pEvent->isTimerEvent)
				{
					CLockGuard<LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
					m_TimerEventQueue.push(static_cast<TimerEvent *>(pEvent));
				}
				else
				{
					// ��ȸ�� �̺�Ʈ�� �ٷ� �����ϰ�
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
			InterlockedExchange(&m_EnqueueFlag, FALSE); // ���� �ٲ����

			// �ϰ� �����⿡ �����ϸ�
			if (!WorkStealing())
			{
				// �߰��� �ٸ� Event�� ���ͼ� m_EnqueueFlag�� TRUE�� �ٲ�� ��� �ȵ�
				// ����� ��Ȳ
				WaitOnAddress(&m_EnqueueFlag, &m_EnqueueComparand, sizeof(LONG), INFINITE); // ���� ������ ���
			}
			
			// �ϰ��� �ִ� ó���� ���� continue;
			// WorkStealing ������ Event Enqueue�� �������� �ʱ� ����
			return false;
		}

		return true;
	}

	// ������ ���� ��� WaitOnAddress�� ������ ���� ����
	// ������ ���� ������ WaitOnAddress�� ������ �ʰ� �ڱ� �ڽ��� �Ϸ� ����� �����
	bool WorkStealing() noexcept
	{
		// �ٸ� ������ pq�� ���ɰ� Ȯ���ؼ� job ������
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

				// �� ť �� �ɰ� Enqueue
				AcquireSRWLockExclusive(&m_lockTimerEventQ);
				m_TimerEventQueue.push(pTimerEvent);
				ReleaseSRWLockExclusive(&m_lockTimerEventQ);
				
				InterlockedIncrement(&g_monitor.m_StealWork);

				return true; // �ϳ� �������� break;
			}
			ReleaseSRWLockExclusive(&thread->m_lockTimerEventQ);
		}

		return false;
	}

	// �ڱ� �ڽ��� �ٻ� �� �ٸ� ������� ���� �Ѱ��ִ� �Լ�
	bool DelegateWork() noexcept
	{
		TimerEvent *pTimerEvent;
		{
			CLockGuard<LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
			if (m_TimerEventQueue.empty())
			{
				// ť�� ��������� ����
				return false;
			}

			pTimerEvent = m_TimerEventQueue.top();
			m_TimerEventQueue.pop();
		}

		// ���� SleepTime�� �� �����带 ã�� Enqueue
		EnqueueEvent(pTimerEvent);
		InterlockedIncrement(&g_monitor.m_DelWork);
		return true;
	}

	bool ConsumeTimerEvent() noexcept
	{
		TimerEvent *pTimerEvent;
		LONG dTime;
		{
			CLockGuard<LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);

			if (m_TimerEventQueue.empty())
			{
				return false;
			}
			pTimerEvent = m_TimerEventQueue.top();

			dTime = pTimerEvent->nextExecuteTime - timeGetTime();
			if (dTime > 0) // 0���� ������ ���� ����
			{
				m_PrevSleepTime = dTime;
				Sleep(dTime);  
				return false;
			}

			m_TimerEventQueue.pop();
		}

		// �����ؾ��� �̺�Ʈ��
		if (!pTimerEvent->isRunning)
		{
			delete pTimerEvent;
			return false;
		}

		INT delayFrame;
		if (pTimerEvent->timeMs == 0)
			delayFrame = 0;
		else
			delayFrame = ((dTime * -1) / pTimerEvent->timeMs) + 1;

		pTimerEvent->execute(delayFrame);
		pTimerEvent->nextExecuteTime += pTimerEvent->timeMs * delayFrame;

		// ���������� sleepTime = 0;
		m_PrevSleepTime = 0;

		// dTime�� �����ų� 0
		// �󸶳� �������� �зȴ��� �Ǵ�
		if ((dTime * -1) > ((1000 / SERVER_SETTING::MAX_CONTENT_FPS) * SERVER_SETTING::DELAY_FRAME))
		{
			// �ڱ� �ڽ��� ������ �ִ� �ϰ� �� �ϳ��� �ٸ� ������� �Ѱ���
			DelegateWork();
		}

		CLockGuard<LOCK_TYPE::EXCLUSIVE> lock(m_lockTimerEventQ);
		m_TimerEventQueue.push(pTimerEvent);

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

	// SleepTime�� ���ڸ��� �ٸ� �����忡 �ϰ� �ѱ�
	DWORD					m_PrevSleepTime = INFINITE;

public:
	inline static std::vector<CContentThread *> s_arrContentThreads;
};

