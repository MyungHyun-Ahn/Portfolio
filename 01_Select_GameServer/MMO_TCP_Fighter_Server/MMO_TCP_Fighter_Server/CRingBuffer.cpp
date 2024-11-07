#include "pch.h"
#include "CRingBuffer.h"

// 디폴트 생성자 10000 바이트 크기 생성
CRingBuffer::CRingBuffer()
{
	m_PQueue = new char[m_iCapacity];
}

CRingBuffer::CRingBuffer(int size) : m_iCapacity(size + 1)
{
	m_PQueue = new char[m_iCapacity];
}

CRingBuffer::~CRingBuffer()
{
	delete m_PQueue;
}

int CRingBuffer::Enqueue(char *data, int size)
{
	// 공간 부족
	int freeSize = GetFreeSize();
	if (freeSize < size)
		return -1;

	int rear = m_iRear;
	int des = DirectEnqueueSize();
	int firstPartSize = 0;
	if (des < 0)
		firstPartSize = size;
	else
		firstPartSize = min(size, des);

	int secondPartSize = size - firstPartSize;
	memcpy_s(m_PQueue + rear, freeSize, data, firstPartSize);
	memcpy_s(m_PQueue, secondPartSize, data + firstPartSize, secondPartSize);
	MoveRear(size);

	return size;
}

int CRingBuffer::Dequeue(char *buffer, int size)
{
	int ret = Peek(buffer, size);
	if (ret == -1)
		return -1;

	MoveFront(ret);
	return ret;
}

int CRingBuffer::Peek(char *buffer, int size)
{
	if (GetUseSize() < size) {
		return -1;
	}

	int front = m_iFront;
	int dds = DirectDequeueSize();
	int firstPartSize = 0;

	if (dds < 0)
		firstPartSize = size;
	else
		firstPartSize = min(size, dds);
	int secondPartSize = size - firstPartSize;

	memcpy_s(buffer, size, m_PQueue + front, firstPartSize);
	memcpy_s(buffer + firstPartSize, size - firstPartSize, m_PQueue, secondPartSize);

	return size;
}

int CRingBuffer::Peek(char *buffer, int size, int offset)
{
	if (GetUseSize(offset) < size) {
		return -1;
	}

	int front = m_iFront;
	front = (front + offset) % m_iCapacity;
	int dds = DirectDequeueSize(offset);
	int firstPartSize = 0;

	if (dds < 0)
		firstPartSize = size;
	else
		firstPartSize = min(size, dds);

	int secondPartSize = size - firstPartSize;

	memcpy_s(buffer, size, m_PQueue + front, firstPartSize);
	memcpy_s(buffer + firstPartSize, size - firstPartSize, m_PQueue, secondPartSize);

	return 0;
}