#pragma once

namespace NetworkLib
{
constexpr UINT64 kSessionIndexMask = 0xffff000000000000;
constexpr UINT64 kSessionIdMask = 0x0000ffffffffffff;
constexpr INT kWsasendMaxBufferCount = 512;

#pragma pack(push, 1)
struct SNetHeader
{
	BYTE code = 119;
	USHORT len;
	BYTE randKey;
	BYTE checkSum;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct SLanHeader
{
	USHORT len;
};
#pragma pack(pop)
}
