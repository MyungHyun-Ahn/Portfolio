#pragma once

namespace NetworkCommonLib
{
// 사실 링버퍼랑 같은 역할인데 링 기능은 빠짐
// - 진짜 RecvBuffer로 쓸 용도
class CRecvBuffer
{
public:
	CRecvBuffer() noexcept;

	~CRecvBuffer() noexcept
	{
		m_releasePage(m_pQueue);
	}

	[[nodiscard]] static std::shared_ptr<CRecvBuffer> MakeShared() noexcept
	{
		try
		{
			auto upPage = std::make_unique_for_overwrite<char[]>(kCapacity);
			return std::shared_ptr<CRecvBuffer>(new CRecvBuffer(upPage.release(), freeHeap, freeHeapPage));
		}
		catch (const std::bad_alloc &)
		{
			return {};
		}
	}

	inline int GetCapacity() const noexcept
	{
		return m_capacity;
	}
	// 현재 사용중인 용량 얻기
	inline int GetUseSize() const noexcept
	{
		int r = m_rear;
		int f = m_front;
		return r - f;
	}

	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_rear;
		int f = m_front;
		return r - (f + offset);
	}

	// 현재 버퍼에 남은 용량 얻기
	inline int GetFreeSize() const noexcept
	{
		return m_capacity - GetUseSize();
	}

	// 모든 데이터 삭제
	inline void Clear() noexcept
	{
		m_front = 0; m_rear = 0; m_refCount = 0;
	}

	// 데이터 삽입 데이터 추출 기능은 필요 없음
	// * 직렬화 버퍼로 만들어서 사용할 것
	// * Peek만 필요
	int Peek(char *outBuffer, int size) noexcept
	{
		if (GetUseSize() < size)
		{
			return -1;
		}

		memcpy_s(outBuffer, size, m_pQueue + m_front, size);
		return size;
	}
	int Peek(char *outBuffer, int size, int offset) noexcept
	{
		if (GetUseSize(offset) < size)
		{
			return -1;
		}

		int front = m_front + offset;

		memcpy_s(outBuffer, size, m_pQueue + front, size);

		return 0;
	}

	// Front, Rear 커서 이동
	inline int				MoveRear(int size) noexcept
	{
		m_rear = (m_rear + size); return m_rear;
	}
	inline int				MoveFront(int size) noexcept
	{
		m_front = (m_front + size); return m_front;
	}

	inline char *GetPQueuePtr() const noexcept
	{
		return m_pQueue;
	}

	inline char *GetOffsetPtr(int offset) const noexcept
	{
		return m_pQueue + offset;
	}

	// Front 포인터 얻기
	inline char *GetFrontPtr() const noexcept
	{
		return m_pQueue + m_front;
	}
	inline int GetFrontOffset() const noexcept
	{
		return m_front;
	}

	// Rear 포인터 얻기
	inline char *GetRearPtr() const noexcept
	{
		return m_pQueue + m_rear;
	}
	inline int GetRearOffset() const noexcept
	{
		return m_rear;
	}

	inline LONG IncreaseRef() noexcept
	{
		return InterlockedIncrement(&m_refCount);
	}
	inline LONG DecreaseRef() noexcept
	{
		return InterlockedDecrement(&m_refCount);
	}

	static CRecvBuffer *Alloc() noexcept;

	inline static void Free(CRecvBuffer *pDelBuffer) noexcept
	{
		pDelBuffer->m_releaseBuffer(pDelBuffer);
	}

	static LONG GetPoolCapacity() noexcept;
	static LONG GetPoolUsage() noexcept;

private:
	using BufferRelease = void (*)(CRecvBuffer *) noexcept;
	using PageRelease = void (*)(char *) noexcept;
	static constexpr int kCapacity = 4096;

	CRecvBuffer(char *pPage, BufferRelease releaseBuffer, PageRelease releasePage) noexcept
		: m_pQueue(pPage)
		, m_releaseBuffer(releaseBuffer)
		, m_releasePage(releasePage)
	{
	}

	static void freeHeap(CRecvBuffer *pBuffer) noexcept
	{
		delete pBuffer;
	}

	static void freeHeapPage(char *pPage) noexcept
	{
		delete[] pPage;
	}

	static void freePooled(CRecvBuffer *pBuffer) noexcept;
	static void freePooledPage(char *pPage) noexcept;

	char *m_pQueue;
	BufferRelease m_releaseBuffer;
	PageRelease m_releasePage;
	int m_capacity = kCapacity;

	int				m_front = 0;
	int				m_rear = 0;

	LONG			m_refCount = 0;

};
}
