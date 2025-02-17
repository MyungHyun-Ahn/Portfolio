#pragma once

class CDBConnector
{
public:
	CDBConnector()
	{
		if (mysql_init(&m_conn) == nullptr)
		{
			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"mysql_init 실패");
			throw std::runtime_error("mysql_init failed");
		}

		m_connection = mysql_real_connect(&m_conn, MYSQL_SETTING::IP.c_str(), MYSQL_SETTING::ID.c_str(), MYSQL_SETTING::PASSWORD.c_str(), MYSQL_SETTING::DEFAULT_SCHEMA.c_str(), MYSQL_SETTING::PORT, nullptr, 0);
		if (m_connection == nullptr)
		{
			const char *errorMsg = mysql_error(&m_conn);

			// CHAR to WCHAR 변환
			WCHAR utf16ErrorMsg[512];
			if (!CEncodingConvertor::Utf8ToUtf16(errorMsg, -1, utf16ErrorMsg, sizeof(utf16ErrorMsg) / sizeof(WCHAR)))
			{
				g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"UTF-16 변환 오류");
				throw std::runtime_error("MySQL connection error and UTF-16 conversion failed");
			}

			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, utf16ErrorMsg);
			throw std::runtime_error("MySQL connection error");
		}
	}

	~CDBConnector() noexcept
	{
		mysql_close(m_connection);
	}

	inline static CDBConnector *GetDBConnector() noexcept
	{
		thread_local CDBConnector dbConnector;
		return &dbConnector;
	}

	// Update Insert 문이라면 여기서 끝
	BOOL Query(const WCHAR *fmt, ...) noexcept
	{
		int queryStat;
		HRESULT ret;
		WCHAR queryMsg[512];

		va_list va;
		va_start(va, fmt);
		ret = StringCchVPrintf(queryMsg, 512, fmt, va);
		if (ret != S_OK)
		{
			if (ret == STRSAFE_E_INSUFFICIENT_BUFFER)
			{
				g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"쿼리 버퍼 크기 부족");
				return FALSE;
			}
		}
		va_end(va);

		// WCHAR to CHAR 변환
		char utf8QueryMsg[512];
		if (!CEncodingConvertor::Utf16ToUtf8(queryMsg, -1, utf8QueryMsg, sizeof(utf8QueryMsg)))
		{
			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"UTF-8 변환 오류");
			return FALSE;
		}
		{
			PROFILE_BEGIN(0, "DB");
			queryStat = mysql_query(m_connection, utf8QueryMsg);
		}
		if (queryStat != 0)
		{
			const char *errorMsg = mysql_error(&m_conn);

			// CHAR to WCHAR 변환
			WCHAR utf16ErrorMsg[512];
			if (!CEncodingConvertor::Utf8ToUtf16(errorMsg, -1, utf16ErrorMsg, sizeof(utf16ErrorMsg) / sizeof(WCHAR)))
			{
				g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"UTF-16 변환 오류");
				return FALSE;
			}

			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, utf16ErrorMsg);
			return FALSE;
		}

		return TRUE;
	}

	inline void StartGetResult() noexcept
	{
		m_sqlResult = mysql_store_result(m_connection);
	}

	// ROW가 NULL 인지 체크하고 사용
	// WCHAR로 변환하여 사용
	inline MYSQL_ROW GetRow() noexcept
	{
		return mysql_fetch_row(m_sqlResult);
	}

	inline void EndGetResult() noexcept
	{
		mysql_free_result(m_sqlResult);
	}

private:
	MYSQL m_conn;
	MYSQL *m_connection = nullptr;
	MYSQL_RES *m_sqlResult = nullptr;
};

// RAII 패턴 -> 결과를 얻고 {} 로 감싸고 호출
class DBGetResult
{
public:
	inline DBGetResult() noexcept
	{
		m_connector = CDBConnector::GetDBConnector();
		m_connector->StartGetResult();
	}

	inline ~DBGetResult() noexcept
	{
		m_connector->EndGetResult();
	}

private:
	CDBConnector *m_connector;
};