#pragma once

enum class IOOperation
{
	RECV,
	SEND
};

struct OverlappedEx
{
	OverlappedEx(IOOperation oper) : m_Operation(oper) {}

	OVERLAPPED m_Overlapped;
	IOOperation m_Operation;
};
