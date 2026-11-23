#include "NetworkCommonLibPch.h"

namespace NetworkCommonLib
{
	// 풀을 사용하는 서버만 이 오브젝트를 링크한다.
	template<>
	MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer<false>, 512, 2> CSerializableBuffer<false>::sm_sbufferPool{};
	template<>
	MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer<true>, 512, 2> CSerializableBuffer<true>::sm_sbufferPool{};

	template<>
	MHLib::memory::CTlsPagePoolManager<static_cast<int>(CSerializableBuffer<false>::EBufferDefine::PacketMaxSize), 16, false>
		CSerializableBuffer<false>::sm_pagePool{};
	template<>
	MHLib::memory::CTlsPagePoolManager<static_cast<int>(CSerializableBuffer<true>::EBufferDefine::PacketMaxSize), 16, false>
		CSerializableBuffer<true>::sm_pagePool{};

	namespace
	{
		MHLib::memory::CTlsMemoryPoolManager<CRecvBuffer, 16, 4> g_recvBufferPool;
		MHLib::memory::CTlsPagePoolManager<4096, 2> g_recvPagePool;
	}

	CRecvBuffer::CRecvBuffer() noexcept
		: CRecvBuffer(static_cast<char *>(g_recvPagePool.Alloc()), freePooled, freePooledPage)
	{
	}

	CRecvBuffer *CRecvBuffer::Alloc() noexcept
	{
		auto *pBuffer = g_recvBufferPool.Alloc();
		pBuffer->Clear();
		return pBuffer;
	}

	void CRecvBuffer::freePooled(CRecvBuffer *pBuffer) noexcept
	{
		g_recvBufferPool.Free(pBuffer);
	}

	void CRecvBuffer::freePooledPage(char *pPage) noexcept
	{
		g_recvPagePool.Free(pPage);
	}

	LONG CRecvBuffer::GetPoolCapacity() noexcept
	{
		return g_recvBufferPool.GetCapacity();
	}

	LONG CRecvBuffer::GetPoolUsage() noexcept
	{
		return g_recvBufferPool.GetUseCount();
	}

	template<>
	MHLib::memory::CTlsMemoryPoolManager<CSerializableBufferView<false>, 128, 4, false> CSerializableBufferView<false>::sm_sbufferPool{};
	template<>
	MHLib::memory::CTlsMemoryPoolManager<CSerializableBufferView<true>, 128, 4, false> CSerializableBufferView<true>::sm_sbufferPool{};
}