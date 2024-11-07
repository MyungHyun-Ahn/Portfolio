#pragma once

#pragma warning(disable : 26495) // 변수 초기화 경고
#pragma warning(disable : 26110) // 락 잠금 해제 경고


// 오브젝트 풀 사용 여부
// #define USE_OBJECT_POOL

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
#include <process.h>

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
#include "DefineSingleton.h"
#include "DefineServer.h"
#include "CObjectPool.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CLogger.h"