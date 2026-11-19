#pragma once

namespace NetworkLib
{
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
