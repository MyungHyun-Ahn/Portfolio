#pragma once

// �� ���۴� ������Ʈ Ǯ�� �ʿ� ����
// * �ֳ� - Session�� ���� �ִ�!

class CRingBuffer
{
public:
	CRingBuffer() noexcept;
	CRingBuffer(int size) noexcept;
	~CRingBuffer() noexcept;

	// ���� �ִ� ũ��
	inline int GetCapacity() const noexcept { return m_iCapacity - 1; }

	// ���� ������� �뷮 ���
	inline int GetUseSize() const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - f + m_iCapacity) % m_iCapacity;
	}

	// offset ���� ������� ������ ���
	inline int GetUseSize(int offset) const noexcept
	{
		int r = m_iRear;
		int f = m_iFront;
		return (r - (f + offset) + m_iCapacity) % m_iCapacity;
	}

	// ���� ���ۿ� ���� �뷮 ���
	inline int GetFreeSize() const noexcept { return m_iCapacity - GetUseSize() - 1; }

	// ��� ������ ����
	inline void Clear() noexcept { m_iFront = 0; m_iRear = 0; }

	// ������ ����
	int				Enqueue(char *data, int size) noexcept;

	// ������ ���� - buffer�� ����
	int				Dequeue(char *buffer, int size) noexcept;

	// ������ �����ϰ� �������� ����
	int				Peek(char *buffer, int size) noexcept;
	int				Peek(char *buffer, int size, int offset) noexcept;


	// ���� �����ͷ� �ܺο��� �����ϴ� �Լ���
	// �ܺο��� �ѹ��� ������ �� �ִ� ���� ��ȯ
	inline int DirectEnqueueSize() const noexcept
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
	inline int DirectDequeueSize() const noexcept
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

	inline int DirectDequeueSize(int offset) const noexcept
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

	// Front, Rear Ŀ�� �̵�
	inline int				MoveRear(int size) noexcept { m_iRear = (m_iRear + size) % m_iCapacity; return m_iRear; }
	inline int				MoveFront(int size) noexcept { m_iFront = (m_iFront + size) % m_iCapacity; return m_iFront; }

	inline char *GetPQueuePtr() const noexcept { return m_PQueue; }

	// Front ������ ���
	inline char *GetFrontPtr() const noexcept { return m_PQueue + m_iFront; }

	// Rear ������ ���
	inline char *GetRearPtr() const noexcept { return m_PQueue + m_iRear; }

private:
	int				m_iCapacity = 15000 + 1; // ����Ʈ 1�� ����Ʈ ũ��, + 1

	int				m_iFront = 0;
	int				m_iRear = 0;

	char *m_PQueue = nullptr;
};