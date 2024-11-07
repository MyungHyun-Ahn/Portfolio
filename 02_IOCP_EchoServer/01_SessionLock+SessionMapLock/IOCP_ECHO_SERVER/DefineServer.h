#pragma once

enum class IOOperation
{
	RECV,
	SEND
};

struct OverlappedEx
{
	OverlappedEx(IOOperation oper) : m_Operation(oper), m_Overlapped() {}

	OVERLAPPED m_Overlapped;
	IOOperation m_Operation;
};
