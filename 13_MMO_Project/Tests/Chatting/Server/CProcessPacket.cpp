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

	namespace
	{
		void ReleaseResponse(NetworkLib::CSerializableBuffer<FALSE> *pResponse) noexcept
		{
			if (pResponse != nullptr && pResponse->DecreaseRef() == 0)
				NetworkLib::CSerializableBuffer<FALSE>::Free(pResponse);
		}
	}

	void CProcessPacket::sendPacket(UINT64 sessionId, EPacketType type, NetworkLib::CSerializableBuffer<FALSE> &packet) noexcept
	{
		if (type == EPacketType::ResMessage)
			packet.SetSessionId(sessionId);
		NetworkLib::PrepareNetPacket(packet, kPacketCode);
		m_contents.m_server.SendPacket(sessionId, &packet);
	}

	bool CProcessPacket::HandleReqLogin(UINT64 sessionId, const SReqLogin &packet) noexcept
	{
		auto it = m_contents.m_umapNonLoginPlayer.find(sessionId);
		if (it == m_contents.m_umapNonLoginPlayer.end())
			return false;

		const INT64 accountNo = packet.m_accountNo;
		// 로컬 테스트 전용 인증이다.
		if (accountNo <= 0 || packet.m_id.front() == L'\0' || packet.m_nickname.front() == L'\0'
			|| wmemchr(packet.m_id.data(), L'\0', packet.m_id.size()) == nullptr
			|| wmemchr(packet.m_nickname.data(), L'\0', packet.m_nickname.size()) == nullptr
			|| std::memcmp(packet.m_sessionKey.data(), kTestSessionKey, kSessionKeyBytes) != 0)
		{
			ReleaseResponse(SendResLogin(sessionId, FALSE, accountNo));
			return false;
		}
		for (const auto &entry : m_contents.m_umapLoginPlayer)
		{
			if (entry.second->m_accountNo == accountNo)
				return false;
		}

		SResLogin response;
		response.m_status = TRUE;
		response.m_accountNo = accountNo;
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
		ReleaseResponse(submitPreparedPacket(sessionId, EPacketType::ResLogin, pResponse));
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
		ReleaseResponse(submitPreparedPacket(sessionId, EPacketType::ResSectorMove, pResponse));
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

		std::array<WCHAR, kNameLength> id{};
		std::array<WCHAR, kNameLength> nickname{};
		std::copy_n(pPlayer->m_id, id.size(), id.begin());
		std::copy_n(pPlayer->m_nickname, nickname.size(), nickname.begin());
		auto *pResponse = SendResMessage(sessionId, packet.m_accountNo, id, nickname, packet.m_message);
		if (pResponse == nullptr)
			return false;
		pPlayer->m_prevRecvTime = timeGetTime();
		InterlockedIncrement(&NetworkLib::g_monitor.m_chatMsgRes);
		m_contents.SendSector(sessionId, pPlayer->m_sectorY, pPlayer->m_sectorX, pResponse);
		ReleaseResponse(pResponse);
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
