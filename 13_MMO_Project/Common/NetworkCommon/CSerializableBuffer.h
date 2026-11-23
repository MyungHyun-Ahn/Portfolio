#pragma once

namespace MHLib::serialization
{
	template<bool isLanServer> class ISerializable;
}

namespace NetworkCommonLib
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
		: CSerializableBuffer(static_cast<char *>(sm_pagePool.Alloc()), freePooledPage, freePooledBuffer)
	{
	}

	virtual ~CSerializableBuffer() noexcept
	{
		m_releasePage(m_pBuffer);
	}

	[[nodiscard]] static std::shared_ptr<CSerializableBuffer> MakeShared() noexcept
	{
		try
		{
			auto upPage = std::make_unique_for_overwrite<char[]>(static_cast<int>(EBufferDefine::PacketMaxSize));
			return std::shared_ptr<CSerializableBuffer>(new CSerializableBuffer(upPage.release(), freeHeapPage, freeHeapBuffer));
		}
		catch (const std::bad_alloc &)
		{
			return {};
		}
	}

	[[nodiscard]] bool IsHeapAllocated() const noexcept
	{
		return m_releaseBuffer == freeHeapBuffer;
	}


	inline void Clear() noexcept
	{
		m_headerFront = 0;
		m_front = (int)EBufferDefine::HeaderSize;
		m_rear = (int)EBufferDefine::HeaderSize;
		m_isEnqueueHeader = FALSE;
		m_isValid = true;
	}

	bool EnqueueHeader(char *pBuffer, int size) noexcept;
	bool Enqueue(char *pBuffer, int size) noexcept;
	bool Dequeue(char *outBuffer, int size) noexcept;
	// 실패 후 직렬화 연산은 Clear() 호출 전까지 버퍼를 변경하지 않는다.
	[[nodiscard]] bool IsValid() const noexcept
	{
		return m_isValid;
	}

	inline int GetBufferSize() const noexcept
	{
		return m_maxSize;
	}
	inline int GetDataSize() const noexcept
	{
		return m_rear - m_front;
	}
	inline int GetHeaderSize() const noexcept
	{
		return  (int)EBufferDefine::HeaderSize;
	}
	inline int GetFullSize() const noexcept
	{
		return GetDataSize() + GetHeaderSize();
	}
	inline int GetIsEnqueueHeader() const noexcept
	{
		return m_isEnqueueHeader;
	}

	// 외부에서 버퍼를 직접 조작하기 위한 용도
	inline char *GetBufferPtr() const noexcept
	{
		return m_pBuffer;
	}
	inline char *GetContentBufferPtr() const noexcept
	{
		return m_pBuffer + m_front;
	}
	int MoveWritePos(int sizeBytes) noexcept
	{
		if (sizeBytes < 0 || !canWrite(sizeBytes))
			fail();
		else
			m_rear += sizeBytes;
		return m_rear;
	}

	int MoveReadPos(int sizeBytes) noexcept
	{
		if (sizeBytes < 0 || !canRead(sizeBytes))
			fail();
		else
			m_front += sizeBytes;
		return m_front;
	}

private:
	static_assert(std::endian::native == std::endian::little);
	static_assert(sizeof(wchar_t) == 2);
	inline static constexpr std::size_t sm_utf16PrefixBytes = sizeof(std::uint32_t);

	template<typename TValue>
	inline static constexpr bool sm_isScalar = std::is_same_v<TValue, char> || std::is_same_v<TValue, signed char>
		|| std::is_same_v<TValue, unsigned char> || std::is_same_v<TValue, short> || std::is_same_v<TValue, unsigned short>
		|| std::is_same_v<TValue, int> || std::is_same_v<TValue, unsigned int> || std::is_same_v<TValue, long>
		|| std::is_same_v<TValue, unsigned long> || std::is_same_v<TValue, long long> || std::is_same_v<TValue, unsigned long long>
		|| std::is_same_v<TValue, float> || std::is_same_v<TValue, double>;

public:
	template<typename TValue> requires sm_isScalar<TValue>
	CSerializableBuffer &operator<<(TValue value) noexcept
	{
		writeBytes(&value, sizeof(value));
		return *this;
	}

	template<typename TValue> requires sm_isScalar<TValue>
	CSerializableBuffer &operator>>(TValue &outValue) noexcept
	{
		readBytes(&outValue, sizeof(outValue));
		return *this;
	}

	template<typename TValue> requires std::derived_from<TValue, MHLib::serialization::ISerializable<isLanServer>>
	CSerializableBuffer &operator<<(const TValue &value) noexcept
	{
		const MHLib::serialization::ISerializable<isLanServer> &serializable = value;
		if (m_isValid && !serializable.Serialize(*this))
			fail();
		return *this;
	}

	template<typename TValue> requires std::derived_from<TValue, MHLib::serialization::ISerializable<isLanServer>>
	CSerializableBuffer &operator>>(TValue &outValue) noexcept
	{
		MHLib::serialization::ISerializable<isLanServer> &serializable = outValue;
		if (m_isValid && !serializable.Deserialize(*this))
			fail();
		return *this;
	}

	template<typename TChar, std::size_t kCount> requires (std::is_same_v<TChar, char> || std::is_same_v<TChar, wchar_t>)
	CSerializableBuffer &operator<<(const std::array<TChar, kCount> &value) noexcept
	{
		writeBytes(value.data(), kCount * sizeof(TChar));
		return *this;
	}

	template<typename TChar, std::size_t kCount> requires (std::is_same_v<TChar, char> || std::is_same_v<TChar, wchar_t>)
	CSerializableBuffer &operator>>(std::array<TChar, kCount> &outValue) noexcept
	{
		readBytes(outValue.data(), kCount * sizeof(TChar));
		return *this;
	}

	CSerializableBuffer &operator<<(const std::wstring &value) noexcept
	{
		const auto remainingBytes = static_cast<std::size_t>(m_maxSize - m_rear);
		if (!canWrite(sm_utf16PrefixBytes) || value.size() > (remainingBytes - sm_utf16PrefixBytes) / sizeof(wchar_t))
		{
			fail();
			return *this;
		}

		const auto codeUnitCount = static_cast<std::uint32_t>(value.size());
		writeBytes(&codeUnitCount, sm_utf16PrefixBytes);
		writeBytes(value.data(), value.size() * sizeof(wchar_t));
		return *this;
	}

	CSerializableBuffer &operator>>(std::wstring &outValue) noexcept
	{
		if (!canRead(sm_utf16PrefixBytes))
		{
			fail();
			return *this;
		}

		std::uint32_t codeUnitCount = 0;
		std::memcpy(&codeUnitCount, m_pBuffer + m_front, sm_utf16PrefixBytes);
		const auto remainingBytes = static_cast<std::size_t>(GetDataSize()) - sm_utf16PrefixBytes;
		if (codeUnitCount > remainingBytes / sizeof(wchar_t))
		{
			fail();
			return *this;
		}

		const char *pText = m_pBuffer + m_front + sm_utf16PrefixBytes;

		const std::size_t textBytes = codeUnitCount * sizeof(wchar_t);
		try
		{
			std::wstring value(codeUnitCount, L'\0');
			std::memcpy(value.data(), pText, textBytes);
			outValue.swap(value);
			m_front += static_cast<int>(sm_utf16PrefixBytes + textBytes);
		}
		catch (const std::bad_alloc &)
		{
			fail();
		}
		return *this;
	}

	inline static CSerializableBuffer *Alloc() noexcept
	{
		// 할당하고 초기화해서 반환
		CSerializableBuffer *pSBuffer = sm_sbufferPool.Alloc();
		pSBuffer->Clear();
		return pSBuffer;
	}

	inline static void Free(CSerializableBuffer *pDelSBuffer) noexcept
	{
		pDelSBuffer->m_releaseBuffer(pDelSBuffer);
	}

	inline static LONG GetPoolCapacity() noexcept
	{
		return sm_sbufferPool.GetCapacity();
	}
	inline static LONG GetPoolUsage() noexcept
	{
		return sm_sbufferPool.GetUseCount();
	}

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

	inline void SetSessionId(UINT64 id) noexcept
	{
		m_sessionId = id;
	}
	inline UINT64 GetSessionId() noexcept
	{
		return m_sessionId;
	}

private:
	using PageRelease = void (*)(char *) noexcept;
	using BufferRelease = void (*)(CSerializableBuffer *) noexcept;

	CSerializableBuffer(char *pPage, PageRelease releasePage, BufferRelease releaseBuffer) noexcept
		: m_pBuffer(pPage)
		, m_releasePage(releasePage)
		, m_releaseBuffer(releaseBuffer)
		, m_front(static_cast<int>(EBufferDefine::HeaderSize))
		, m_rear(static_cast<int>(EBufferDefine::HeaderSize))
	{
	}

	static void freePooledPage(char *pPage) noexcept
	{
		sm_pagePool.Free(pPage);
	}

	static void freePooledBuffer(CSerializableBuffer *pBuffer) noexcept
	{
		sm_sbufferPool.Free(pBuffer);
	}

	static void freeHeapPage(char *pPage) noexcept
	{
		delete[] pPage;
	}

	static void freeHeapBuffer(CSerializableBuffer *pBuffer) noexcept
	{
		delete pBuffer;
	}

	bool fail() noexcept
	{
		m_isValid = false;
		return false;
	}

	bool canWrite(std::size_t sizeBytes) const noexcept
	{
		return m_isValid && sizeBytes <= static_cast<std::size_t>(m_maxSize - m_rear);
	}

	bool canRead(std::size_t sizeBytes) const noexcept
	{
		return m_isValid && sizeBytes <= static_cast<std::size_t>(GetDataSize());
	}

	bool writeBytes(const void *pData, std::size_t sizeBytes) noexcept
	{
		if (!canWrite(sizeBytes) || (sizeBytes != 0 && pData == nullptr))
			return fail();
		if (sizeBytes != 0)
			std::memmove(m_pBuffer + m_rear, pData, sizeBytes);
		m_rear += static_cast<int>(sizeBytes);
		return true;
	}

	bool readBytes(void *outData, std::size_t sizeBytes) noexcept
	{
		if (!canRead(sizeBytes) || (sizeBytes != 0 && outData == nullptr))
			return fail();
		if (sizeBytes != 0)
			std::memmove(outData, m_pBuffer + m_front, sizeBytes);
		m_front += static_cast<int>(sizeBytes);
		return true;
	}

	bool m_isValid = true;
	char *m_pBuffer;
	// 마지막 참조를 놓는 스레드와 관계없이 원래 할당 경로로 반환한다.
	PageRelease m_releasePage;
	BufferRelease m_releaseBuffer;
	int m_headerFront = 0;
	int m_front = 0;
	int m_rear = 0;
	int m_maxSize = (int)EBufferDefine::PacketMaxSize;

	LONG			m_refCount = 0;


public:
	BOOL			m_isEnqueueHeader = 0;
	UINT64			m_sessionId = 0;

	static MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer, 512, 2> sm_sbufferPool;
	static MHLib::memory::CTlsPagePoolManager<(int)EBufferDefine::PacketMaxSize, 16, false> sm_pagePool;
};

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::EnqueueHeader(char *pBuffer, int size) noexcept
{
	if (!m_isValid)
		return false;
	if (size < 0 || size > GetHeaderSize() || (size != 0 && pBuffer == nullptr))
		return fail();
	if (size > GetHeaderSize() - m_headerFront)
		return true; // 이미 준비한 송신 헤더를 덮어쓰지 않는다.

	if (size != 0)
		std::memmove(m_pBuffer + m_headerFront, pBuffer, size);
	m_isEnqueueHeader = TRUE;
	m_headerFront += size;

	return true;
}

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::Enqueue(char *pBuffer, int size) noexcept
{
	return size >= 0 ? writeBytes(pBuffer, size) : fail();
}

template<bool isLanServer>
bool CSerializableBuffer<isLanServer>::Dequeue(char *outBuffer, int size) noexcept
{
	return size >= 0 ? readBytes(outBuffer, size) : fail();
}
}
