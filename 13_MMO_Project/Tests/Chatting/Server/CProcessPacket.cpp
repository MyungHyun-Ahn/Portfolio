#include "pch.h"
#include "NetworkLimits.h"
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
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IServerPacketHandler.g.h"
#include "ServerPacketSend.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	bool CProcessPacket::HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept
	{
		auto it = m_contents.m_umapNonLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapNonLoginPlayer.end())
			return false;

		SResLogin response;
		const INT64 accountNo = packet.m_accountNo;
		response.m_accountNo = accountNo;
		// 로컬 테스트 전용 인증이다.
		if (accountNo <= 0 || packet.m_id.front() == L'\0' || packet.m_nickname.front() == L'\0'
			|| wmemchr(packet.m_id.data(), L'\0', packet.m_id.size()) == nullptr
			|| wmemchr(packet.m_nickname.data(), L'\0', packet.m_nickname.size()) == nullptr
			|| std::memcmp(packet.m_sessionKey.data(), kTestSessionKey, kSessionKeyBytes) != 0)
		{
			m_contents.SendResponse(sessionId, MakePacketResLogin(response));
			return false;
		}
		for (const auto &entry : m_contents.m_umapLoginPlayer)
		{
			if (entry.second->m_accountNo == accountNo)
				return false;
		}

		response.m_status = TRUE;
		auto *pResponse = MakePacketResLogin(response);
		if (pResponse == nullptr)
			return false;
		CPlayer *pPlayer = it->second;
		pPlayer->m_accountNo = accountNo;
		pPlayer->m_prevRecvTime = timeGetTime();
		std::copy(packet.m_id.begin(), packet.m_id.end(), pPlayer->m_id);
		std::copy(packet.m_nickname.begin(), packet.m_nickname.end(), pPlayer->m_nickname);
		m_contents.m_umapNonLoginPlayer.erase(it);
		m_contents.m_umapLoginPlayer.emplace(sessionId, pPlayer);
		m_contents.SendResponse(sessionId, pResponse);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"LOGIN account=%lld", accountNo);
		return true;
	}

	bool CProcessPacket::HandleReqSectorMove(UINT64 sessionId, const SReqSectorMove &packet) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end())
			return false;

		CPlayer *pPlayer = it->second;
		const INT64 accountNo = packet.m_accountNo;
		const WORD sectorX = packet.m_sectorX;
		const WORD sectorY = packet.m_sectorY;
		if (accountNo != pPlayer->m_accountNo || sectorX >= kMaxSectorX || sectorY >= kMaxSectorY)
			return false;

		SResSectorMove response;
		response.m_accountNo = accountNo;
		response.m_sectorX = sectorX;
		response.m_sectorY = sectorY;
		auto *pResponse = MakePacketResSectorMove(response);
		if (pResponse == nullptr)
			return false;
		// 콘텐츠 프레임 안에서 이전 섹터 제거 후 등록한다.
		if (pPlayer->m_sectorX != 0xff && pPlayer->m_sectorY != 0xff)
			m_contents.m_sectors[pPlayer->m_sectorY][pPlayer->m_sectorX].m_players.erase(sessionId);
		m_contents.m_sectors[sectorY][sectorX].m_players.emplace(sessionId, pPlayer);
		pPlayer->m_sectorX = sectorX;
		pPlayer->m_sectorY = sectorY;
		pPlayer->m_prevRecvTime = timeGetTime();
		m_contents.SendResponse(sessionId, pResponse);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"SECTOR account=%lld x=%u y=%u", accountNo, sectorX, sectorY);
		return true;
	}

	bool CProcessPacket::HandleReqMessage(UINT64 sessionId, const SReqMessage &packet) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end())
			return false;

		CPlayer *pPlayer = it->second;
		if (packet.m_accountNo != pPlayer->m_accountNo || pPlayer->m_sectorX == 0xff || pPlayer->m_sectorY == 0xff
			|| packet.m_message.empty() || packet.m_message.size() > kMaxMessageBytes / sizeof(WCHAR))
			return false;

		SResMessage response;
		response.m_accountNo = packet.m_accountNo;
		std::copy_n(pPlayer->m_id, response.m_id.size(), response.m_id.begin());
		std::copy_n(pPlayer->m_nickname, response.m_nickname.size(), response.m_nickname.begin());
		try
		{
			response.m_message = packet.m_message;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}
		auto *pResponse = MakePacketResMessage(response);
		if (pResponse == nullptr)
			return false;
		pPlayer->m_prevRecvTime = timeGetTime();
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

	bool CProcessPacket::HandleHeartbeatNoti(UINT64 sessionId, const SHeartbeatNoti &packet) noexcept
	{
		auto it = m_contents.m_umapLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapLoginPlayer.end() || packet.m_accountNo != it->second->m_accountNo)
			return false;
		it->second->m_prevRecvTime = timeGetTime();
		return true;
	}
}
