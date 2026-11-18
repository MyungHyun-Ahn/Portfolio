#pragma once

namespace NetworkLib
{
template<bool isLanServer>
class CSerializableBuffer
{
public:
	enum class EBufferDefine : int
	{
		HeaderSize = isLanServer ? 2 : 5,
		PacketMaxSize = isLanServer ? 128 : 1024 // 채팅 로그인과 최대 메시지 응답
	};


	CSerializableBuffer() noexcept
	{
		m_pBuffer = (char *)sm_pagePool.Alloc();
		m_maxSize = (int)EBufferDefine::PacketMaxSize;
		m_headerFront = 0;
		m_front = (int)EBufferDefine::HeaderSize;
		m_rear = (int)EBufferDefine::HeaderSize;
	}

	virtual ~CSerializableBuffer() noexcept
	{
		sm_pagePool.Free(m_pBuffer);
	}


	inline void Clear() noexcept
	{
		m_headerFront = 0;
		m_front = (int)EBufferDefine::HeaderSize;
		m_rear = (int)EBufferDefine::HeaderSize;
		m_isEnqueueHeader = FALSE;
	}

	bool EnqueueHeader(char *pBuffer, int size) noexcept;
	bool Enqueue(char *pBuffer, int size) noexcept;
	bool Dequeue(char *outBuffer, int size) noexcept;

	inline int GetBufferSize() const noexcept { return m_maxSize; }
	inline int GetDataSize() const noexcept { return m_rear - m_front; }
	inline int GetHeaderSize() const noexcept { return  (int)EBufferDefine::HeaderSize; }
	inline int GetFullSize() const noexcept { return GetDataSize() + GetHeaderSize(); }
	inline int GetIsEnqueueHeader() const noexcept { return m_isEnqueueHeader; }

	// 외부에서 버퍼를 직접 조작하기 위한 용도
	inline char *GetBufferPtr() const noexcept { return m_pBuffer; }
	inline char *GetContentBufferPtr() const noexcept { return m_pBuffer + m_front; }
	inline int MoveWritePos(int size) noexcept { m_rear += size; return m_rear; }
	inline int MoveReadPos(int size) noexcept { m_front += size; return m_front; }

public:
	inline CSerializableBuffer &operator<<(char chData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(char))
		{
			// TODO: resize
		}

		char *pPtr = (char *)(m_pBuffer + m_rear);
		*pPtr = chData;

		MoveWritePos(sizeof(char));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned char byData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(unsigned char))
		{
			// TODO: resize
		}

		unsigned char *pPtr = (unsigned char *)(m_pBuffer + m_rear);
		*pPtr = byData;

		MoveWritePos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBuffer &operator<<(short shData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(short))
		{
			// TODO: resize
		}

		short *pPtr = (short *)(m_pBuffer + m_rear);
		*pPtr = shData;

		MoveWritePos(sizeof(short));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned short wData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(unsigned short))
		{
			// TODO: resize
		}

		unsigned short *pPtr = (unsigned short *)(m_pBuffer + m_rear);
		*pPtr = wData;

		MoveWritePos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBuffer &operator<<(int iData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(int))
		{
			// TODO: resize
		}

		int *pPtr = (int *)(m_pBuffer + m_rear);
		*pPtr = iData;

		MoveWritePos(sizeof(int));

		return *this;
	}

	inline CSerializableBuffer &operator<<(long lData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(long))
		{
			// TODO: resize
		}

		long *pPtr = (long *)(m_pBuffer + m_rear);
		*pPtr = lData;

		MoveWritePos(sizeof(long));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned long lData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(unsigned long))
		{
			// TODO: resize
		}

		unsigned long *pPtr = (unsigned long *)(m_pBuffer + m_rear);
		*pPtr = lData;

		MoveWritePos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBuffer &operator<<(float fData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(float))
		{
			// TODO: resize
		}

		float *pPtr = (float *)(m_pBuffer + m_rear);
		*pPtr = fData;

		MoveWritePos(sizeof(float));

		return *this;
	}

	inline CSerializableBuffer &operator<<(__int64 iData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(__int64))
		{
			// TODO: resize
		}

		__int64 *pPtr = (__int64 *)(m_pBuffer + m_rear);
		*pPtr = iData;

		MoveWritePos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBuffer &operator<<(unsigned __int64 uiData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(unsigned __int64))
		{
			// TODO: resize
		}

		unsigned __int64 *pPtr = (unsigned __int64 *)(m_pBuffer + m_rear);
		*pPtr = uiData;

		MoveWritePos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBuffer &operator<<(double dData) noexcept
	{
		if (m_maxSize - m_rear > sizeof(double))
		{
			// TODO: resize
		}

		*(double *)(m_pBuffer + m_rear) = dData;

		MoveWritePos(sizeof(double));

		return *this;
	}

	inline CSerializableBuffer &operator>>(char &outValue) noexcept
	{
		if (GetDataSize() < sizeof(char))
		{
			__debugbreak();
		}

		outValue = *(char *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(char));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned char &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned char))
		{
			__debugbreak();
		}

		outValue = *(unsigned char *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(unsigned char));

		return *this;
	}

	inline CSerializableBuffer &operator>>(short &outValue) noexcept
	{
		if (GetDataSize() < sizeof(short))
		{
			__debugbreak();
		}

		outValue = *(short *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(short));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned short &outValue) noexcept
	{
		if (GetDataSize() < sizeof(char))
		{
			__debugbreak();
		}

		outValue = *(unsigned short *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(unsigned short));

		return *this;
	}

	inline CSerializableBuffer &operator>>(int &outValue) noexcept
	{
		if (GetDataSize() < sizeof(int))
		{
			__debugbreak();
		}

		outValue = *(int *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(int));

		return *this;
	}

	inline CSerializableBuffer &operator>>(long &outValue) noexcept
	{
		if (GetDataSize() < sizeof(long))
		{
			__debugbreak();
		}

		outValue = *(long *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(long));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned long &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned long))
		{
			__debugbreak();
		}

		outValue = *(unsigned long *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(unsigned long));

		return *this;
	}

	inline CSerializableBuffer &operator>>(float &outValue) noexcept
	{
		if (GetDataSize() < sizeof(float))
		{
			__debugbreak();
		}

		outValue = *(float *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(float));

		return *this;
	}

	inline CSerializableBuffer &operator>>(__int64 &outValue) noexcept
	{
		if (GetDataSize() < sizeof(__int64))
		{
			__debugbreak();
		}

		outValue = *(__int64 *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(__int64));

		return *this;
	}

	inline CSerializableBuffer &operator>>(unsigned __int64 &outValue) noexcept
	{
		if (GetDataSize() < sizeof(unsigned __int64))
		{
			__debugbreak();
		}

		outValue = *(unsigned __int64 *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(unsigned __int64));

		return *this;
	}

	inline CSerializableBuffer &operator>>(double &outValue) noexcept
	{
		if (GetDataSize() < sizeof(double))
		{
			__debugbreak();
		}

		outValue = *(double *)(m_pBuffer + m_front);
		MoveReadPos(sizeof(double));

		return *this;
	}

public:
	inline static CSerializableBuffer *Alloc() noexcept
	{
		// 할당하고 초기화해서 반환
		CSerializableBuffer *pSBuffer = sm_sbufferPool.Alloc();
		pSBuffer->Clear();
		return pSBuffer;
	}

	inline static void Free(CSerializableBuffer *pDelSBuffer) noexcept
	{
		sm_sbufferPool.Free(pDelSBuffer);
	}

	inline static LONG GetPoolCapacity() noexcept { return sm_sbufferPool.GetCapacity(); }
	inline static LONG GetPoolUsage() noexcept { return sm_sbufferPool.GetUseCount(); }

	inline LONG IncreaseRef() noexcept 
	{
		return InterlockedIncrement(&m_refCount);
	}
	inline LONG DecreaseRef() noexcept 
	{ 
		LONG back = InterlockedDecrement(&m_refCount);
		if (back == -1)
			__debugbreak();

		return back;
	}

	inline void SetSessionId(UINT64 id) noexcept { m_sessionId = id; }
	inline UINT64 GetSessionId() noexcept { return m_sessionId; }

private:
	char *m_pBuffer;
	int m_headerFront = 0;
	int m_front = 0;
	int m_rear = 0;
	int m_maxSize = (int)EBufferDefine::PacketMaxSize;

	LONG			m_refCount = 0;


public:
	BOOL			m_isEnqueueHeader = 0;
	UINT64			m_sessionId = 0;

	inline static MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer, 512, 2> sm_sbufferPool =
		MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer, 512, 2>();
	inline static MHLib::memory::CTlsPagePoolManager<(int)EBufferDefine::PacketMaxSize, 16, false> sm_pagePool =
		MHLib::memory::CTlsPagePoolManager<(int)EBufferDefine::PacketMaxSize, 16, false>();
};

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::EnqueueHeader(char *pBuffer, int size) noexcept
{
	m_isEnqueueHeader = TRUE;

	// 이 상황은 이미 헤더를 삽입한 것
	//	* CGameServer::SendSector에서 발생 가능
	//  * 싱글 스레드에서는 문제가 발생할 여지가 없으므로 그냥 return true
	if (m_headerFront + size > m_front)
		return true;

	memcpy_s(m_pBuffer + m_headerFront, m_front - m_headerFront, pBuffer, size);

	m_headerFront += size;

	return true;
}

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::Enqueue(char *pBuffer, int size) noexcept
{
	if (m_maxSize - m_rear < size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_pBuffer + m_rear, m_maxSize - m_rear, pBuffer, size);
	MoveWritePos(size);

	return true;
}

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::Dequeue(char *outBuffer, int size) noexcept
{
	if (GetDataSize() < size)
	{
		return false;
	}

	memcpy_s(outBuffer, size, m_pBuffer + m_front, size);
	MoveReadPos(size);

	return true;
}
}
