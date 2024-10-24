#pragma once

class CRingBuffer
{
public:
	CRingBuffer();
	CRingBuffer(int size);
	~CRingBuffer();

	// 버퍼 최대 크기
	inline int GetCapacity() const { return m_iCapacity - 1; }

	// 현재 사용중인 용량 얻기
	inline int GetUseSize() const
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - f + m_iCapacity) % m_iCapacity;
	}

	// offset 부터 사용중인 사이즈 계산
	inline int GetUseSize(int offset) const
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - (f + offset) + m_iCapacity) % m_iCapacity;
	}

	// 현재 버퍼에 남은 용량 얻기
	inline int GetFreeSize() const { return m_iCapacity - GetUseSize() - 1; }

	// 모든 데이터 삭제
	inline void Clear() { m_iFront = 0; m_iRear = 0; }

	// 데이터 삽입
	int				Enqueue(char *data, int size);

	// 데이터 추출 - buffer에 저장
	int				Dequeue(char *buffer, int size);

	// 데이터 추출하고 꺼내지는 않음
	int				Peek(char *buffer, int size);
	int				Peek(char *buffer, int size, int offset);


	// 버퍼 포인터로 외부에서 조작하는 함수들
	// 외부에서 한번에 조작할 수 있는 길이 반환
	inline int DirectEnqueueSize() const
	{
		int f = m_iFront;
		int r = m_iRear;

		if (f <= r)
		{
			return m_iCapacity - r - (f == 0 ? 1 : 0);
		}
		else
		{
			return f - r - 1;
		}
	}
	inline int DirectDequeueSize() const
	{
		int f = m_iFront;
		int r = m_iRear;

		if (f <= r)
		{
			return r - f;
		}
		else
		{
			return m_iCapacity - f;
		}
	}

	inline int DirectDequeueSize(int offset) const
	{
		int f = m_iFront;
		f = (f + offset) % m_iCapacity;
		int r = m_iRear;

		if (f <= r)
		{
			return r - f;
		}
		else
		{
			return m_iCapacity - f;
		}
	}

	// Front, Rear 커서 이동
	inline int				MoveRear(int size) { m_iRear = (m_iRear + size) % m_iCapacity; return m_iRear; }
	inline int				MoveFront(int size) { m_iFront = (m_iFront + size) % m_iCapacity; return m_iFront; }

	inline char *GetPQueuePtr() { return m_PQueue; }

	// Front 포인터 얻기
	inline char *GetFrontPtr() { return m_PQueue + m_iFront; }

	// Rear 포인터 얻기
	inline char *GetRearPtr() { return m_PQueue + m_iRear; }

private:
	int				m_iCapacity = 15000 + 1; // 디폴트 1만 바이트 크기, + 1

	int				m_iFront = 0;
	int				m_iRear = 0;

	char *m_PQueue = nullptr;
};


