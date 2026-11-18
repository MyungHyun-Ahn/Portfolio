#include "pch.h"
#include "DefineServer.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CNetServer.h"
#include "CBaseContents.h"
#include "CommonProtocol.h"
#include "NetPacket.h"
#include "CPlayer.h"
#include "CSector.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"
#include "CGenPacket.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CProcessPacket::ConsumePacket(EPacketType type, UINT64 sessionId,
		NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		switch (type)
		{
		case EPacketType::ReqLogin:
			InterlockedIncrement(&NetworkLib::g_monitor.m_loginReq);
			return packetProcReqLogin(sessionId, message);
		case EPacketType::ReqSectorMove:
			InterlockedIncrement(&NetworkLib::g_monitor.m_sectorMoveReq);
			return packetProcReqSectorMove(sessionId, message);
		case EPacketType::ReqMessage:
			InterlockedIncrement(&NetworkLib::g_monitor.m_chatMsgReq);
			return packetProcReqMessage(sessionId, message);
		case EPacketType::ReqHeartbeat:
			return packetProcReqHeartbeat(sessionId, message);
		default:
			return false;
		}
	}

	bool CProcessPacket::packetProcReqLogin(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		auto it = m_contents.m_umapNonLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapNonLoginPlayer.end() || message.GetDataSize() != 152)
			return false;

		INT64 accountNo;
		WCHAR id[kNameLength];
		WCHAR nickname[kNameLength];
		char sessionKey[kSessionKeyBytes];
		message >> accountNo;
		message.Dequeue((char *)id, sizeof(id));
		message.Dequeue((char *)nickname, sizeof(nickname));
		message.Dequeue(sessionKey, sizeof(sessionKey));

		// 로컬 테스트 전용 인증이다.
		if (accountNo <= 0 || id[0] == L'\0' || nickname[0] == L'\0'
			|| wmemchr(id, L'\0', kNameLength) == nullptr || wmemchr(nickname, L'\0', kNameLength) == nullptr
			|| memcmp(sessionKey, kTestSessionKey, sizeof(sessionKey)) != 0)
		{
			m_contents.SendResponse(sessionId, CGenPacket::MakePacketResLogin(FALSE, accountNo));
			return false;
		}
		for (const auto &entry : m_contents.m_umapLoginPlayer)
		{
			if (entry.second->m_accountNo == accountNo)
				return false;
		}

		CPlayer *pPlayer = it->second;
		pPlayer->m_accountNo = accountNo;
		pPlayer->m_prevRecvTime = timeGetTime();
		memcpy(pPlayer->m_id, id, sizeof(id));
		memcpy(pPlayer->m_nickname, nickname, sizeof(nickname));
		m_contents.m_umapNonLoginPlayer.erase(it);
		m_contents.m_umapLoginPlayer.emplace(sessionId, pPlayer);
		m_contents.SendResponse(sessionId, CGenPacket::MakePacketResLogin(TRUE, accountNo));
		g_pLogger->WriteLogConsole(ELogLevel::System, L"LOGIN account=%lld", accountNo);
		return true;
	}

	bool CProcessPacket::packetProcReqSectorMove(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end() || message.GetDataSize() != 12)
			return false;

		CPlayer *pPlayer = it->second;
		INT64 accountNo;
		WORD sectorX;
		WORD sectorY;
		message >> accountNo >> sectorX >> sectorY;
		if (accountNo != pPlayer->m_accountNo || sectorX >= kMaxSectorX || sectorY >= kMaxSectorY)
			return false;

		// 콘텐츠 프레임 안에서 이전 섹터 제거 후 등록한다.
		if (pPlayer->m_sectorX != 0xff && pPlayer->m_sectorY != 0xff)
			m_contents.m_sectors[pPlayer->m_sectorY][pPlayer->m_sectorX].m_players.erase(sessionId);
		m_contents.m_sectors[sectorY][sectorX].m_players.emplace(sessionId, pPlayer);
		pPlayer->m_sectorX = sectorX;
		pPlayer->m_sectorY = sectorY;
		pPlayer->m_prevRecvTime = timeGetTime();
		m_contents.SendResponse(sessionId, CGenPacket::MakePacketResSectorMove(accountNo, sectorX, sectorY));
		g_pLogger->WriteLogConsole(ELogLevel::System, L"SECTOR account=%lld x=%u y=%u", accountNo, sectorX, sectorY);
		return true;
	}

	bool CProcessPacket::packetProcReqMessage(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end() || message.GetDataSize() < 10)
			return false;

		CPlayer *pPlayer = it->second;
		INT64 accountNo;
		WORD messageBytes;
		message >> accountNo >> messageBytes;
		if (accountNo != pPlayer->m_accountNo || pPlayer->m_sectorX == 0xff || pPlayer->m_sectorY == 0xff
			|| messageBytes == 0 || messageBytes > kMaxMessageBytes || messageBytes % sizeof(WCHAR) != 0
			|| message.GetDataSize() != messageBytes)
			return false;

		WCHAR chatMessage[kMaxMessageBytes / sizeof(WCHAR)];
		message.Dequeue((char *)chatMessage, messageBytes);
		pPlayer->m_prevRecvTime = timeGetTime();
		auto *pResponse = CGenPacket::MakePacketResMessage(accountNo, pPlayer->m_id, pPlayer->m_nickname, messageBytes, chatMessage);
		pResponse->IncreaseRef();
		pResponse->SetSessionId(sessionId);
		NetworkLib::PrepareNetPacket(*pResponse, kPacketCode);
		m_contents.m_server.SendPacket(sessionId, pResponse);
		InterlockedIncrement(&NetworkLib::g_monitor.m_chatMsgRes);
		m_contents.SendSector(sessionId, pPlayer->m_sectorY, pPlayer->m_sectorX, pResponse);
		if (pResponse->DecreaseRef() == 0)
			NetworkLib::CSerializableBuffer<FALSE>::Free(pResponse);
		return true;
	}

	bool CProcessPacket::packetProcReqHeartbeat(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> &message) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end() || message.GetDataSize() != sizeof(INT64))
			return false;
		INT64 accountNo;
		message >> accountNo;
		if (accountNo != it->second->m_accountNo)
			return false;
		it->second->m_prevRecvTime = timeGetTime();
		return true;
	}
}
