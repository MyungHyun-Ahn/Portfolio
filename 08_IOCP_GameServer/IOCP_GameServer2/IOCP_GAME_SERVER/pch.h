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
#pragma comment(lib, "Synchronization.lib")

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
#include <unordered_set>
#include <map>
#include <string>
#include <queue>

// ��Ÿ - ���� ��
#include <new>
#include <locale>
#include <DbgHelp.h>
#include <chrono>
#include <functional>