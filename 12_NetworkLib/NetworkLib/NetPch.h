#pragma once

#pragma warning(disable : 26495) // 변수 초기화 경고
#pragma warning(disable : 26110) // 락 잠금 해제 경고
#pragma warning(disable : 6387) // AcceptEx 인자전달

// 소켓 관련
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

// 윈도우 API
#include <windows.h>

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
#include <unordered_set>
#include <map>
#include <string>
#include <queue>

// 기타 - 언어설정 등
#include <new>
#include <chrono>
#include <functional>

// MHLib
#include "MHLib/utils/DefineSingleton.h"
#include "MHLib/utils/CProfileManager.h"

#include "MHLib/utils/CSmartPtr.h"
#include "MHLib/utils/CLockGuard.h"
#include "MHLib/utils/CLogger.h"
#include "MHLib/debug/CCrashDump.h"
#include "MHLib/security/CEncryption.h"

// Lock-Free
#include "MHLib/memory/CLFMemoryPool.h"
#include "MHLib/memory/CTLSMemoryPool.h"
#include "MHLib/memory/CTLSPagePool.h"
#include "MHLib/containers/CLFStack.h"
#include "MHLib/containers/CLFQueue.h"

#include "DefineServer.h"
#include "COverlappedAllocator.h"

// buffers
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"

#include "BaseEvent.h"