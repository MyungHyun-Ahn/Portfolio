#include "pch.h"
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "CServer.h"
#include "IContents.h"
#include "ContentsHandle.h"
#include "CContentsRuntime.h"
#include "ChatMetrics.h"
#include "CommonProtocol.h"
#include "PacketFraming.h"
#include "CPlayer.h"
#include "CSector.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketSend.g.h"
#include "ServerPacketDispatch.g.h"
#include "IServerPacketHandler.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;


	CChatContents::CChatContents(ContentsRuntimeLib::ContentsInstanceId instanceId, CChatServer &server) noexcept
		: IContents(instanceId)
		, m_server(server)
		, m_processPacket(*this)
	{
		m_umapSessions.reserve(256);
		m_umapNonLoginPlayer.reserve(256);
		m_umapLoginPlayer.reserve(256);
	}

	void CChatContents::OnEnter(UINT64 sessionId) noexcept
	{
		g_pLogger->RegisterLogProducer();
		CPlayer *pPlayer = CPlayer::Alloc();
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
		CPlayer *pPlayer = it->second;
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

	bool CChatContents::onRecv(UINT64 sessionId, NetworkCommonLib::CSerializableBufferView &message) noexcept
	{
		RecordChatContentUpdate();
		std::uint16_t type = 0;
		const bool isHandled = DispatchServerPacket(sessionId, message, m_processPacket, &type);
		switch (static_cast<EPacketType>(type))
		{
		case EPacketType::ReqLogin:
			RecordChatLoginRequest();
			break;
		case EPacketType::ReqSectorMove:
			RecordChatSectorMoveRequest();
			break;
		case EPacketType::ReqMessage:
			RecordChatMessageRequest();
			break;
		default:
			break;
		}
		if (!isHandled)
		{
			g_pLogger->WriteLog(ELogLevel::Error, L"ChatServer", sessionId, std::source_location::current(),
				L"event=packet_rejected packet_type=%u | 패킷을 처리하지 못했습니다.", type);
			return false;
		}
		return true;
	}

	void CChatContents::SendSector(UINT64 sessionId, WORD sectorY, WORD sectorX,
		const ServerNetworkLib::SendPacketPtr &spMessage) noexcept
	{
		auto *pMessage = spMessage.GetRealPtr();
		// 공유하는 송신 버퍼는 큐에 넣기 전에 한 번만 준비한다.
		NetworkCommonLib::PreparePacket(*pMessage, kPacketCode, m_server.GetPacketMode());
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
					RecordChatMessageSendRequest();
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

	void CChatContents::OnFrame() noexcept
	{
		// 세션 하나의 요청이 다른 세션이나 heartbeat를 굶기지 않도록 프레임당 상한을 둔다.
		constexpr int kMaxPacketsPerSession = 64;
		for (const auto &entry : m_umapSessions)
		{
			const auto sessionId = entry.first;
			ServerNetworkLib::RecvPacketPtr upPacket(nullptr, NetworkCommonLib::CSerializableBufferView::Free);
			for (int count = 0; count < kMaxPacketsPerSession; ++count)
			{
				if (!m_server.TryPopRecvPacket(sessionId, upPacket))
					break;
				if (!onRecv(sessionId, *upPacket))
				{
					m_server.Disconnect(sessionId);
					break;
				}
			}
		}

		const DWORD nowTime = timeGetTime();
		if (nowTime - m_prevHeartbeatTime >= 1000)
		{
			m_prevHeartbeatTime = nowTime;
			nonLoginHeartbeat();
			loginHeartbeat();
		}
		RecordCompletedChatFrame();
	}
}
