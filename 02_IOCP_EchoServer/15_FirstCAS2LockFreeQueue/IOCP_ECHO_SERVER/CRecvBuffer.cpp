#include "pch.h"
#include "CRecvBuffer.h"

int CRecvBuffer::Peek(char *buffer, int size)
{
	if (GetUseSize() < size) {
		return -1;
	}

	memcpy_s(buffer, size, m_PQueue + m_iFront, size);
	return size;
}

int CRecvBuffer::Peek(char *buffer, int size, int offset)
{
	if (GetUseSize(offset) < size) {
		return -1;
	}
	
	int front = m_iFront + offset;

	memcpy_s(buffer, size, m_PQueue + front, size);

	return 0;
}
