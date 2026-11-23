#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "CServer.h"
#include "CBaseContents.h"
#include "CMonitor.h"
#include "CommonProtocol.h"
#include "CPlayer.h"
#include "CSector.h"
#include "PacketTypes.g.h"
#include "IServerPacketHandler.g.h"
#include "ServerPacketSend.g.h"
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
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=session_enter | 세션이 콘텐츠에 진입했습니다.");
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
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=session_leave | 세션이 콘텐츠를 떠났습니다.");
	}

	ServerNetworkLib::ERecvResult CChatContents::OnRecv(UINT64 sessionId,
		NetworkCommonLib::CSerializableBufferView *pMessage, int /*delayFrame*/) noexcept
	{
		InterlockedIncrement(&ServerNetworkLib::g_monitor.m_updateTps);
		// 테스트 채팅의 기존 디스패처에만 풀 버퍼로 본문을 전달한다.
		auto spMessage = MHLib::utils::CSmartPtr<NetworkCommonLib::CSerializableBuffer>::MakeShared();
		auto &message = *spMessage;
		const int bodyBytes = pMessage->GetDataSize();
		if (!pMessage->Dequeue(message.GetContentBufferPtr(), bodyBytes))
			return ServerNetworkLib::ERecvResult::Failed;
		message.MoveWritePos(bodyBytes);
		std::uint16_t type{};
		message >> type;
		if (!message.IsValid())
		{
			return ServerNetworkLib::ERecvResult::Failed;
		}
		switch (static_cast<EPacketType>(type))
		{
		case EPacketType::ReqLogin:
			InterlockedIncrement(&ServerNetworkLib::g_monitor.m_loginReq);
			break;
		case EPacketType::ReqSectorMove:
			InterlockedIncrement(&ServerNetworkLib::g_monitor.m_sectorMoveReq);
			break;
		case EPacketType::ReqMessage:
			InterlockedIncrement(&ServerNetworkLib::g_monitor.m_chatMsgReq);
			break;
		default:
			break;
		}
		// View 반환은 CBaseContents에서 수행한다.
		if (!DispatchServerPacket(type, sessionId, message, m_processPacket))
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", sessionId, std::source_location::current(),
				L"event=packet_rejected packet_type=%u | 패킷을 처리하지 못했습니다.", type);
			return ServerNetworkLib::ERecvResult::Failed;
		}
		return ServerNetworkLib::ERecvResult::Received;
	}

	void CChatContents::SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX,
		NetworkCommonLib::CSerializableBuffer *pMessage) noexcept
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
					InterlockedIncrement(&ServerNetworkLib::g_monitor.m_chatMsgRes);
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
