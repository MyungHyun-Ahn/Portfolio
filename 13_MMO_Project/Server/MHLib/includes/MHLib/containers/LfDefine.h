#pragma once

namespace MHLib::containers
{
	// T가 기본 타입이거나 포인터 타입인지 확인하는 Concept 정의
	template<typename TObject>
	concept FundamentalOrPointer = std::is_fundamental_v<TObject> || std::is_pointer_v<TObject>;

	// Identifier
	constexpr ULONG_PTR kAddrMask = 0x00007FFFFFFFFFFF;
	constexpr ULONG_PTR kIdentifierMask = ~kAddrMask;
	constexpr INT kZeroBit = 17;
	constexpr INT kNonZeroBit = 64 - kZeroBit;
	constexpr ULONG_PTR kMaxIdentifier = 131071;

	inline ULONG_PTR CombineIdentAndAddr(ULONG_PTR ident, ULONG_PTR addr)
	{
		ident = (ident % kMaxIdentifier) << kNonZeroBit;
		return ident | addr;
	}

	inline ULONG_PTR GetIdentifier(ULONG_PTR combined)
	{
		return (combined & kIdentifierMask) >> kNonZeroBit;
	}

	inline ULONG_PTR GetAddress(ULONG_PTR combined)
	{
		return combined & kAddrMask;
	}
}