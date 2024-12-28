#pragma once
// ��� �����۶� ���� �����ε� �� ����� ����
// - ��¥ RecvBuffer�� �� �뵵
class CRecvBuffer
{
public:
	CRecvBuffer()
	{
		m_PQueue = new char[m_iCapacity];
	}
	~CRecvBuffer()
	{
		delete m_PQueue;
	}

	inline int GetCapacity() const { return m_iCapacity; }
	// ���� ������� �뷮 ���
	inline int GetUseSize() const
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - f;
	}

	inline int GetUseSize(int offset) const
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - (f + offset);
	}

	// ���� ���ۿ� ���� �뷮 ���
	inline int GetFreeSize() const { return m_iCapacity - GetUseSize(); }

	// ��� ������ ����
	inline void Clear() { m_iFront = 0; m_iRear = 0; m_iRefCount = 0; }

	// ������ ���� ������ ���� ����� �ʿ� ����
	// * ����ȭ ���۷� ���� ����� ��
	// * Peek�� �ʿ�
	int Peek(char *buffer, int size);
	int Peek(char *buffer, int size, int offset);

	// Front, Rear Ŀ�� �̵�
	inline int				MoveRear(int size) { m_iRear = (m_iRear + size); return m_iRear; }
	inline int				MoveFront(int size) { m_iFront = (m_iFront + size); return m_iFront; }

	inline char *GetPQueuePtr() { return m_PQueue; }

	inline char *GetOffsetPtr(int offset) { return m_PQueue + offset; }

	// Front ������ ���
	inline char *GetFrontPtr() { return m_PQueue + m_iFront; }
	inline int GetFrontOffset() { return m_iFront; }

	// Rear ������ ���
	inline char *GetRearPtr() { return m_PQueue + m_iRear; }
	inline int GetRearOffset() { return m_iRear; }

	inline LONG IncreaseRef() { return InterlockedIncrement(&m_iRefCount); }
	inline LONG DecreaseRef() { return InterlockedDecrement(&m_iRefCount); }

	inline static CRecvBuffer *Alloc()
	{
		CRecvBuffer *pBuffer = s_sbufferPool.Alloc();
		pBuffer->Clear();
		return pBuffer;
	}

	inline static void Free(CRecvBuffer *delBuffer)
	{
		s_sbufferPool.Free(delBuffer);
	}

	inline static LONG GetPoolCapacity() { return s_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sbufferPool.GetUseCount(); }

private:
	char			*m_PQueue = nullptr;

	int				m_iCapacity = 5000;

	int				m_iFront = 0;
	int				m_iRear = 0;

	LONG			m_iRefCount = 0;

	// inline static CLFMemoryPool<CRecvBuffer> s_sbufferPool = CLFMemoryPool<CRecvBuffer>(5000, false);
	inline static CTLSMemoryPoolManager<CRecvBuffer> s_sbufferPool = CTLSMemoryPoolManager<CRecvBuffer>();
};