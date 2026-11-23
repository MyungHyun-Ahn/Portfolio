#pragma once

namespace NetworkCommonLib
{
enum class EPacketMode
{
	Net,
	Lan
};

#pragma pack(push, 1)
struct SPacketHeader
{
	BYTE code = 119;
	USHORT len;
	BYTE randKey;
	BYTE checkSum;
};
#pragma pack(pop)
}
