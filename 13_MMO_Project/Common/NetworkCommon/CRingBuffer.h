#pragma once

namespace NetworkLib
{
// 링버퍼 안씀
// - Recv - recv 버퍼로 교체 -> SerializableBufferView로
// - Send -> WSASend -> 직렬화버퍼 여러개

// 링 버퍼는 오브젝트 풀이 필요 없음
// * 왜냐 - Session에 물려 있다!

class CRingBuffer
{
public:
	CRingBuffer() noexcept
	{
		m_pQueue = new char[m_capacity];
	}
	CRingBuffer(int size)
		: m_capacity(size + 1)
	{
		m_pQueue = new char[m_capacity];
	}
	~CRingBuffer() noexcept
	{
		delete[] m_pQueue;
	}

	// 버퍼 최대 크기
	inline int GetCapacity() const noexcept
	{
		return m_capacity - 1;
	}

	// 현재 사용중인 용량 얻기
	inline int GetUseSize() const noexcept
	{
		int r = m_rear;
		int f = m_front;
		return (r - f + m_capacity) % m_capacity;
	}

	// offset 부터 사용중인 사이즈 계산
	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_rear;
		int f = m_front;
		return (r - (f + offset) + m_capacity) % m_capacity;
	}

	// 현재 버퍼에 남은 용량 얻기
	inline int GetFreeSize() const noexcept
	{
		return m_capacity - GetUseSize() - 1;
	}

	// 모든 데이터 삭제
	inline void Clear() noexcept
	{
		m_front = 0; m_rear = 0;
	}

	// 데이터 삽입
	int				Enqueue(char *pData, int size) noexcept
	{
		// 공간 부족
		int freeSize = GetFreeSize();
		if (freeSize < size)
			return -1;

		int rear = m_rear;
		int des = DirectEnqueueSize();
		int firstPartSize = 0;
		if (des < 0)
			firstPartSize = size;
		else
			firstPartSize = min(size, des);

		int secondPartSize = size - firstPartSize;
		memcpy_s(m_pQueue + rear, freeSize, pData, firstPartSize);
		memcpy_s(m_pQueue, secondPartSize, pData + firstPartSize, secondPartSize);
		MoveRear(size);

		return size;
	}

	// 데이터 추출 - buffer에 저장
	int				Dequeue(char *outBuffer, int size) noexcept
	{
		int ret = Peek(outBuffer, size);
		if (ret == -1)
			return -1;

		MoveFront(ret);
		return ret;
	}

	// 데이터 추출하고 꺼내지는 않음
	int				Peek(char *outBuffer, int size) noexcept
	{
		if (GetUseSize() < size)
		{
			return -1;
		}

		int front = m_front;
		int dds = DirectDequeueSize();
		int firstPartSize = 0;

		if (dds < 0)
			firstPartSize = size;
		else
			firstPartSize = min(size, dds);
		int secondPartSize = size - firstPartSize;

		memcpy_s(outBuffer, size, m_pQueue + front, firstPartSize);
		memcpy_s(outBuffer + firstPartSize, size - firstPartSize, m_pQueue, secondPartSize);

		return size;
	}
	int				Peek(char *outBuffer, int size, int offset) noexcept
	{
		if (GetUseSize(offset) < size)
		{
			return -1;
		}

		int front = m_front;
		front = (front + offset) % m_capacity;
		int dds = DirectDequeueSize(offset);
		int firstPartSize = 0;

		if (dds < 0)
			firstPartSize = size;
		else
			firstPartSize = min(size, dds);

		int secondPartSize = size - firstPartSize;

		memcpy_s(outBuffer, size, m_pQueue + front, firstPartSize);
		memcpy_s(outBuffer + firstPartSize, size - firstPartSize, m_pQueue, secondPartSize);

		return 0;
	}


	// 버퍼 포인터로 외부에서 조작하는 함수들
	// 외부에서 한번에 조작할 수 있는 길이 반환
	inline int DirectEnqueueSize() const noexcept
	{
		int f = m_front;
		int r = m_rear;

		if (f <= r)
		{
			return m_capacity - r - (f == 0 ? 1 : 0);
		}
		else
		{
			return f - r - 1;
		}
	}
	inline int DirectDequeueSize() const noexcept
	{
		int f = m_front;
		int r = m_rear;

		if (f <= r)
		{
			return r - f;
		}
		else
		{
			return m_capacity - f;
		}
	}

	inline int DirectDequeueSize(int offset) const noexcept
	{
		int f = m_front;
		f = (f + offset) % m_capacity;
		int r = m_rear;

		if (f <= r)
		{
			return r - f;
		}
		else
		{
			return m_capacity - f;
		}
	}

	// Front, Rear 커서 이동
	inline int				MoveRear(int size) noexcept
	{
		m_rear = (m_rear + size) % m_capacity; return m_rear;
	}
	inline int				MoveFront(int size) noexcept
	{
		m_front = (m_front + size) % m_capacity; return m_front;
	}

	inline char *GetPQueuePtr() const noexcept
	{
		return m_pQueue;
	}

	// Front 포인터 얻기
	inline char *GetFrontPtr() const noexcept
	{
		return m_pQueue + m_front;
	}

	// Rear 포인터 얻기
	inline char *GetRearPtr() const noexcept
	{
		return m_pQueue + m_rear;
	}

private:
	int				m_capacity = 4096; // 디폴트 1만 바이트 크기, + 1

	int				m_front = 0;
	int				m_rear = 0;

	char *m_pQueue = nullptr;
};
}
