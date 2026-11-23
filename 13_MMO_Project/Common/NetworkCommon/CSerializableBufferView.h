#pragma once

namespace ServerNetworkLib
{
	class CSession;
}

namespace NetworkCommonLib
{



// CRecvBuffer로 받은 데이터를 직렬화 버퍼처럼 볼 수 있게 함
class CSerializableBufferView
{
public:
	CSerializableBufferView() noexcept = default;
	virtual ~CSerializableBufferView() noexcept = default;


private:
	friend class ServerNetworkLib::CSession;

	// 오프셋 시작과 끝을 받음
	inline void init(MHLib::utils::CSmartPtr<CRecvBuffer> spRecvBuffer, int offsetStart, int offsetEnd) noexcept
	{
		m_spRecvBuffer = spRecvBuffer;
		m_pBuffer = m_spRecvBuffer->GetOffsetPtr(offsetStart);
		m_headerFront = 0;
		m_front = (int)CSerializableBuffer::EBufferDefine::HeaderSize;
		// Rear는 end - start
		m_rear = offsetEnd - offsetStart;
		m_readHeaderSize = 0;
	}

	void init(const std::shared_ptr<CRecvBuffer> &spRecvBuffer, int offsetStart, int offsetEnd) noexcept
	{
		m_spSharedRecvBuffer = spRecvBuffer;
		m_pBuffer = spRecvBuffer->GetOffsetPtr(offsetStart);
		m_headerFront = 0;
		m_front = (int)CSerializableBuffer::EBufferDefine::HeaderSize;
		m_rear = offsetEnd - offsetStart;
		m_readHeaderSize = 0;
	}

	// 이거 하고서는 Copy를 꼭 해줘야함
	inline void initAndAlloc(int size) noexcept
	{
		initStorage(new char[size], size);
	}

	[[nodiscard]] bool tryInitHeapStorage(int size) noexcept
	{
		m_upFragmentStorage.reset(new (std::nothrow) char[size]);
		if (m_upFragmentStorage == nullptr)
			return false;
		initStorage(m_upFragmentStorage.get(), size);
		return true;
	}

	void initStorage(char *pBuffer, int size) noexcept
	{
		m_pBuffer = pBuffer;
		m_bufferSize = size;
		m_headerFront = 0;
		m_front = (int)CSerializableBuffer::EBufferDefine::HeaderSize;
		m_rear = 0;
		m_readHeaderSize = 0;
	}

	inline void clear() noexcept
	{
		m_readHeaderSize = 0;
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
		m_pBuffer = nullptr;
		m_sessionId = 0;
	}
	// 이걸 보고 헤더가 딜레이 된 것이라면 지연처리 작업
	inline USHORT isReadHeaderSize() noexcept
	{
		return m_readHeaderSize;
	}



	// 오프셋부터 쓰는 용도
	bool copy(char *pBuffer, int offset, int size) noexcept;
	// 뒤로 이어서 쓰는 용도
	bool copy(char *pBuffer, int size) noexcept;

	bool getHeader(char *outBuffer, int size) noexcept;

	inline int getHeaderSize() const noexcept
	{
		return  (int)CSerializableBuffer::EBufferDefine::HeaderSize;
	}
	inline int getFullSize() const noexcept
	{
		return GetDataSize() + getHeaderSize();
	}

	// 외부에서 버퍼를 직접 조작하기 위한 용도
	inline char *getBufferPtr() const noexcept
	{
		return m_pBuffer;
	}
	inline char *getContentBufferPtr() const noexcept
	{
		return m_pBuffer + m_front;
	}
	inline int moveWritePos(int size) noexcept
	{
		m_rear += size; return m_rear;
	}
	inline int moveReadPos(int size)
	{
		m_front += size; return m_front;
	}

	// delayedHeader에 쓴다.
	bool writeDelayedHeader(char *pBuffer, int size) noexcept;
	bool getDelayedHeader(char *outBuffer, int size) noexcept;

	static CSerializableBufferView *alloc() noexcept;

public:
	inline int GetDataSize() const noexcept
	{
		return m_rear - m_front;
	}

	static void Free(CSerializableBufferView *pDelSBuffer) noexcept;

	// operator
public:
	bool Dequeue(char *outBuffer, int size) noexcept;

	// 데이터 넣는 건 필요 없음
	inline CSerializableBufferView &operator>>(char &outValue) noexcept
	{
		if (GetDataSize() < sizeof(char))
		{
			__debugbreak();
		}

		outValue = *(char *)(m_pBuffer + m_front);
		moveReadPos(sizeof(char));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned char &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned char))
		{
			__debugbreak();
		}

		outValue = *(unsigned char *)(m_pBuffer + m_front);
		moveReadPos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBufferView &operator>>(short &outValue) noexcept
	{
		if (GetDataSize() < sizeof(short))
		{
			__debugbreak();
		}

		outValue = *(short *)(m_pBuffer + m_front);
		moveReadPos(sizeof(short));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned short &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned short))
		{
			__debugbreak();
		}

		outValue = *(unsigned short *)(m_pBuffer + m_front);
		moveReadPos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBufferView &operator>>(int &outValue) noexcept
	{
		if (GetDataSize() < sizeof(int))
		{
			__debugbreak();
		}

		outValue = *(int *)(m_pBuffer + m_front);
		moveReadPos(sizeof(int));

		return *this;
	}

	inline CSerializableBufferView &operator>>(long &outValue) noexcept
	{
		if (GetDataSize() < sizeof(long))
		{
			__debugbreak();
		}

		outValue = *(long *)(m_pBuffer + m_front);
		moveReadPos(sizeof(long));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned long &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned long))
		{
			__debugbreak();
		}

		outValue = *(unsigned long *)(m_pBuffer + m_front);
		moveReadPos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBufferView &operator>>(float &outValue) noexcept
	{
		if (GetDataSize() < sizeof(float))
		{
			__debugbreak();
		}

		outValue = *(float *)(m_pBuffer + m_front);
		moveReadPos(sizeof(float));

		return *this;
	}

	inline CSerializableBufferView &operator>>(__int64 &outValue) noexcept
	{
		int size = GetDataSize();
		if (size < sizeof(__int64))
		{
			__debugbreak();
		}

		outValue = *(__int64 *)(m_pBuffer + m_front);
		moveReadPos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBufferView &operator>>(unsigned __int64 &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned __int64))
		{
			__debugbreak();
		}

		outValue = *(unsigned __int64 *)(m_pBuffer + m_front);
		moveReadPos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBufferView &operator>>(double &outValue) noexcept
	{
		if (GetDataSize() < sizeof(double))
		{
			__debugbreak();
		}

		outValue = *(double *)(m_pBuffer + m_front);
		moveReadPos(sizeof(double));

		return *this;
	}

	static LONG GetPoolCapacity() noexcept;
	static LONG GetPoolUsage() noexcept;

	inline LONG IncreaseRef() noexcept
	{
		return InterlockedIncrement(&m_refCount);
	}
	inline LONG DecreaseRef() noexcept
	{
		return InterlockedDecrement(&m_refCount);
	}


private:
	char *m_pBuffer = nullptr;
	int m_bufferSize = 0;
	int m_headerFront = 0;
	int m_front = 0;
	int m_rear = 0;

	MHLib::utils::CSmartPtr<CRecvBuffer> m_spRecvBuffer;
	std::shared_ptr<CRecvBuffer> m_spSharedRecvBuffer;
	std::unique_ptr<char[]> m_upFragmentStorage;
	// 사이즈 헤더 조차도 밀림을 방지하기 위한 버퍼
	char m_delayedHeader[(int)CSerializableBuffer::EBufferDefine::HeaderSize];
	USHORT m_readHeaderSize = 0;

	LONG			m_refCount = 0;
	UINT64			m_sessionId = 0;

	static MHLib::memory::CTlsMemoryPoolManager<CSerializableBufferView, 128, 4, false> sm_sbufferPool;
};

// offset은 거의 0만 쓸 듯?
inline bool CSerializableBufferView::copy(char *pBuffer, int offset, int size) noexcept
{
	if (m_bufferSize - m_rear < size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_pBuffer + offset, m_bufferSize - m_rear, pBuffer, size);

	// Copy 함수가 호출될 때는 무조건 HEADER는 읽은 상태임
	m_readHeaderSize = (int)CSerializableBuffer::EBufferDefine::HeaderSize;
	m_rear = offset + size;

	return true;
}

inline bool CSerializableBufferView::copy(char *pBuffer, int size) noexcept
{
	if (m_bufferSize - m_rear < size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_pBuffer + m_rear, m_bufferSize - m_rear, pBuffer, size);

	// Copy 함수가 호출될 때는 무조건 HEADER는 읽은 상태임
	m_readHeaderSize = (int)CSerializableBuffer::EBufferDefine::HeaderSize;
	m_rear += size;

	return true;
}

inline bool CSerializableBufferView::Dequeue(char *outBuffer, int size) noexcept
{
	if (GetDataSize() < size)
	{
		return false;
	}

	memcpy_s(outBuffer, size, m_pBuffer + m_front, size);
	moveReadPos(size);

	return true;
}

inline bool CSerializableBufferView::getHeader(char *outBuffer, int size) noexcept
{
	if (size < (int)CSerializableBuffer::EBufferDefine::HeaderSize)
	{
		return false;
	}

	memcpy_s(outBuffer, size, m_pBuffer, size);

	return true;
}

inline bool CSerializableBufferView::writeDelayedHeader(char *pBuffer, int size) noexcept
{
	if (m_readHeaderSize + size > (int)CSerializableBuffer::EBufferDefine::HeaderSize)
	{
		return false;
	}

	memcpy_s(m_delayedHeader + m_readHeaderSize, (int)CSerializableBuffer::EBufferDefine::HeaderSize, pBuffer, size);
	m_readHeaderSize += size;
	return true;
}

inline bool CSerializableBufferView::getDelayedHeader(char *outBuffer, int size) noexcept
{
	if (size < m_readHeaderSize)
	{
		return false;
	}

	memcpy_s(outBuffer, size, m_delayedHeader, size);

	return true;
}
}
