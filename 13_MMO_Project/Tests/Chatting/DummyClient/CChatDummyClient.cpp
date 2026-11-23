#include "pch.h"
#include <new>
#include <utility>
#include "CSerializableBuffer.h"
#include "CommonProtocol.h"
#include "CNetClientManager.h"
#include "CNetClient.h"
#include "MHLib/serialization/ISerializable.h"
#include "IncludePacketAll.g.h"
#include "IClientPacketHandler.g.h"
#include "CChatNetClient.g.h"
#include "SDummyConfig.h"
#include "SDummyRunStats.h"
#include "CChatDummyClient.h"

namespace Chatting
{
	CChatDummyClient::CChatDummyClient(NetworkLib::CNetClientManager &network, const SDummyConfig &config,
		SDummyRunStats &stats, size_t index, const std::wstring &runMarker)
		: CChatNetClient(network)
		, m_config(config)
		, m_stats(stats)
		, m_runMarker(runMarker)
		, m_dummyAccount(config.m_accountBase + static_cast<INT64>(index))
		, m_id(std::to_wstring(m_dummyAccount))
		, m_name(L"dummy" + std::to_wstring(index + 1))
		, m_phase(EChatPhase::Idle)
		, m_phaseBeforeClose(EChatPhase::Idle)
	{
		if (m_name.size() > kMaxNameCodeUnits)
			m_name = m_id;
	}

	bool CChatDummyClient::Start() noexcept
	{
		// IsActive()는 이전 연결의 terminal 이벤트를 소비한 뒤에만 해제된다.
		if (!Connect(m_config.m_ip.c_str(), m_config.m_port, kPacketCode, m_config.m_connectTimeoutMilliseconds))
		{
			recordFailure("connect_submit_failed");
			return false;
		}
		setPhase(EChatPhase::ConnectRequested, m_config.m_connectTimeoutMilliseconds);
		return true;
	}

	void CChatDummyClient::Close() noexcept
	{
		if (!IsActive() || m_phase == EChatPhase::Closing)
			return;
		m_phaseBeforeClose = m_phase;
		setPhase(EChatPhase::Closing);
		Disconnect();
	}

	void CChatDummyClient::Pump() noexcept
	{
		Tick();
		(void)checkDeadline();
	}

	bool CChatDummyClient::IsReady() const noexcept
	{
		return m_phase == EChatPhase::Ready && IsConnected();
	}

	bool CChatDummyClient::HasPendingAction() const noexcept
	{
		return m_action != EAction::None;
	}

	bool CChatDummyClient::TakeActionCompletion() noexcept
	{
		return std::exchange(m_hasActionCompletion, false);
	}

	int CChatDummyClient::GetSectorIndex() const noexcept
	{
		return m_sectorIndex;
	}

	void CChatDummyClient::StartMove(int sectorIndex) noexcept
	{
		if (!ReqSectorMove(static_cast<WORD>(sectorIndex % kMaxSectorX), static_cast<WORD>(sectorIndex / kMaxSectorX)))
		{
			recordFailure("move_submit_failed");
			return;
		}
		m_action = EAction::Move;
		m_deadline = TClock::now() + std::chrono::milliseconds(m_config.m_sectorTimeoutMilliseconds);
		++m_stats.m_moveSubmitted;
	}

	void CChatDummyClient::StartMessage() noexcept
	{
		try
		{
			m_expectedMessage = m_runMarker + m_id + L":" + std::to_wstring(++m_sequence) + L" 루프백 😀 100%";
		}
		catch (const std::bad_alloc &)
		{
			recordFailure("message_allocation_failed");
			return;
		}
		const auto startedAt = TClock::now();
		if (!SendReqMessage(m_dummyAccount, m_expectedMessage))
		{
			recordFailure("message_submit_failed");
			return;
		}
		m_messageStartedAt = startedAt;
		m_action = EAction::Message;
		m_deadline = TClock::now() + std::chrono::milliseconds(m_config.m_deliveryTimeoutMilliseconds);
		++m_stats.m_messageSubmitted;
	}

	void CChatDummyClient::SendHeartbeat() noexcept
	{
		if (SendNotiHeartbeat(m_dummyAccount))
			++m_stats.m_heartbeatSubmitted;
		else
			recordFailure("heartbeat_submit_failed");
	}

	bool CChatDummyClient::onConnect() noexcept
	{
		if (m_phase == EChatPhase::Closing || !checkDeadline())
			return false;
		setPhase(EChatPhase::Connected);
		std::array<char, kSessionKeyBytes> sessionKey{};
		std::copy_n(kTestSessionKey, sessionKey.size(), sessionKey.begin());
		if (!ReqLogin(m_dummyAccount, m_id, m_name, sessionKey))
		{
			recordFailure("login_submit_failed");
			Close();
			return false;
		}
		setPhase(EChatPhase::LoginPending, m_config.m_loginTimeoutMilliseconds);
		return true;
	}

	void CChatDummyClient::onRecvRejected() noexcept
	{
		const char *pReason = "protocol_rejected";
		if (m_phase == EChatPhase::LoginPending)
			pReason = "login_protocol_rejected";
		else if (m_phase == EChatPhase::SectorPending || m_action == EAction::Move)
			pReason = "sector_protocol_rejected";
		recordFailure(pReason);
		m_hasReportedRecvRejection = true;
	}

	void CChatDummyClient::onLoginAccepted(const SResLogin &) noexcept
	{
		if (!checkDeadline())
			return;
		if (!ReqSectorMove(m_config.m_sectorX, m_config.m_sectorY))
		{
			recordFailure("sector_submit_failed");
			Close();
			return;
		}
		setPhase(EChatPhase::SectorPending, m_config.m_sectorTimeoutMilliseconds);
	}

	void CChatDummyClient::onSectorEntered(const SResSectorMove &packet) noexcept
	{
		if (!checkDeadline())
			return;
		m_sectorIndex = packet.m_sectorY * kMaxSectorX + packet.m_sectorX;
		if (m_phase == EChatPhase::SectorPending)
			setPhase(EChatPhase::Ready);
		else if (m_action == EAction::Move)
		{
			++m_stats.m_moveCompleted;
			completeAction();
		}
	}

	void CChatDummyClient::onChatMessageEcho(const SResMessage &packet) noexcept
	{
		if (!checkDeadline())
			return;
		if (m_action != EAction::Message || packet.m_accountNo != m_dummyAccount || packet.m_message != m_expectedMessage)
		{
			recordFailure("message_echo_mismatch");
			return;
		}
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(TClock::now() - m_messageStartedAt);
		m_stats.RecordMessageRtt(static_cast<UINT64>(elapsed.count()));
		completeAction();
	}

	void CChatDummyClient::onChatMessageNoti(const SMessageNoti &packet) noexcept
	{
		if (packet.m_accountNo >= m_config.m_accountBase
			&& static_cast<UINT64>(packet.m_accountNo - m_config.m_accountBase) < m_config.m_clientCount
			&& packet.m_message.starts_with(m_runMarker))
			++m_stats.m_peerReceived;
	}

	void CChatDummyClient::onChatDisconnected(bool wasConnected, DWORD error) noexcept
	{
		const bool isClosing = m_phase == EChatPhase::Closing;
		if (!wasConnected && !(isClosing && error == ERROR_OPERATION_ABORTED))
		{
			// 앱의 종료 요청 전에 IOCP가 접속 실패를 확정했으면 원래 실패를 보존한다.
			m_stats.Fail(error == WSAETIMEDOUT ? "connect_timeout" : "connect_failed",
				m_dummyAccount, isClosing ? m_phaseBeforeClose : m_phase, error);
		}
		else if (error != ERROR_OPERATION_ABORTED || (!isClosing && !m_hasReportedRecvRejection))
			m_stats.Fail("unexpected_disconnect", m_dummyAccount, isClosing ? m_phaseBeforeClose : m_phase, error);
		m_hasReportedRecvRejection = false;
		m_action = EAction::None;
		m_hasActionCompletion = false;
		setPhase(EChatPhase::Terminal);
	}

	void CChatDummyClient::setPhase(EChatPhase phase, DWORD timeoutMilliseconds) noexcept
	{
		m_phase = phase;
		m_deadline = timeoutMilliseconds == 0 ? TClock::time_point{} : TClock::now() + std::chrono::milliseconds(timeoutMilliseconds);
		++m_stats.m_stages[static_cast<size_t>(phase)];
	}

	bool CChatDummyClient::checkDeadline() noexcept
	{
		if (m_deadline == TClock::time_point{} || TClock::now() < m_deadline)
			return true;
		const char *pReason = "message_echo_timeout";
		if (m_phase == EChatPhase::ConnectRequested)
			pReason = "connect_timeout";
		else if (m_phase == EChatPhase::LoginPending)
			pReason = "login_timeout";
		else if (m_phase == EChatPhase::SectorPending || m_action == EAction::Move)
			pReason = "sector_timeout";
		recordFailure(pReason);
		Close();
		return false;
	}

	void CChatDummyClient::completeAction() noexcept
	{
		m_action = EAction::None;
		m_deadline = {};
		m_hasActionCompletion = true;
	}

	void CChatDummyClient::recordFailure(const char *pReason, DWORD error) noexcept
	{
		m_stats.Fail(pReason, m_dummyAccount, m_phase, error);
	}
}
