#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "CNetServer.h"
#include "CBaseContents.h"
#include "CMonitor.h"
#include "CommonProtocol.h"
#include "NetPacket.h"
#include "CPlayer.h"
#include "CSector.h"
#include "PacketTypes.g.h"
#include "IServerPacketHandler.g.h"
#include "ServerPacketDispatch.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	CChatContents::CChatContents(CChatServer &server) noexcept
		: m_server(server)
		, m_processPacket(*this)
	{
		m_umapSessions.reserve(256);
		m_umapNonLoginPlayer.reserve(256);
		m_umapLoginPlayer.reserve(256);
	}

	void CChatContents::OnEnter(UINT64 sessionId, void *pObject) noexcept
	{
		CPlayer *pPlayer = pObject == nullptr ? CPlayer::Alloc() : static_cast<CPlayer *>(pObject);
		m_umapSessions.emplace(sessionId, pPlayer);
		m_umapNonLoginPlayer.emplace(sessionId, pPlayer);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"ENTER session=%llu", sessionId);
	}

	void CChatContents::OnLeave(UINT64 sessionId) noexcept
	{
		auto it = m_umapSessions.find(sessionId);
		if (it == m_umapSessions.end())
			return;
		CPlayer *pPlayer = static_cast<CPlayer *>(it->second);
		// 섹터에서 제거
		if (pPlayer->m_sectorY != 0xff && pPlayer->m_sectorX != 0xff)
			m_sectors[pPlayer->m_sectorY][pPlayer->m_sectorX].m_players.erase(sessionId);
		m_umapNonLoginPlayer.erase(sessionId);
		m_umapLoginPlayer.erase(sessionId);
		m_umapSessions.erase(it);
		CPlayer::Free(pPlayer);
		g_pLogger->WriteLogConsole(ELogLevel::System, L"LEAVE session=%llu", sessionId);
	}

	NetworkLib::ERecvResult CChatContents::OnRecv(UINT64 sessionId,
		NetworkLib::CSerializableBuffer<FALSE> *pMessage, int /*delayFrame*/) noexcept
	{
		InterlockedIncrement(&NetworkLib::g_monitor.m_updateTps);
		std::uint16_t type{};
		*pMessage >> type;
		if (!pMessage->IsValid())
		{
			return NetworkLib::ERecvResult::Failed;
		}
		switch (static_cast<EPacketType>(type))
		{
		case EPacketType::ReqLogin:
			InterlockedIncrement(&NetworkLib::g_monitor.m_loginReq);
			break;
		case EPacketType::ReqSectorMove:
			InterlockedIncrement(&NetworkLib::g_monitor.m_sectorMoveReq);
			break;
		case EPacketType::ReqMessage:
			InterlockedIncrement(&NetworkLib::g_monitor.m_chatMsgReq);
			break;
		default:
			break;
		}
		// 수신 버퍼의 ref 반환은 CBaseContents에서 수행한다.
		if (!DispatchServerPacket(type, sessionId, *pMessage, m_processPacket))
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"REJECT session=%llu type=%u", sessionId, type);
			return NetworkLib::ERecvResult::Failed;
		}
		return NetworkLib::ERecvResult::Received;
	}

	void CChatContents::SendResponse(UINT64 sessionId, NetworkLib::CSerializableBuffer<FALSE> *pMessage) noexcept
	{
		if (pMessage == nullptr)
			return;
		pMessage->IncreaseRef();
		NetworkLib::PrepareNetPacket(*pMessage, kPacketCode);
		m_server.SendPacket(sessionId, pMessage);
		if (pMessage->DecreaseRef() == 0)
			NetworkLib::CSerializableBuffer<FALSE>::Free(pMessage);
	}

	void CChatContents::SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX,
		NetworkLib::CSerializableBuffer<FALSE> *pMessage) noexcept
	{
		int startY = sectorY - kSectorViewStart;
		int startX = sectorX - kSectorViewStart;
		for (int y = 0; y < kSectorViewCount; y++)
		{
			for (int x = 0; x < kSectorViewCount; x++)
			{
				if (startY + y < 0 || startY + y >= kMaxSectorY || startX + x < 0 || startX + x >= kMaxSectorX)
					continue;
				for (const auto &entry : m_sectors[startY + y][startX + x].m_players)
				{
					if (entry.first == sessionId)
						continue;
					InterlockedIncrement(&NetworkLib::g_monitor.m_chatMsgRes);
					m_server.SendPacket(entry.first, pMessage);
				}
			}
		}
	}

	void CChatContents::nonLoginHeartbeat() noexcept
	{
		DWORD nowTime = timeGetTime();
		for (const auto &entry : m_umapNonLoginPlayer)
		{
			if (nowTime - entry.second->m_prevRecvTime > kLoginTimeoutMilliseconds)
				m_server.Disconnect(entry.first, TRUE);
		}
	}

	void CChatContents::loginHeartbeat() noexcept
	{
		DWORD nowTime = timeGetTime();
		for (const auto &entry : m_umapLoginPlayer)
		{
			if (nowTime - entry.second->m_prevRecvTime > kReceiveTimeoutMilliseconds)
				m_server.Disconnect(entry.first, TRUE);
		}
	}

	void CChatContents::OnLoopEnd() noexcept
	{
		if (timeGetTime() - m_prevHeartbeatTime < 1000)
			return;
		m_prevHeartbeatTime = timeGetTime();
		nonLoginHeartbeat();
		loginHeartbeat();
	}
}
