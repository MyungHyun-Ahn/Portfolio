#pragma once

namespace ServerNetworkLib
{
class CServer;


struct SMoveJob
{
	UINT64 m_sessionId;
	// nullptr 넘어오면 해당 컨텐츠에서 알아서 만들어야 함
	void *m_pObject;

	inline static SMoveJob *Alloc() noexcept
	{
		SMoveJob *pMoveJob = sm_moveJobPool.Alloc();
		return pMoveJob;
	}

	inline static void Free(SMoveJob *pFreeJob) noexcept
	{
		sm_moveJobPool.Free(pFreeJob);
	}

	inline static MHLib::memory::CTlsMemoryPoolManager<SMoveJob> sm_moveJobPool = MHLib::memory::CTlsMemoryPoolManager<SMoveJob>();
};

enum class ERecvResult
{
	Received = 0,
	Moved,
	Failed
};

// 모든 콘텐츠는 BaseContent를 상속받아 구현
class CBaseContents
{
public:
	friend class ServerNetworkLib::CServer;
	friend struct SContentsFrameEvent;
	friend class CMonitor;

	CBaseContents() noexcept
	{
		m_contentsId = InterlockedIncrement(&sm_currentContentsId);
	}

	// 컨텐츠가 삭제되는 것 -> 한 프레임이 끝나고 락걸고 삭제
	~CBaseContents() noexcept
	{
		ProcessMoveJob();
		ProcessLeaveJob();
	}

	void MoveJobEnqueue(UINT64 sessionId, void *pObject) noexcept;
	void LeaveJobEnqueue(UINT64 sessionId);
	void ProcessMoveJob() noexcept;
	void ProcessLeaveJob() noexcept;
	void ProcessRecvMsg(int delayFrame) noexcept;

	virtual void OnEnter(const UINT64 sessionId, void *pObject) noexcept = 0;
	virtual void OnLeave(const UINT64 sessionId) noexcept = 0;
	virtual ERecvResult OnRecv(const UINT64 sessionId, CSerializableBufferView *pMessage, int delayFrame) noexcept = 0;
	virtual void OnLoopEnd() noexcept = 0;

protected:
	LONG m_contentsId;
	inline static LONG sm_currentContentsId = 0;

	// 이 컨텐츠 루프를 끝내고 싶으면 m_pTimerEvent의 flag 를 끄면 루프를 돌고 파괴
	STimerEvent *m_pTimerEvent;

	// Player 생성 요청, 삭제 요청 등이 넘어옴
	CLfQueue<SMoveJob *> m_moveJobQ; // Move Job Queue
	CLfQueue<UINT64> m_leaveJobQ; // Leave Job Queue

	LONG m_fps = 0;

	// Key : SessionId
	// Value : 알아서 Player 객체 등의 오브젝트 포인터

	std::unordered_map<UINT64, void *> m_umapSessions;
};
}
