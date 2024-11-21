#pragma once

enum class DEFINE
{
	HEADER_SIZE = sizeof(USHORT),
	DEFAULT_SIZE = 1400
};

class CSerializableBuffer
{
public:

	CSerializableBuffer();
	CSerializableBuffer(int size);

	virtual ~CSerializableBuffer();


	inline void Clear()
	{
		m_HeaderFront = 0;
		m_Front = (int)DEFINE::HEADER_SIZE;
		m_Rear = (int)DEFINE::HEADER_SIZE;
	}

	bool EnqueueHeader(char *buffer, int size);
	bool Enqueue(char *buffer, int size);
	bool Dequeue(char *buffer, int size);

	inline int GetBufferSize() const { return m_MaxSize; }
	inline int GetDataSize() const { return m_Rear - m_Front; }
	inline int GetHeaderSize() const { return  (int)DEFINE::HEADER_SIZE; }
	inline int GetFullSize() const { return GetDataSize() + GetHeaderSize(); }

	// 외부에서 버퍼를 직접 조작하기 위한 용도
	inline char *GetBufferPtr() { return m_Buffer; }
	inline char *GetContentBufferPtr() { return m_Buffer + m_Front; }
	inline int MoveWritePos(int size) { m_Rear += size; return m_Rear; }
	inline int MoveReadPos(int size) { m_Front += size; return m_Front; }

public:
	inline CSerializableBuffer &operator<<(char chData)
	{
		if (m_MaxSize - m_Rear > sizeof(char))
		{
			// TODO: resize
		}

		char *ptr = (char *)(m_Buffer + m_Rear);
		*ptr = chData;

		MoveWritePos(sizeof(char));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned char byData)
	{
		if (m_MaxSize - m_Rear > sizeof(unsigned char))
		{
			// TODO: resize
		}

		unsigned char *ptr = (unsigned char *)(m_Buffer + m_Rear);
		*ptr = byData;

		MoveWritePos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBuffer &operator<<(short shData)
	{
		if (m_MaxSize - m_Rear > sizeof(short))
		{
			// TODO: resize
		}

		short *ptr = (short *)(m_Buffer + m_Rear);
		*ptr = shData;

		MoveWritePos(sizeof(short));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned short wData)
	{
		if (m_MaxSize - m_Rear > sizeof(unsigned short))
		{
			// TODO: resize
		}

		unsigned short *ptr = (unsigned short *)(m_Buffer + m_Rear);
		*ptr = wData;

		MoveWritePos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBuffer &operator<<(int iData)
	{
		if (m_MaxSize - m_Rear > sizeof(int))
		{
			// TODO: resize
		}

		int *ptr = (int *)(m_Buffer + m_Rear);
		*ptr = iData;

		MoveWritePos(sizeof(int));

		return *this;
	}

	inline CSerializableBuffer &operator<<(long lData)
	{
		if (m_MaxSize - m_Rear > sizeof(long))
		{
			// TODO: resize
		}

		long *ptr = (long *)(m_Buffer + m_Rear);
		*ptr = lData;

		MoveWritePos(sizeof(long));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned long lData)
	{
		if (m_MaxSize - m_Rear > sizeof(unsigned long))
		{
			// TODO: resize
		}

		unsigned long *ptr = (unsigned long *)(m_Buffer + m_Rear);
		*ptr = lData;

		MoveWritePos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBuffer &operator<<(float fData)
	{
		if (m_MaxSize - m_Rear > sizeof(float))
		{
			// TODO: resize
		}

		float *ptr = (float *)(m_Buffer + m_Rear);
		*ptr = fData;

		MoveWritePos(sizeof(float));

		return *this;
	}

	inline CSerializableBuffer &operator<<(__int64 iData)
	{
		if (m_MaxSize - m_Rear > sizeof(__int64))
		{
			// TODO: resize
		}

		__int64 *ptr = (__int64 *)(m_Buffer + m_Rear);
		*ptr = iData;

		MoveWritePos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned __int64 uiData)
	{
		if (m_MaxSize - m_Rear > sizeof(unsigned __int64))
		{
			// TODO: resize
		}

		unsigned __int64 *ptr = (unsigned __int64 *)(m_Buffer + m_Rear);
		*ptr = uiData;

		MoveWritePos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBuffer &operator<<(double dData)
	{
		if (m_MaxSize - m_Rear > sizeof(double))
		{
			// TODO: resize
		}

		*(double *)(m_Buffer + m_Rear) = dData;

		MoveWritePos(sizeof(double));

		return *this;
	}

	inline CSerializableBuffer &operator>>(char &chData)
	{
		if (GetDataSize() < sizeof(char))
		{
			throw;
		}

		chData = *(char *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(char));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned char &byData)
	{
		if (GetDataSize() < sizeof(unsigned char))
		{
			throw;
		}

		byData = *(unsigned char *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBuffer &operator>>(short &shData)
	{
		if (GetDataSize() < sizeof(short))
		{
			throw;
		}

		shData = *(short *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(short));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned short &wData)
	{
		if (GetDataSize() < sizeof(char))
		{
			throw;
		}

		wData = *(unsigned short *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBuffer &operator>>(int &iData)
	{
		if (GetDataSize() < sizeof(int))
		{
			throw;
		}

		iData = *(int *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(int));

		return *this;
	}

	inline CSerializableBuffer &operator>>(long &lData)
	{
		if (GetDataSize() < sizeof(long))
		{
			throw;
		}

		lData = *(long *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(long));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned long &ulData)
	{
		if (GetDataSize() < sizeof(unsigned long))
		{
			throw;
		}

		ulData = *(unsigned long *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBuffer &operator>>(float &fData)
	{
		if (GetDataSize() < sizeof(float))
		{
			throw;
		}

		fData = *(float *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(float));

		return *this;
	}

	inline CSerializableBuffer &operator>>(__int64 &iData)
	{
		if (GetDataSize() < sizeof(__int64))
		{
			throw;
		}

		iData = *(__int64 *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned __int64 &uiData)
	{
		if (GetDataSize() < sizeof(unsigned __int64))
		{
			throw;
		}

		uiData = *(unsigned __int64 *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBuffer &operator>>(double &dData)
	{
		if (GetDataSize() < sizeof(double))
		{
			throw;
		}

		dData = *(double *)(m_Buffer + m_Front);
		MoveReadPos(sizeof(double));

		return *this;
	}

public:
	inline static CSerializableBuffer *Alloc()
	{
		// 할당하고 초기화해서 반환
		CSerializableBuffer *pSBuffer = s_sbufferPool.Alloc();
		pSBuffer->Clear();
		return pSBuffer;
	}

	inline static void Free(CSerializableBuffer *delSBuffer)
	{
		s_sbufferPool.Free(delSBuffer);
	}

	inline static LONG GetPoolCapacity() { return s_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sbufferPool.GetUseCount(); }

private:
	char *m_Buffer;
	int m_HeaderFront = 0;
	int m_Front = 0;
	int m_Rear = 0;
	int m_MaxSize = (int)DEFINE::DEFAULT_SIZE;

	inline static CLFMemoryPool<CSerializableBuffer> s_sbufferPool = CLFMemoryPool<CSerializableBuffer>(5000, false);
};