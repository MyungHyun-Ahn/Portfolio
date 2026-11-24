#pragma once

namespace WorldServer
{
	inline constexpr BYTE kFixedMapsPacketCode = 0x77;
	inline constexpr unsigned short kInvalidOpcode = 99;
	inline constexpr UINT64 kFirstMapId = 1;
	inline constexpr UINT64 kSecondMapId = 2;

	enum class EAddResult : unsigned short
	{
		Applied,
		WrongMap,
		InvalidRequest
	};

	enum class EMoveResult : unsigned short
	{
		Moved,
		InvalidTarget,
		Closed,
		Busy,
		InvalidRequest,
		Disconnected // 내부 결과로만 사용한다.
	};

	struct SAddResponse
	{
		UINT64 mapId;
		UINT64 total;
		EAddResult result;
		bool operator==(const SAddResponse &) const = default;
	};
}
