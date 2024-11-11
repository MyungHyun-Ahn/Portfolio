#pragma once

// 서버 정보
constexpr const CHAR *SERVER_IP = "0.0.0.0";
constexpr USHORT SERVER_PORT = 6000;
constexpr USHORT PACKET_HEADER_SIZE = 2;
constexpr UINT64 LOGIN_PACKET = 0x7fffffffffffffff;
constexpr INT WSASEND_MAX_BUFFER_COUNT = 64;
constexpr INT ACCEPTEX_COUNT = 500;