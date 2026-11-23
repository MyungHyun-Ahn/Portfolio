#pragma once

namespace WorldServer
{
	inline constexpr BYTE kFixedMapsPacketCode = 0x77;
	inline constexpr unsigned short kReqAdd = 1;
	inline constexpr unsigned short kResAdd = 2;
	inline constexpr unsigned short kInvalidOpcode = 99;
	inline constexpr UINT64 kFirstMapId = 1;
	inline constexpr UINT64 kSecondMapId = 2;
	inline constexpr int kRequestBytes = sizeof(unsigned short) + sizeof(UINT64) + sizeof(UINT32);
	inline constexpr int kResponseBytes = 2 * sizeof(unsigned short) + 2 * sizeof(UINT64);

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
