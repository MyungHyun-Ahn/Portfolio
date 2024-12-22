#include "pch.h"
#include "CSerializableBufferView.h"

// offset은 거의 0만 쓸 듯?
bool CSerializableBufferView::Copy(char *buffer, int offset, int size)
{
	if (m_iBufferSize - m_Rear < size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_pBuffer + offset, m_iBufferSize - m_Rear, buffer, size);

	// Copy 함수가 호출될 때는 무조건 HEADER는 읽은 상태임
	m_iReadHeaderSize = (int)DEFINE::HEADER_SIZE;
	m_Rear = offset + size;

	return true;
}

bool CSerializableBufferView::Copy(char *buffer, int size)
{
	if (m_iBufferSize - m_Rear < size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_pBuffer + m_Rear, m_iBufferSize - m_Rear, buffer, size);

	// Copy 함수가 호출될 때는 무조건 HEADER는 읽은 상태임
	m_iReadHeaderSize = (int)DEFINE::HEADER_SIZE;
	m_Rear += size;

	return true;
}

bool CSerializableBufferView::Dequeue(char *buffer, int size)
{
	if (GetDataSize() < size)
	{
		return false;
	}

	memcpy_s(buffer, size, m_pBuffer + m_Front, size);
	MoveReadPos(size);

	return true;
}

bool CSerializableBufferView::GetHeader(char *buffer, int size)
{
	if (size < (int)DEFINE::HEADER_SIZE)
	{
		return false;
	}

	memcpy_s(buffer, size, m_pBuffer, size);

	return true;
}

bool CSerializableBufferView::WriteDelayedHeader(char *buffer, int size)
{
	if (m_iReadHeaderSize + size > (int)DEFINE::HEADER_SIZE)
	{
		return false;
	}

	memcpy_s(m_delayedHeader + m_iReadHeaderSize, (int)DEFINE::HEADER_SIZE, buffer, size);
	m_iReadHeaderSize += size;
	return true;
}

bool CSerializableBufferView::GetDelayedHeader(char *buffer, int size)
{
	if (size < m_iReadHeaderSize)
	{
		return false;
	}

	memcpy_s(buffer, size, m_delayedHeader, size);

	return true;
}
