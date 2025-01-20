#pragma once

#pragma warning(disable : 26495) // 변수 초기화 경고
#pragma warning(disable : 26110) // 락 잠금 해제 경고
#pragma warning(disable : 6387) // AcceptEx 인자전달
#pragma warning(disable : 4244) // xutility

// 오브젝트 풀 사용 여부
#define USE_OBJECT_POOL

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "Pdh.lib")

// 소켓 관련
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

// 윈도우 API
#include <windows.h>
#include <Psapi.h>
#include <strsafe.h>
#include <Pdh.h>

// C 런타임 라이브러리
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <process.h>
#include <cwchar>

// 컨테이너 자료구조
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

// 기타 - 언어설정 등
#include <new>
#include <locale>
#include <DbgHelp.h>

#include "DefineSingleton.h"
#include "CProfileManager.h"

#include "CSmartPtr.h"
#include "CLogger.h"
#include "CEncryption.h"

// Lock-Free
#include "LFDefine.h"
#include "CLFMemoryPool.h"
#include "CTLSSharedMemoryPool.h"
#include "CTLSMemoryPool.h"
#include "CLFStack.h"
#include "CLFQueue.h"

#include "CTLSPagePool.h"
#include "CTLSSharedPagePool.h"

#include "COverlappedAllocator.h"

#include "CSingleMemoryPool.h"

// 직접 정의한 라이브러리
#include "DefineServer.h"

#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"

#include "CMonitor.h"
#include "CCrashDump.h"
#include "CMyFileLoader.h"

#include "CDeque.h"