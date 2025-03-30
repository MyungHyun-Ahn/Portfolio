#pragma once

namespace NET_SERVER
{
	class CNetServer;
}

struct MOVE_JOB
{
	UINT64 sessionID;
	// nullptr �Ѿ���� �ش� ���������� �˾Ƽ� ������ ��
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

// ��� �������� BaseContent�� ��ӹ޾� ����
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

	// �������� �����Ǵ� �� -> �� �������� ������ ���ɰ� ����
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

	// �� ������ ������ ������ ������ m_pTimerEvent�� flag �� ���� ������ ���� �ı�
	TimerEvent *m_pTimerEvent;

	// Player ���� ��û, ���� ��û ���� �Ѿ��
	CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
	CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

	LONG m_FPS = 0;

	// Key : SessionId
	// Value : �˾Ƽ� Player ��ü ���� ������Ʈ ������
	std::unordered_map<UINT64, void *> m_umapSessions;
	SRWLOCK m_lockSessionMapLock;
};

