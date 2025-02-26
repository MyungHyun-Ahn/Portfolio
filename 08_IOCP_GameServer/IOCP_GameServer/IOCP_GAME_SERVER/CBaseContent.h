#pragma once

namespace NETWORK_SERVER
{
	class CNetServer;
}

struct MOVE_JOB
{
	UINT64 sessionID;
	// nullptr 넘어오면 해당 컨텐츠에서 알아서 만들어야 함
	void *objectPtr;

	inline MOVE_JOB *Alloc() noexcept
	{
		MOVE_JOB *pMoveJob = s_MoveJobPool.Alloc();
		return pMoveJob;
	}

	inline void Free(MOVE_JOB *freeJob) noexcept
	{
		s_MoveJobPool.Free(freeJob);
	}

	inline static CTLSMemoryPoolManager<MOVE_JOB> s_MoveJobPool = CTLSMemoryPoolManager<MOVE_JOB>();
};

// 모든 콘텐츠는 BaseContent를 상속받아 구현
class CBaseContent
{
public:
	friend class NETWORK_SERVER::CNetServer;
	friend struct ContentFrameEvent;

	CBaseContent() noexcept
	{
		InitializeSRWLock(&m_lockSessionMapLock);
		m_ContentID = InterlockedIncrement(&s_CurrentContentID);
	}

	virtual void OnEnter(const UINT64 sessionID, void *pObject) noexcept = 0;

	// OnLeave에서 유저 수 0 되면 TimerEvent flag = 0 만들기
	virtual void OnLeave(const UINT64 sessionID) noexcept = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept = 0;

protected:
	LONG m_ContentID;
	inline static LONG s_CurrentContentID = 0;

	// 이 컨텐츠 루프를 끝내고 싶으면 m_pTimerEvent의 flag 를 끄면 루프를 돌고 파괴
	TimerEvent *m_pTimerEvent;

	// Player 생성 요청, 삭제 요청 등이 넘어옴
	CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
	CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

	// Key : SessionId
	// Value : 알아서 Player 객체 등의 오브젝트 포인터
	std::unordered_map<UINT64, void *> m_umapSessions;
	SRWLOCK m_lockSessionMapLock;
};

