#pragma once

constexpr UINT64 SESSION_INDEX_MASK = 0xffff000000000000;
constexpr UINT64 SESSION_ID_MASK = 0x0000ffffffffffff;

enum class IOOperation
{
	ACCEPTEX,
	RECV,
	SEND
};

struct OverlappedEx
{
	OverlappedEx(IOOperation oper) : m_Operation(oper) {}

	OVERLAPPED m_Overlapped;
	IOOperation m_Operation;
	LONG m_Index;
};
