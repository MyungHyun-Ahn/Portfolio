#pragma once

// CRecvBuffer�� ���� �����͸� ����ȭ ����ó�� �� �� �ְ� ��
class CSerializableBufferView
{
public:

	CSerializableBufferView() = default;
	virtual ~CSerializableBufferView() = default;


private:
	friend class CSession;

	// ������ ���۰� ���� ����
	inline void Init(CSmartPtr<CRecvBuffer> spRecvBuffer, int offsetStart, int offsetEnd)
	{
		m_spRecvBuffer = spRecvBuffer;
		m_pBuffer = m_spRecvBuffer->GetOffsetPtr(offsetStart);
		m_HeaderFront = 0;
		m_Front = (int)DEFINE::HEADER_SIZE;
		// Rear�� end - start
		m_Rear = offsetEnd - offsetStart;
		m_iReadHeaderSize = 0;
	}

	// �̰� �ϰ��� Copy�� �� �������
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
			// Ref Count ���̰� ���� �� �ִٸ� ����
			m_spRecvBuffer.~CSmartPtr();
		}
		else
		{
			// ���� �Ҵ� ���� ������
			delete[] m_pBuffer;
		}
	}
	// �̰� ���� ����� ������ �� ���̶�� ����ó�� �۾�
	inline USHORT isReadHeaderSize()
	{
		return m_iReadHeaderSize;
	}



	// �����º��� ���� �뵵
	bool Copy(char *buffer, int offset, int size);
	// �ڷ� �̾ ���� �뵵
	bool Copy(char *buffer, int size);

	bool GetHeader(char *buffer, int size);

	inline int GetDataSize() const { return m_Rear - m_Front; }
	inline int GetHeaderSize() const { return  (int)DEFINE::HEADER_SIZE; }
	inline int GetFullSize() const { return GetDataSize() + GetHeaderSize(); }

	// �ܺο��� ���۸� ���� �����ϱ� ���� �뵵
	inline char *GetBufferPtr() { return m_pBuffer; }
	inline char *GetContentBufferPtr() { return m_pBuffer + m_Front; }
	inline int MoveWritePos(int size) { m_Rear += size; return m_Rear; }
	inline int MoveReadPos(int size) { m_Front += size; return m_Front; }

	// delayedHeader�� ����.
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
		// ���� �Ҵ� ���� ���۰� �ƴ϶�� recv ���۴� nullptr�� �ƴ�
		delSBuffer->Clear();

		s_sbufferPool.Free(delSBuffer);
	}

	// operator
public:
	bool Dequeue(char *buffer, int size);

	// ������ �ִ� �� �ʿ� ����
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
	// ������ ��� ������ �и��� �����ϱ� ���� ����
	char m_delayedHeader[(int)DEFINE::HEADER_SIZE];
	USHORT m_iReadHeaderSize = 0;

	LONG			m_iRefCount = 0;

	// inline static CLFMemoryPool<CSerializableBufferView> s_sbufferPool = CLFMemoryPool<CSerializableBufferView>(5000, false);
	inline static CTLSMemoryPoolManager<CSerializableBufferView> s_sbufferPool = CTLSMemoryPoolManager<CSerializableBufferView>();
};

