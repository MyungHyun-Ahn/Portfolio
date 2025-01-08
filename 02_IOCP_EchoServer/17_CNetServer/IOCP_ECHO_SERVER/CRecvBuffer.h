#pragma once
// 사실 링버퍼랑 같은 역할인데 링 기능은 빠짐
// - 진짜 RecvBuffer로 쓸 용도
class CRecvBuffer
{
public:
	CRecvBuffer() noexcept
	{
		m_PQueue = new char[m_iCapacity];
	}
	~CRecvBuffer() noexcept
	{
		delete m_PQueue;
	}

	inline int GetCapacity() const noexcept { return m_iCapacity; }
	// 현재 사용중인 용량 얻기
	inline int GetUseSize() const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - f;
	}

	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return r - (f + offset);
	}

	// 현재 버퍼에 남은 용량 얻기
	inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize(); }

	// 모든 데이터 삭제
	inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; m_iRefCount = 0; }

	// 데이터 삽입 데이터 추출 기능은 필요 없음
	// * 직렬화 버퍼로 만들어서 사용할 것
	// * Peek만 필요
	int Peek(char *buffer, int size) noexcept;
	int Peek(char *buffer, int size, int offset) noexcept;

	// Front, Rear 커서 이동
	inline int				MoveRear(int size) noexcept { m_iRear = (m_iRear + size); return m_iRear; }
	inline int				MoveFront(int size) noexcept { m_iFront = (m_iFront + size); return m_iFront; }

	inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

	inline char *GetOffsetPtr(int offset) const noexcept { return m_PQueue + offset; }

	// Front 포인터 얻기
	inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }
	inline int GetFrontOffset() const noexcept { return m_iFront; }

	// Rear 포인터 얻기
	inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }
	inline int GetRearOffset() const noexcept { return m_iRear; }

	inline LONG IncreaseRef() noexcept { return InterlockedIncrement(&m_iRefCount); }
	inline LONG DecreaseRef() noexcept { return InterlockedDecrement(&m_iRefCount); }

	inline static CRecvBuffer *Alloc() noexcept
	{
		CRecvBuffer *pBuffer = s_sbufferPool.Alloc();
		pBuffer->Clear();
		return pBuffer;
	}

	inline static void Free(CRecvBuffer *delBuffer) noexcept
	{
		s_sbufferPool.Free(delBuffer);
	}

	inline static LONG GetPoolCapacity() noexcept { return s_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return s_sbufferPool.GetUseCount(); }

private:
	char			*m_PQueue = nullptr;

	int				m_iCapacity = 5000;

	int				m_iFront = 0;
	int				m_iRear = 0;

	LONG			m_iRefCount = 0;

	// inline static CLFMemoryPool<CRecvBuffer> s_sbufferPool = CLFMemoryPool<CRecvBuffer>(5000, false);
	inline static CTLSMemoryPoolManager<CRecvBuffer, 16, 4> s_sbufferPool = CTLSMemoryPoolManager<CRecvBuffer, 16, 4>();
};