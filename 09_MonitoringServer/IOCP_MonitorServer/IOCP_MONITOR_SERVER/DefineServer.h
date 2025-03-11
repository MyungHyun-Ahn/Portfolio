#pragma once

constexpr UINT64 SESSION_INDEX_MASK = 0xffff000000000000;
constexpr UINT64 SESSION_ID_MASK = 0x0000ffffffffffff;
constexpr INT WSASEND_MAX_BUFFER_COUNT = 128;

namespace NET_SERVER
{
#pragma pack(push, 1)
	struct NetHeader
	{
		BYTE code = 109;
		USHORT len;
		BYTE randKey;
		BYTE checkSum;
	};
#pragma pack(pop)
};

namespace LAN_SERVER
{
#pragma pack(push, 1)
	struct LanHeader
	{
		USHORT len;
	};
#pragma pack(pop)
};