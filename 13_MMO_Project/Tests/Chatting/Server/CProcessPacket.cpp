#include "pch.h"
#include <new>
#include <utility>
#include "NetworkLimits.h"
#include "COverlappedAllocator.h"
#include "CRingBuffer.h"
#include "CSerializableBuffer.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"
#include "SBaseEvent.h"
#include "ServerSetting.h"
#include "CMonitor.h"
#include "CServer.h"
#include "IContents.h"
#include "CContentsRuntime.h"
#include "CommonProtocol.h"
#include "PacketFraming.h"
#include "CPlayer.h"
#include "CSector.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "ServerPacketDispatch.g.h"
#include "ServerPacketSend.g.h"
#include "IServerPacketHandler.g.h"
#include "CProcessPacket.h"
#include "CChatContents.h"
#include "CChatServer.h"

namespace Chatting
{
	using MHLib::utils::ELogLevel;
	using MHLib::utils::g_pLogger;

	void CProcessPacket::sendPacket(UINT64 sessionId, NetworkCommonLib::CSerializableBuffer &packet) noexcept
	{
		NetworkCommonLib::PreparePacket(packet, kPacketCode, m_contents.m_server.GetPacketMode());
		m_contents.m_server.SendPacket(sessionId, &packet);
	}

	bool CProcessPacket::HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept
	{
		auto it = m_contents.m_umapNonLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapNonLoginPlayer.end())
			return false;

		const INT64 accountNo = packet.m_accountNo;
		// 로컬 테스트 전용 인증이다.
		if (accountNo <= 0 || !IsValidString(packet.m_id, kMaxNameCodeUnits) || !IsValidString(packet.m_nickname, kMaxNameCodeUnits)
			|| std::memcmp(packet.m_sessionKey.data(), kTestSessionKey, kSessionKeyBytes) != 0)
		{
			SendResLogin(sessionId, FALSE, accountNo);
			return false;
		}
		for (const auto &entry : m_contents.m_umapLoginPlayer)
		{
			if (entry.second->m_accountNo == accountNo)
				return false;
		}

		// 문자열 복사가 끝나기 전에는 플레이어와 로그인 상태를 변경하지 않는다.
		std::wstring id;
		std::wstring nickname;
		try
		{
			id = packet.m_id;
			nickname = packet.m_nickname;
		}
		catch (const std::bad_alloc &)
		{
			return false;
		}

		SResLogin response;
		response.m_status = TRUE;
		response.m_accountNo = accountNo;
		const auto spResponse = MakePacket(response);
		if (spResponse.GetRealPtr() == nullptr)
			return false;
		CPlayer *pPlayer = it->second;
		pPlayer->m_accountNo = accountNo;
		pPlayer->m_prevRecvTime = timeGetTime();
		pPlayer->m_id = std::move(id);
		pPlayer->m_nickname = std::move(nickname);
		m_contents.m_umapNonLoginPlayer.erase(it);
		m_contents.m_umapLoginPlayer.emplace(sessionId, pPlayer);
		SendPacket(sessionId, spResponse);
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=login_accepted account_no=%lld | 테스트 로그인을 승인했습니다.", accountNo);
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
		const auto spResponse = MakePacket(response);
		if (spResponse.GetRealPtr() == nullptr)
			return false;
		// 콘텐츠 프레임 안에서 이전 섹터 제거 후 등록한다.
		if (pPlayer->m_sectorX != 0xff && pPlayer->m_sectorY != 0xff)
			m_contents.m_sectors[pPlayer->m_sectorY][pPlayer->m_sectorX].m_players.erase(sessionId);
		m_contents.m_sectors[sectorY][sectorX].m_players.emplace(sessionId, pPlayer);
		pPlayer->m_sectorX = sectorX;
		pPlayer->m_sectorY = sectorY;
		pPlayer->m_prevRecvTime = timeGetTime();
		SendPacket(sessionId, spResponse);
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=sector_entered account_no=%lld sector_x=%u sector_y=%u | 섹터에 입장했습니다.", accountNo, sectorX, sectorY);
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
		response.m_id = pPlayer->m_id;
		response.m_nickname = pPlayer->m_nickname;
		response.m_message = packet.m_message;
		const auto spResponse = MakePacket(response);
		if (spResponse.GetRealPtr() == nullptr)
			return false;

		// 송신 전에 두 패킷을 모두 만든다. 이미 직렬화한 응답의 문자열은 알림에 옮긴다.
		SMessageNoti notification;
		notification.m_accountNo = response.m_accountNo;
		notification.m_id = std::move(response.m_id);
		notification.m_nickname = std::move(response.m_nickname);
		notification.m_message = std::move(response.m_message);
		const auto spNotification = MakePacket(notification);
		if (spNotification.GetRealPtr() == nullptr)
			return false;

		SendPacket(sessionId, spResponse);
		pPlayer->m_prevRecvTime = timeGetTime();
		InterlockedIncrement(&ServerNetworkLib::g_monitor.m_chatMsgRes);
		m_contents.SendSector(sessionId, pPlayer->m_sectorY, pPlayer->m_sectorX, spNotification);
		g_pLogger->WriteLog(ELogLevel::Debug, L"ChatServer", sessionId, std::source_location::current(),
			L"event=chat_processed sector_x=%u sector_y=%u | 채팅 요청을 처리했습니다.", pPlayer->m_sectorX, pPlayer->m_sectorY);
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
