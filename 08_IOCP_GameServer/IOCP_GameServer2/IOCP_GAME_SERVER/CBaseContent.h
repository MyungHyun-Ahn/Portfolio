#pragma once

namespace NET_SERVER
{
	class CNetServer;
}

struct MOVE_JOB
{
	UINT64 sessionID;
	// nullptr 넘어오면 해당 컨텐츠에서 알아서 만들어야 함
	void *objectPtr;

	inline static MOVE_JOB *Alloc() noexcept
	{
		MOVE_JOB *pMoveJob = s_MoveJobPool.Alloc();
		return pMoveJob;
	}

	inline static void Free(MOVE_JOB *freeJob) noexcept
	{
		s_MoveJobPool.Free(freeJob);
	}

	inline static CTLSMemoryPoolManager<MOVE_JOB> s_MoveJobPool = CTLSMemoryPoolManager<MOVE_JOB>();
};

enum class RECV_RET
{
	RECV_TRUE = 0,
	RECV_MOVE,
	RECV_FALSE
};

// 모든 콘텐츠는 BaseContent를 상속받아 구현
class CBaseContent
{
public:
	friend class NET_SERVER::CNetServer;
	friend struct ContentFrameEvent;
	friend class CMonitor;

	CBaseContent() noexcept
	{
		InitializeSRWLock(&m_lockSessionMapLock);
		m_ContentID = InterlockedIncrement(&s_CurrentContentID);
	}

	// 컨텐츠가 삭제되는 것 -> 한 프레임이 끝나고 락걸고 삭제
	~CBaseContent() noexcept
	{
		ConsumeMoveJob();
		ConsumeLeaveJob();
	}

	void MoveJobEnqueue(UINT64 sessionID, void *pObject) noexcept;
	void LeaveJobEnqueue(UINT64 sessionID);
	void ConsumeMoveJob() noexcept;
	void ConsumeLeaveJob() noexcept;
	void ConsumeRecvMsg() noexcept;

	virtual void OnEnter(const UINT64 sessionID, void *pObject) noexcept = 0;
	virtual void OnLeave(const UINT64 sessionID) noexcept = 0;
	virtual RECV_RET OnRecv(const UINT64 sessionID, CSerializableBuffer<FALSE> *message) noexcept = 0;
	virtual void OnLoopEnd() noexcept = 0;

protected:
	LONG m_ContentID;
	inline static LONG s_CurrentContentID = 0;

	// 이 컨텐츠 루프를 끝내고 싶으면 m_pTimerEvent의 flag 를 끄면 루프를 돌고 파괴
	TimerEvent *m_pTimerEvent;

	// Player 생성 요청, 삭제 요청 등이 넘어옴
	CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
	CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

	LONG m_FPS = 0;

	// Key : SessionId
	// Value : 알아서 Player 객체 등의 오브젝트 포인터
	std::unordered_map<UINT64, void *> m_umapSessions;
	SRWLOCK m_lockSessionMapLock;
};

