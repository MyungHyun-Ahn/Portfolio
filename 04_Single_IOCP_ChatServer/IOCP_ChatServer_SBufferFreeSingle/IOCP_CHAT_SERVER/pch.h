#pragma once

#pragma warning(disable : 26495) // ���� �ʱ�ȭ ���
#pragma warning(disable : 26110) // �� ��� ���� ���
#pragma warning(disable : 6387) // AcceptEx ��������
#pragma warning(disable : 4244) // xutility

// ������Ʈ Ǯ ��� ����
#define USE_OBJECT_POOL

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "Pdh.lib")

// ���� ����
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

// ������ API
#include <windows.h>
#include <Psapi.h>
#include <strsafe.h>
#include <Pdh.h>

// C ��Ÿ�� ���̺귯��
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <process.h>
#include <cwchar>

// �����̳� �ڷᱸ��
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

// ��Ÿ - ���� ��
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

// ���� ������ ���̺귯��
#include "DefineServer.h"

#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"

#include "CMonitor.h"
#include "CCrashDump.h"
#include "CMyFileLoader.h"

#include "CDeque.h"