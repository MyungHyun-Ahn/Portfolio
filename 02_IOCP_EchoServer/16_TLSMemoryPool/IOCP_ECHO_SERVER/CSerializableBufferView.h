#pragma once

// CRecvBuffer로 받은 데이터를 직렬화 버퍼처럼 볼 수 있게 함
class CSerializableBufferView
{
public:

	CSerializableBufferView() = default;
	virtual ~CSerializableBufferView() = default;


private:
	friend class CSession;

	// 오프셋 시작과 끝을 받음
	inline void Init(CSmartPtr<CRecvBuffer> spRecvBuffer, int offsetStart, int offsetEnd)
	{
		m_spRecvBuffer = spRecvBuffer;
		m_pBuffer = m_spRecvBuffer->GetOffsetPtr(offsetStart);
		m_HeaderFront = 0;
		m_Front = (int)DEFINE::HEADER_SIZE;
		// Rear는 end - start
		m_Rear = offsetEnd - offsetStart;
		m_iReadHeaderSize = 0;
	}

	// 이거 하고서는 Copy를 꼭 해줘야함
	inline void InitAndAlloc(int size)
	{
		m_pBuffer = new char[size];
		m_iBufferSize = size;
		m_HeaderFront = 0;
		m_Front = (int)DEFINE::HEADER_SIZE;
		m_Rear = 0;
		m_iReadHeaderSize = 0;
	}

	inline void Clear()
	{
		m_iReadHeaderSize = 0;
		if (m_spRecvBuffer.GetRealPtr() != nullptr)
		{
			// Ref Count 줄이고 지울 수 있다면 삭제
			m_spRecvBuffer.~CSmartPtr();
		}
		else
		{
			// 직접 할당 받은 버퍼임
			delete[] m_pBuffer;
		}
	}
	// 이걸 보고 헤더가 딜레이 된 것이라면 지연처리 작업
	inline USHORT isReadHeaderSize()
	{
		return m_iReadHeaderSize;
	}



	// 오프셋부터 쓰는 용도
	bool Copy(char *buffer, int offset, int size);
	// 뒤로 이어서 쓰는 용도
	bool Copy(char *buffer, int size);

	bool GetHeader(char *buffer, int size);

	inline int GetDataSize() const { return m_Rear - m_Front; }
	inline int GetHeaderSize() const { return  (int)DEFINE::HEADER_SIZE; }
	inline int GetFullSize() const { return GetDataSize() + GetHeaderSize(); }

	// 외부에서 버퍼를 직접 조작하기 위한 용도
	inline char *GetBufferPtr() { return m_pBuffer; }
	inline char *GetContentBufferPtr() { return m_pBuffer + m_Front; }
	inline int MoveWritePos(int size) { m_Rear += size; return m_Rear; }
	inline int MoveReadPos(int size) { m_Front += size; return m_Front; }

	// delayedHeader에 쓴다.
	bool WriteDelayedHeader(char *buffer, int size);
	bool GetDelayedHeader(char *buffer, int size);

	inline static CSerializableBufferView *Alloc()
	{
		CSerializableBufferView *pSBuffer = s_sbufferPool.Alloc();
		return pSBuffer;
	}

public:
	inline static void Free(CSerializableBufferView *delSBuffer)
	{
		// 직접 할당 받은 버퍼가 아니라면 recv 버퍼는 nullptr이 아님
		delSBuffer->Clear();

		s_sbufferPool.Free(delSBuffer);
	}

	// operator
public:
	bool Dequeue(char *buffer, int size);

	// 데이터 넣는 건 필요 없음
	inline CSerializableBufferView &operator>>(char &chData)
	{
		if (GetDataSize() < sizeof(char))
		{
			throw;
		}

		chData = *(char *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(char));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned char &byData)
	{
		if (GetDataSize() < sizeof(unsigned char))
		{
			throw;
		}

		byData = *(unsigned char *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBufferView &operator>>(short &shData)
	{
		if (GetDataSize() < sizeof(short))
		{
			throw;
		}

		shData = *(short *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(short));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned short &wData)
	{
		if (GetDataSize() < sizeof(char))
		{
			throw;
		}

		wData = *(unsigned short *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBufferView &operator>>(int &iData)
	{
		if (GetDataSize() < sizeof(int))
		{
			throw;
		}

		iData = *(int *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(int));

		return *this;
	}

	inline CSerializableBufferView &operator>>(long &lData)
	{
		if (GetDataSize() < sizeof(long))
		{
			throw;
		}

		lData = *(long *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(long));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned long &ulData)
	{
		if (GetDataSize() < sizeof(unsigned long))
		{
			throw;
		}

		ulData = *(unsigned long *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBufferView &operator>>(float &fData)
	{
		if (GetDataSize() < sizeof(float))
		{
			throw;
		}

		fData = *(float *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(float));

		return *this;
	}

	inline CSerializableBufferView &operator>>(__int64 &iData)
	{
		int size = GetDataSize();
		if (size < sizeof(__int64))
		{
			throw;
		}

		iData = *(__int64 *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned __int64 &uiData)
	{
		if (GetDataSize() < sizeof(unsigned __int64))
		{
			throw;
		}

		uiData = *(unsigned __int64 *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBufferView &operator>>(double &dData)
	{
		if (GetDataSize() < sizeof(double))
		{
			throw;
		}

		dData = *(double *)(m_pBuffer + m_Front);
		MoveReadPos(sizeof(double));

		return *this;
	}

	inline static LONG GetPoolCapacity() { return s_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() { return s_sbufferPool.GetUseCount(); }

	inline LONG IncreaseRef() { return InterlockedIncrement(&m_iRefCount); }
	inline LONG DecreaseRef() { return InterlockedDecrement(&m_iRefCount); }


private:
	char *m_pBuffer;
	int m_iBufferSize = 0;
	int m_HeaderFront = 0;
	int m_Front = 0;
	int m_Rear = 0;
	CSmartPtr<CRecvBuffer> m_spRecvBuffer;
	// 사이즈 헤더 조차도 밀림을 방지하기 위한 버퍼
	char m_delayedHeader[(int)DEFINE::HEADER_SIZE];
	USHORT m_iReadHeaderSize = 0;

	LONG			m_iRefCount = 0;

	// inline static CLFMemoryPool<CSerializableBufferView> s_sbufferPool = CLFMemoryPool<CSerializableBufferView>(5000, false);
	inline static CTLSMemoryPoolManager<CSerializableBufferView> s_sbufferPool = CTLSMemoryPoolManager<CSerializableBufferView>();
};

