#pragma once

#pragma warning(disable : 26495) // ���� �ʱ�ȭ ���
#pragma warning(disable : 26110) // �� ��� ���� ���
#pragma warning(disable : 6387) // AcceptEx ��������

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

// �����̳� �ڷᱸ��
// ���� ���� ���� �����̳ʷ� ������ ����
#include <deque>
#include <vector>
#include <unordered_map>
#include <map>

// ��Ÿ - ���� ��
#include <locale>
#include <DbgHelp.h>

// ���� ������ ���̺귯��
#include "DefineSingleton.h"
#include "DefineServer.h"
#include "CObjectPool.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CLogger.h"
#include "CMonitor.h"
#include "CCrashDump.h"