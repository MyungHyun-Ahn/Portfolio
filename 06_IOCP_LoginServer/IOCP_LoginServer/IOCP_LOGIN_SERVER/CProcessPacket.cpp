#include "pch.h"
#include "CNetServer.h"
#include "CNetSession.h"
#include "DBSetting.h"
#include "CDBConnector.h"
#include "CRedisConnector.h"
#include "LoginServerSetting.h"
#include "CUser.h"
#include "CLoginServer.h"
#include "CommonProtocol.h"
#include "CProcessPacket.h"
#include "CGenPacket.h"

bool CLoginProcessPacket::PacketProcReqLogin(UINT64 sessionId, CSmartPtr<CSerializableBufferView<FALSE>> message) noexcept
{
	std::unordered_map<UINT64, CUser> &userMap = m_pLoginServer->m_umapUser;
	AcquireSRWLockExclusive(&m_pLoginServer->m_userMapLock);
	CUser &user = userMap[sessionId];
	user.m_dwPrevRecvTime = timeGetTime();
	ReleaseSRWLockExclusive(&m_pLoginServer->m_userMapLock);

	INT64	accountNo;
	char	sessionKey[64];

	*message >> accountNo;
	message->Dequeue(sessionKey, 64);

	if (message->GetDataSize() != 0)
	{
		return false;
	}

	CRedisConnector *redisConnector = CRedisConnector::GetRedisConnector();
	redisConnector->Set(accountNo, sessionKey);

	WCHAR ID[20];
	WCHAR Nickname[20];

	CDBConnector *mySqlConnector = CDBConnector::GetDBConnector();
	mySqlConnector->Query(L"SELECT `userid`, `usernick` FROM `account` WHERE `accountno` = '%lld';", accountNo);
	{
		DBGetResult dbResult;
		MYSQL_ROW dbRow = mySqlConnector->GetRow();
		if (dbRow == nullptr)
		{
			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"Failed to find account, accountno : %lld", accountNo);
			return false;
		}

		// ID 읽기
		if (!CEncodingConvertor::Utf8ToUtf16(dbRow[0], 20, ID, sizeof(ID) / sizeof(WCHAR)))
		{
			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"ID 변환 오류, SessionId : %lld", sessionId);
			return false;
		}

		// Nickname 읽기
		if (!CEncodingConvertor::Utf8ToUtf16(dbRow[1], 20, Nickname, sizeof(Nickname) / sizeof(WCHAR)))
		{
			g_Logger->WriteLog(L"SYSTEM", L"MySQL", LOG_LEVEL::ERR, L"Nickname 변환 오류, SessionId : %lld", sessionId);
			return false;
		}
	}

	InterlockedIncrement(&g_monitor.m_lAuthTPS);

	CSerializableBuffer<FALSE> *loginRes = CGenPacket::makePacketResLogin(accountNo, (BYTE)dfLOGIN_STATUS_OK, ID, Nickname, L"0.0.0.0", 0, LOGIN_SERVER_SETTING::CHAT_SERVER_IP_1.c_str(), LOGIN_SERVER_SETTING::CHAT_SERVER_PORT_1);
	loginRes->IncreaseRef();
	m_pLoginServer->SendPacket(sessionId, loginRes);
	if (loginRes->DecreaseRef() == 0)
		CSerializableBuffer<FALSE>::Free(loginRes);

	return true;
}
