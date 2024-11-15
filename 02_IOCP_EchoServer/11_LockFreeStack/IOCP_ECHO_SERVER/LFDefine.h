#pragma once

// Identifier
constexpr ULONG_PTR ADDR_MASK = 0x00007FFFFFFFFFFF;
constexpr ULONG_PTR IDENTIFIER_MASK = ~ADDR_MASK;
constexpr INT ZERO_BIT = 17;
constexpr INT NON_ZERO_BIT = 64 - ZERO_BIT;
constexpr ULONG_PTR MAX_IDENTIFIER = 131071;

inline ULONG_PTR CombineIdentAndAddr(ULONG_PTR ident, ULONG_PTR addr)
{
	ident = (ident % MAX_IDENTIFIER) << NON_ZERO_BIT;
	return ident | addr;
}

inline ULONG_PTR GetIdentifier(ULONG_PTR combined)
{
	return (combined & IDENTIFIER_MASK) >> NON_ZERO_BIT;
}

inline ULONG_PTR GetAddress(ULONG_PTR combined)
{
	return combined & ADDR_MASK;
}