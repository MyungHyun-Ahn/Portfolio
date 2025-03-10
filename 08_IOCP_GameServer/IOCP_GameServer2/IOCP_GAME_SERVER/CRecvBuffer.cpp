#include "pch.h"
#include "MyInclude.h"
#include "CRecvBuffer.h"

int CRecvBuffer::Peek(char *buffer, int size) noexcept
{
	if (GetUseSize() < size) {
		return -1;
	}

	memcpy_s(buffer, size, m_PQueue + m_iFront, size);
	return size;
}

int CRecvBuffer::Peek(char *buffer, int size, int offset) noexcept
{
	if (GetUseSize(offset) < size) {
		return -1;
	}
	
	int front = m_iFront + offset;

	memcpy_s(buffer, size, m_PQueue + front, size);

	return 0;
}
