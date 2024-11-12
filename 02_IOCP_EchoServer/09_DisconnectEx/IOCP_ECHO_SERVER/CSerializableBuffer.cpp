#include "pch.h"
#include "CSerializableBuffer.h"

CSerializableBuffer::CSerializableBuffer()
{
	m_Buffer = new char[m_MaxSize];
	m_HeaderFront = 0;
	m_Front = (int)DEFINE::HEADER_SIZE;
	m_Rear = (int)DEFINE::HEADER_SIZE;
}

CSerializableBuffer::CSerializableBuffer(int size) : m_MaxSize(size)
{
	m_Buffer = new char[m_MaxSize];
	m_HeaderFront = 0;
	m_Front = (int)DEFINE::HEADER_SIZE;
	m_Rear = (int)DEFINE::HEADER_SIZE;
}

CSerializableBuffer::~CSerializableBuffer()
{
	delete m_Buffer;
}

bool CSerializableBuffer::EnqueueHeader(char *buffer, int size)
{
	// 이 상황은 이미 헤더를 삽입한 것
	//	* CGameServer::SendSector에서 발생 가능
	//  * 싱글 스레드에서는 문제가 발생할 여지가 없으므로 그냥 return true
	if (m_HeaderFront + size > m_Front)
		return true;

	memcpy_s(m_Buffer + m_HeaderFront, m_Front - m_HeaderFront, buffer, size);

	m_HeaderFront += size;

	return true;
}

bool CSerializableBuffer::Enqueue(char *buffer, int size)
{
	if (m_MaxSize - m_Rear > size)
	{
		// TODO: resize

		return false;
	}

	memcpy_s(m_Buffer + m_Rear, m_MaxSize - m_Rear, buffer, size);
	MoveWritePos(size);

	return true;
}

bool CSerializableBuffer::Dequeue(char *buffer, int size)
{
	if (GetDataSize() < size)
	{
		return false;
	}

	memcpy_s(buffer, size, m_Buffer + m_Front, size);
	MoveReadPos(size);

	return true;
}