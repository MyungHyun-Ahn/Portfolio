#pragma once

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "DbgHelp.Lib")

// 소켓 관련
#include <WinSock2.h>
#include <WS2tcpip.h>

// 윈도우 API
#include <windows.h>
#include <Psapi.h>
#include <strsafe.h>

// C 런타임 라이브러리
#include <cstdio>
#include <cstdlib>
#include <ctime>

// 컨테이너 자료구조
// 추후 직접 만든 컨테이너로 변경할 생각
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>

// 기타 - 언어설정 등
#include <locale>
#include <DbgHelp.h>

// 직접 정의한 라이브러리
#include "Define.h"
#include "GameDefine.h"
#include "CObjectPool.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CLogger.h"
#include "CProfiler.h"
#include "CCrashDump.h"

// warning disable
#pragma warning(disable : 26495) // 멤버 변수 초기화
#pragma warning(disable : 4244) // 축소 변환 경고
#pragma warning(disable : 6001) // 초기화되지 않은 메모리
