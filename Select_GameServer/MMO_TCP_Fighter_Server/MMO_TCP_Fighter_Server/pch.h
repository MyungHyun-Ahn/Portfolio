#pragma once

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm")
#pragma comment(lib, "DbgHelp.Lib")

// ���� ����
#include <WinSock2.h>
#include <WS2tcpip.h>

// ������ API
#include <windows.h>
#include <Psapi.h>
#include <strsafe.h>

// C ��Ÿ�� ���̺귯��
#include <cstdio>
#include <cstdlib>
#include <ctime>

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
#include "Define.h"
#include "GameDefine.h"
#include "CObjectPool.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CLogger.h"
#include "CProfiler.h"
#include "CCrashDump.h"

// warning disable
#pragma warning(disable : 26495) // ��� ���� �ʱ�ȭ
#pragma warning(disable : 4244) // ��� ��ȯ ���
#pragma warning(disable : 6001) // �ʱ�ȭ���� ���� �޸�
