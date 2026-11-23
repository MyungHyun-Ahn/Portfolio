#include "NetworkCommonLibPch.h"

namespace NetworkCommonLib
{
	// 풀을 사용하는 서버만 이 오브젝트를 링크한다.
	MHLib::memory::CTlsMemoryPoolManager<CSerializableBuffer, 512, 2> CSerializableBuffer::sm_sbufferPool{};

	MHLib::memory::CTlsPagePoolManager<static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize), 16, false>
		CSerializableBuffer::sm_pagePool{};

	CSerializableBuffer::CSerializableBuffer() noexcept
		: CSerializableBuffer(static_cast<char *>(sm_pagePool.Alloc()), freePooledPage, freePooledBuffer)
	{
	}

	CSerializableBuffer *CSerializableBuffer::Alloc() noexcept
	{
		// 할당하고 초기화해서 반환
		CSerializableBuffer *pSBuffer = sm_sbufferPool.Alloc();
		pSBuffer->Clear();
		return pSBuffer;
	}

	LONG CSerializableBuffer::GetPoolCapacity() noexcept
	{
		return sm_sbufferPool.GetCapacity();
	}

	LONG CSerializableBuffer::GetPoolUsage() noexcept
	{
		return sm_sbufferPool.GetUseCount();
	}

	void CSerializableBuffer::freePooledPage(char *pPage) noexcept
	{
		sm_pagePool.Free(pPage);
	}

	void CSerializableBuffer::freePooledBuffer(CSerializableBuffer *pBuffer) noexcept
	{
		sm_sbufferPool.Free(pBuffer);
	}

	CSerializableBufferView *CSerializableBufferView::alloc() noexcept
	{
		CSerializableBufferView *pSBuffer = sm_sbufferPool.Alloc();
		return pSBuffer;
	}

	void CSerializableBufferView::Free(CSerializableBufferView *pDelSBuffer) noexcept
	{
		pDelSBuffer->clear();
		sm_sbufferPool.Free(pDelSBuffer);
	}

	LONG CSerializableBufferView::GetPoolCapacity() noexcept
	{
		return sm_sbufferPool.GetCapacity();
	}

	LONG CSerializableBufferView::GetPoolUsage() noexcept
	{
		return sm_sbufferPool.GetUseCount();
	}

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

	MHLib::memory::CTlsMemoryPoolManager<CSerializableBufferView, 128, 4, false> CSerializableBufferView::sm_sbufferPool{};
}