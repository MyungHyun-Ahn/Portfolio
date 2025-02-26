#pragma once

namespace NETWORK_SERVER
{
	class CNetServer;
}

struct MOVE_JOB
{
	UINT64 sessionID;
	// nullptr �Ѿ���� �ش� ���������� �˾Ƽ� ������ ��
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

// ��� �������� BaseContent�� ��ӹ޾� ����
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

	// OnLeave���� ���� �� 0 �Ǹ� TimerEvent flag = 0 �����
	virtual void OnLeave(const UINT64 sessionID) noexcept = 0;
	virtual void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept = 0;

protected:
	LONG m_ContentID;
	inline static LONG s_CurrentContentID = 0;

	// �� ������ ������ ������ ������ m_pTimerEvent�� flag �� ���� ������ ���� �ı�
	TimerEvent *m_pTimerEvent;

	// Player ���� ��û, ���� ��û ���� �Ѿ��
	CLFQueue<MOVE_JOB *> m_MoveJobQ; // Move Job Queue
	CLFQueue<UINT64> m_LeaveJobQ; // Leave Job Queue

	// Key : SessionId
	// Value : �˾Ƽ� Player ��ü ���� ������Ʈ ������
	std::unordered_map<UINT64, void *> m_umapSessions;
	SRWLOCK m_lockSessionMapLock;
};

