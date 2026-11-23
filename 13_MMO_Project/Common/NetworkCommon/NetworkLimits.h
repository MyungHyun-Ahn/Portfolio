#pragma once

namespace NetworkCommonLib
{
constexpr UINT64 kSessionIndexMask = 0xffff000000000000;
constexpr UINT64 kSessionIdMask = 0x0000ffffffffffff;
constexpr INT kWsasendMaxBufferCount = 512;
// 링버퍼의 여유 칸과 int 커서 덧셈까지 포함한 상한.
constexpr int kMaxRecvBufferBytes = 0x3fffffff;
}
