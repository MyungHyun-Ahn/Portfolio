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

	struct SAddResponse
	{
		UINT64 mapId;
		UINT64 total;
		EAddResult result;
		bool operator==(const SAddResponse &) const = default;
	};
}
