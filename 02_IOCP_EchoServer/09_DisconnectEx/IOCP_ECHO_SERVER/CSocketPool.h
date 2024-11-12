#pragma once
class CSocketPool
{
public:
	static BOOL Init(int poolSize)
	{
		int errVal;
		InitializeSRWLock(&m_Lock);

		for (int i = 0; i < poolSize; i++)
		{
			SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (sock == INVALID_SOCKET)
			{
				errVal = WSAGetLastError();
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CSocketPool socket() 실패 : %d", errVal);
				return FALSE;
			}

			m_arrSocketPool.push_back(sock);
		}

		return TRUE;
	}

	static SOCKET Alloc()
	{
		int errVal;
		SOCKET retSock;
		AcquireSRWLockExclusive(&m_Lock);
		if (m_arrSocketPool.empty())
		{
			ReleaseSRWLockExclusive(&m_Lock);
			retSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (retSock == INVALID_SOCKET)
			{
				errVal = WSAGetLastError();
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CSocketPool socket() 실패 : %d", errVal);
				return INVALID_SOCKET;
			}
			return retSock;
		}

		retSock = m_arrSocketPool.back();
		m_arrSocketPool.pop_back();
		ReleaseSRWLockExclusive(&m_Lock);

		return retSock;
	}

	static void Free(SOCKET delSock)
	{
		AcquireSRWLockExclusive(&m_Lock);
		m_arrSocketPool.push_back(delSock);
		ReleaseSRWLockExclusive(&m_Lock);
	}

private:
	inline static std::vector<SOCKET>		m_arrSocketPool;
	inline static SRWLOCK					m_Lock;
};

