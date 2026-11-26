#include "ClientLifecyclePch.h"
#include <optional>
#include "CSerializableBuffer.h"
#include "PacketHeaders.h"
#include "PacketFraming.h"
#include "ClientEvent.h"
#include "CClientMailbox.h"
#include "CClientManager.h"
#include "SClientNetworkSettings.h"
#include "CClient.h"
#include "NetworkLimits.h"
#include "CRingBuffer.h"
#include "CSession.h"
#include "CRecvBuffer.h"
#include "CSerializableBufferView.h"

namespace ClientNetworkLib
{
	using NetworkCommonLib::CSerializableBuffer;

	class CClientLifecycleProbe
	{
		static_assert(std::is_same_v<decltype(CClientManager::m_sessions),
			std::unordered_map<SessionId, std::shared_ptr<ClientNetworkLib::CSession>>>);

		static_assert(std::is_same_v<decltype(CClientManager::m_mailboxes),
			std::unordered_map<SessionId, std::shared_ptr<CClientMailbox>>>);

	public:
		static size_t GetMailboxCount(CClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return client.m_mailboxes.size();
		}

		static LPFN_CONNECTEX ReplaceConnectEx(CClientManager &client, LPFN_CONNECTEX pConnectEx)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return std::exchange(client.m_pConnectEx, pConnectEx);
		}

		static auto FindSession(CClientManager &client, SessionId sessionId)
		{
			return client.findSession(sessionId);
		}

		static auto GetFirstSessionEntry(CClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			const auto first = client.m_sessions.begin();
			return first != client.m_sessions.end() ? std::make_pair(first->first, first->second)
				: std::pair<SessionId, std::shared_ptr<ClientNetworkLib::CSession>>{};
		}

		static auto LockSession(ClientNetworkLib::CSession &session)
		{
			return std::unique_lock(session.m_sessionStateLock);
		}

		static int GetBufferedRecvBytes(ClientNetworkLib::CSession &session)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			return session.m_recvBuffer.GetUseSize();
		}

		static int GetRecvBufferCapacity(const ClientNetworkLib::CSession &session)
		{
			return session.m_recvBuffer.GetCapacity();
		}

		static bool IsAccepting(CClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return client.m_isAccepting;
		}

		static LPFN_CONNECTEX GetCachedConnectEx(CClientManager &client)
		{
			std::lock_guard sessionTableLock(client.m_sessionTableLock);
			return client.m_pConnectEx;
		}

		static bool IsConnectPending(ClientNetworkLib::CSession &session)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			if (session.m_state != ClientNetworkLib::CSession::EState::Connecting || session.m_ioCount != 1)
				return false;
			DWORD bytes = 0;
			DWORD flags = 0;
			return !WSAGetOverlappedResult(session.m_socket, &session.m_connectOverlapped, &bytes, FALSE, &flags)
				&& WSAGetLastError() == WSA_IO_INCOMPLETE;
		}

		static void AttachSendTestSocket(ClientNetworkLib::CSession &session, SOCKET socketHandle)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			session.m_socket = socketHandle;
			session.m_state = ClientNetworkLib::CSession::EState::Connected;
		}

		static bool QueueSend(ClientNetworkLib::CSession &session, std::shared_ptr<CSerializableBuffer> spPacket)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			return session.queueSendLocked(std::move(spPacket));
		}

		static bool CompleteSend(ClientNetworkLib::CSession &session, OVERLAPPED *pOverlapped, DWORD bytes, DWORD error)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			if (pOverlapped != &session.m_sendOverlapped || session.m_ioCount != 1)
				return false;
			session.completeLocked(pOverlapped, bytes, error);
			return true;
		}

		static bool FinishSendTestClose(ClientNetworkLib::CSession &session)
		{
			std::lock_guard sessionStateLock(session.m_sessionStateLock);
			session.beginCloseLocked(ERROR_OPERATION_ABORTED);
			return session.finishCloseLocked();
		}
	};
}

namespace
{
	using namespace ClientNetworkLib;
	using namespace NetworkCommonLib;
	constexpr DWORD kConnectTimeoutMilliseconds = 5000;
	constexpr int kDefaultFixtureRecvBufferBytes = 4096;
	static_assert(std::is_same_v<decltype(&CClient::Disconnect), void (CClient::*)() noexcept>);
	static_assert(std::is_same_v<decltype(&CClientManager::Disconnect), bool (CClientManager::*)(SessionId) noexcept>);

	void Check(bool isPassed, const wchar_t *pCase)
	{
		if (!isPassed)
		{
			g_pLogger->WriteLogConsole(ELogLevel::Error, L"FAIL: %ls", pCase);
			fflush(nullptr);
			ExitProcess(1);
		}
	}

	SOCKET BindLoopback(USHORT &outPort)
	{
		SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		int addressBytes = sizeof(address);
		Check(socketHandle != INVALID_SOCKET && bind(socketHandle, reinterpret_cast<sockaddr *>(&address), addressBytes) == 0
			&& getsockname(socketHandle, reinterpret_cast<sockaddr *>(&address), &addressBytes) == 0, L"bind loopback");
		outPort = ntohs(address.sin_port);
		return socketHandle;
	}

	SClientEvent WaitEvent(CClientManager &client, SessionId sessionId)
	{
		const auto deadline = GetTickCount64() + 7000;
		SClientEvent event{};
		while (!client.TryPopEvent(sessionId, event))
		{
			Check(GetTickCount64() < deadline, L"mailbox deadline");
			Sleep(1);
		}
		return event;
	}

	SOCKET AcceptPeer(SOCKET listener)
	{
		fd_set readable;
		FD_ZERO(&readable);
		FD_SET(listener, &readable);
		timeval timeout{ 7, 0 };
		Check(select(0, &readable, nullptr, nullptr, &timeout) == 1, L"accept deadline");
		SOCKET peer = accept(listener, nullptr, nullptr);
		Check(peer != INVALID_SOCKET, L"accept peer");
		DWORD timeoutMilliseconds = 2000;
		Check(setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMilliseconds),
			sizeof(timeoutMilliseconds)) == 0, L"bound fixture receive wait");
		return peer;
	}

	void ExpectOnlyTerminal(CClientManager &client, SessionId sessionId, EClientEvent kind, DWORD error)
	{
		SClientEvent event = WaitEvent(client, sessionId);
		Check(event.m_kind == kind && event.m_error == error, L"expected terminal event and reason");
		Check(!client.TryPopEvent(sessionId, event), L"terminal event was delivered only once");
	}

	void VerifyDecodeAndValidateChecksum()
	{
		using MHLib::security::CEncryption;
		constexpr int kMaxPayloadBytes = static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize) - sizeof(SPacketHeader);
		for (const int randKey : { 0, 127, 255 })
		{
			for (int payloadBytes = 0; payloadBytes <= kMaxPayloadBytes; ++payloadBytes)
			{
				std::vector<char> plain(payloadBytes + 1);
				for (int i = 1; i <= payloadBytes; ++i)
					plain[i] = static_cast<char>(i * 173 + payloadBytes);
				const BYTE checksum = CEncryption::CalCheckSum(plain.data() + 1, payloadBytes);
				for (const bool isValid : { true, false })
				{
					plain[0] = static_cast<char>(isValid ? checksum : checksum ^ 1);
					auto fused = plain;
					CEncryption::Encoding(fused.data(), payloadBytes + 1, randKey);
					auto legacy = fused;
					CEncryption::Decoding(legacy.data(), payloadBytes + 1, randKey);
					const bool legacyValid = CEncryption::CalCheckSum(legacy.data() + 1, payloadBytes) == static_cast<BYTE>(legacy[0]);
					Check(legacyValid == isValid, L"checksum fixture exercises both decisions");
					Check(CEncryption::DecodeAndValidateChecksum(fused.data(), payloadBytes + 1, randKey) == legacyValid,
						L"fused decoding preserves the legacy checksum decision");
					Check(fused == legacy && fused == plain, L"fused decoding preserves every decoded byte");
				}
			}
		}
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"PASS: fused Net decoding and checksum match the legacy path");
	}

	void VerifyRecvRing(EPacketMode mode)
	{
		constexpr BYTE kPacketCode = 0x77;
		constexpr int kPacketBytes = static_cast<int>(CSerializableBuffer::EBufferDefine::PacketMaxSize);
		constexpr int kHeaderBytes = sizeof(SPacketHeader);
		constexpr int kRecvBufferBytes = kPacketBytes * 2;
		CClientManager client;
		Check(!client.Init({1, kPacketBytes - 1}), L"ring rejects capacity below a full frame");
		Check(!client.Init({1, INT_MAX}), L"ring rejects capacity that overflows cursor arithmetic");
		Check(client.Init({1, kRecvBufferBytes, mode}), L"ring fixture initializes the manager with a custom capacity");
		USHORT port = 0;
		SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"ring fixture listens");
		const auto sessionId = client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds);
		Check(sessionId != 0, L"ring fixture submits a connection");
		SOCKET peer = AcceptPeer(listener);
		Check(WaitEvent(client, sessionId).m_kind == EClientEvent::Connected, L"ring fixture connects");
		auto spSession = CClientLifecycleProbe::FindSession(client, sessionId);
		Check(spSession != nullptr, L"ring fixture finds the live session");
		Check(CClientLifecycleProbe::GetRecvBufferCapacity(*spSession) == kRecvBufferBytes, L"session uses configured ring capacity");

		const auto makeFrame = [mode](int packetBytes, BYTE value)
		{
			auto spPacket = CSerializableBuffer::MakeShared();
			Check(spPacket != nullptr, L"allocate ring fixture frame");
			for (int index = kHeaderBytes; index < packetBytes; ++index)
				*spPacket << value;
			PreparePacket(*spPacket, kPacketCode, mode);
			return spPacket;
		};
		const auto sendBytes = [peer](const char *pBytes, int size)
		{
			Check(send(peer, pBytes, size, 0) == size, L"send ring fixture bytes");
		};
		const auto expectFrame = [&](int packetBytes, BYTE expected)
		{
			auto event = WaitEvent(client, sessionId);
			Check(event.m_kind == EClientEvent::Packet && event.m_spPacket->GetDataSize() == packetBytes - kHeaderBytes,
				L"ring fixture delivers a complete frame");
			for (int index = kHeaderBytes; index < packetBytes; ++index)
			{
				BYTE value = 0;
				*event.m_spPacket >> value;
				Check(value == expected, L"ring fixture preserves payload bytes");
			}
		};
		const auto expectPartial = [&](int bytes)
		{
			const auto deadline = GetTickCount64() + 3000;
			while (CClientLifecycleProbe::GetBufferedRecvBytes(*spSession) != bytes)
			{
				Check(GetTickCount64() < deadline, L"ring retains incomplete bytes across receive completions");
				Sleep(1);
			}
			SClientEvent event;
			Check(!client.TryPopEvent(sessionId, event), L"incomplete frame is not published");
		};

		// 사용 가능 용량보다 1바이트 적게 소비해 다음 헤더를 링 끝에 걸친다.
		const auto spFull = makeFrame(kPacketBytes, 42);
		sendBytes(spFull->GetBufferPtr(), kPacketBytes);
		expectFrame(kPacketBytes, 42);
		const auto spAlmostFull = makeFrame(kPacketBytes - 1, 43);
		sendBytes(spAlmostFull->GetBufferPtr(), kPacketBytes - 1);
		expectFrame(kPacketBytes - 1, 43);

		constexpr int kSmallFrameBytes = kHeaderBytes + 3;
		const auto spFirst = makeFrame(kSmallFrameBytes, 21);
		const auto spSecond = makeFrame(kSmallFrameBytes, 22);
		sendBytes(spFirst->GetBufferPtr(), 2);
		expectPartial(2);
		sendBytes(spFirst->GetBufferPtr() + 2, kHeaderBytes - 1);
		expectPartial(kHeaderBytes + 1);
		std::array<char, 2 + kSmallFrameBytes> tail{};
		memcpy(tail.data(), spFirst->GetBufferPtr() + kHeaderBytes + 1, 2);
		memcpy(tail.data() + 2, spSecond->GetBufferPtr(), kSmallFrameBytes);
		sendBytes(tail.data(), static_cast<int>(tail.size()));
		expectFrame(kSmallFrameBytes, 21);
		expectFrame(kSmallFrameBytes, 22);
		if (mode == EPacketMode::Net)
		{
			constexpr BYTE kPayload = 42;
			constexpr int kChecksumOffset = offsetof(SPacketHeader, checkSum);
			const SPacketHeader header{ kPacketCode, 1, 0, static_cast<BYTE>(kPayload + 1) };
			std::array<char, kHeaderBytes + 1> frame{};
			memcpy(frame.data(), &header, kHeaderBytes);
			frame[kHeaderBytes] = kPayload;
			MHLib::security::CEncryption::Encoding(frame.data() + kChecksumOffset, 2, header.randKey);
			sendBytes(frame.data(), static_cast<int>(frame.size()));
			ExpectOnlyTerminal(client, sessionId, EClientEvent::Disconnected, ERROR_INVALID_DATA);
			g_pLogger->WriteLogConsole(ELogLevel::Info, L"PASS: Net checksum rejection does not publish a packet");
		}
		else
		{
			Check(shutdown(peer, SD_SEND) == 0, L"ring fixture sends EOF");
			ExpectOnlyTerminal(client, sessionId, EClientEvent::Disconnected, 0);
		}
		closesocket(peer);
		closesocket(listener);
		client.Stop();
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"PASS: %ls receive ring wrap, split header/body and adjacent frames",
			mode == EPacketMode::Net ? L"Net" : L"Lan");
	}

	class CTickClient : public CClient
	{
	public:
		CTickClient(CClientManager &network, bool rejectsPacket, DWORD expectedError) noexcept
			: CClient(network)
			, m_rejectsPacket(rejectsPacket)
			, m_expectedError(expectedError)
		{
		}

		int m_callbackCount = 0;

	private:
		bool onConnect() noexcept override
		{
			Check(GetCurrentThreadId() == m_appThreadId && m_callbackCount == 0 && IsConnected(), L"Tick dispatches Connected");
			++m_callbackCount;
			return true;
		}

		bool onRecv(CSerializableBuffer &message) noexcept override
		{
			Check(GetCurrentThreadId() == m_appThreadId && m_callbackCount == 1, L"Tick dispatches Packet after Connected");
			BYTE value = 0;
			message >> value;
			Check(value == 42, L"Tick passes the received packet to onRecv");
			++m_callbackCount;
			return !m_rejectsPacket;
		}

		void onRecvRejected() noexcept override
		{
			Check(m_rejectsPacket && GetCurrentThreadId() == m_appThreadId && m_callbackCount == 2,
				L"Tick reports receive rejection synchronously on the app thread");
			++m_callbackCount;
		}

		void onDisconnect(bool wasConnected, DWORD error) noexcept override
		{
			Check(GetCurrentThreadId() == m_appThreadId && m_callbackCount == (m_rejectsPacket ? 3 : 2),
				L"Tick dispatches Disconnected after Packet and any receive rejection");
			Check(wasConnected && error == m_expectedError && !IsActive() && !IsConnected(),
				L"Tick preserves the native terminal reason and clears connection state before onDisconnect");
			++m_callbackCount;
		}

		DWORD m_appThreadId = GetCurrentThreadId();
		bool m_rejectsPacket;
		DWORD m_expectedError;
	};

	void VerifyClientTick(bool rejectsPacket, bool sendsPeerEof)
	{
		constexpr BYTE kPacketCode = 0x77;
		CClientManager network;
		CTickClient client(network, rejectsPacket, sendsPeerEof ? ERROR_SUCCESS : ERROR_OPERATION_ABORTED);
		Check(network.Init({1, kDefaultFixtureRecvBufferBytes}), L"Tick fixture initializes the manager");
		USHORT port = 0;
		SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"Tick fixture listens");
		Check(client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds), L"Tick fixture submits a connection");
		SOCKET peer = AcceptPeer(listener);
		auto spPacket = CSerializableBuffer::MakeShared();
		Check(spPacket != nullptr, L"Tick fixture allocates a packet");
		*spPacket << BYTE{42};
		PreparePacket(*spPacket, kPacketCode);
		const int frameBytes = spPacket->GetFullSize();
		Check(send(peer, spPacket->GetBufferPtr(), frameBytes, 0) == frameBytes, L"Tick peer sends a packet");
		const auto deadline = GetTickCount64() + 7000;
		if (sendsPeerEof)
		{
			Check(shutdown(peer, SD_SEND) == 0, L"Tick peer sends EOF after the packet");
			while (CClientLifecycleProbe::GetFirstSessionEntry(network).second)
			{
				Check(GetTickCount64() < deadline, L"session retires before the app calls Tick");
				Sleep(1);
			}
		}
		Check(client.m_callbackCount == 0 && client.IsActive() && !client.IsConnected(), L"callbacks wait for the app's Tick");
		while (client.IsActive())
		{
			Check(GetTickCount64() < deadline, L"Tick consumes the terminal event");
			client.Tick();
			if (client.IsActive())
				Sleep(1);
		}
		client.Tick();
		Check(client.m_callbackCount == (rejectsPacket ? 4 : 3) && CClientLifecycleProbe::GetMailboxCount(network) == 0,
			L"Tick drains callbacks once and consumes the mailbox");
		closesocket(peer);
		closesocket(listener);
		network.Stop();
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"PASS: Net Tick callback order (reject=%d, peer_eof=%d), app thread and mailbox retirement",
			rejectsPacket, sendsPeerEof);
	}

	void VerifySendBatch(EPacketMode mode)
	{
		constexpr BYTE kPacketCode = 0x77;
		USHORT port = 0;
		const SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"batch listen");
		const SOCKET sender = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		SOCKADDR_IN address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(port);
		Check(sender != INVALID_SOCKET && connect(sender, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0,
			L"connect batch sender");
		const SOCKET peer = AcceptPeer(listener);
		closesocket(listener);
		const HANDLE completionPort = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sender), nullptr, 0, 1);
		Check(completionPort != nullptr, L"batch sender uses an IOCP consumed by the test");
		{
			ClientNetworkLib::CSession session(kPacketCode, std::make_shared<CClientMailbox>(), kDefaultFixtureRecvBufferBytes, mode);
			CClientLifecycleProbe::AttachSendTestSocket(session, sender);
			constexpr DWORD kFrameBytes = sizeof(SPacketHeader) + sizeof(BYTE);
			constexpr BYTE kPacketCount = 4;
			std::array<std::weak_ptr<CSerializableBuffer>, kPacketCount> packets;
			auto enqueuePacket = [&](BYTE index)
			{
				auto spPacket = CSerializableBuffer::MakeShared();
				Check(spPacket != nullptr, L"allocate batch packet");
				*spPacket << index;
				packets[index] = spPacket;
				Check(CClientLifecycleProbe::QueueSend(session, std::move(spPacket)), L"enqueue batch packet");
			};
			for (BYTE index = 0; index < kPacketCount; ++index)
				enqueuePacket(index);
			auto completeSend = [&](DWORD expectedBytes, bool isClosing = false)
			{
				DWORD bytes = 0;
				ULONG_PTR key = 0;
				OVERLAPPED *pOverlapped = nullptr;
				const BOOL isCompleted = GetQueuedCompletionStatus(completionPort, &bytes, &key, &pOverlapped, 7000);
				const DWORD error = isCompleted ? ERROR_SUCCESS : GetLastError();
				Check(pOverlapped != nullptr, L"Send completion was dequeued before deadline");
				Check(isCompleted ? bytes == expectedBytes : isClosing && error == ERROR_OPERATION_ABORTED,
					L"Send completion covers the batch or reports cancellation during close");
				Check(CClientLifecycleProbe::CompleteSend(session, pOverlapped, bytes, error), L"only one Send I/O is in flight");
			};
			completeSend(kFrameBytes);
			Check(packets[0].expired() && !packets[1].expired() && !packets[2].expired() && !packets[3].expired(),
				L"queued batch remains owned until completion is processed");
			completeSend(kFrameBytes * (kPacketCount - 1));
			for (BYTE index = 0; index < kPacketCount; ++index)
			{
				Check(packets[index].expired(), L"batch completion releases each owned buffer");
				std::array<char, kFrameBytes> frame{};
				Check(recv(peer, frame.data(), static_cast<int>(frame.size()), MSG_WAITALL) == static_cast<int>(frame.size()),
					L"peer receives each complete frame");
				SPacketHeader header{};
				memcpy(&header, frame.data(), sizeof(header));
				Check(header.code == kPacketCode && header.len == 1, L"batch preserves each packet header");
				if (mode == EPacketMode::Net)
					MHLib::security::CEncryption::Decoding(frame.data() + 4, header.len + 1, header.randKey);
				else
					Check(header.randKey == 0 && header.checkSum == 0, L"LAN does not write a checksum");
				Check(static_cast<BYTE>(frame.back()) == index, L"batch preserves packet FIFO on the wire");
			}

			for (BYTE index = 0; index < kPacketCount; ++index)
				enqueuePacket(index);
			completeSend(kFrameBytes);
			enqueuePacket(0);
			Check(!CClientLifecycleProbe::FinishSendTestClose(session), L"close waits for the active batch completion");
			for (const auto &wpPacket : packets)
				Check(!wpPacket.expired(), L"close retains active batch and queued owners while completion is unread");
			completeSend(kFrameBytes * (kPacketCount - 1), true);
			Check(CClientLifecycleProbe::FinishSendTestClose(session), L"close finishes after the active batch completion");
			for (const auto &wpPacket : packets)
				Check(wpPacket.expired(), L"close releases active batch and queued owners after completion");
		}
		CloseHandle(completionPort);
		closesocket(peer);
		g_pLogger->WriteLogConsole(ELogLevel::Info, L"PASS: %ls batch FIFO and whole completion ownership",
			mode == EPacketMode::Net ? L"Net" : L"Lan");
	}

	void CheckConnectPending(CClientManager &client, SessionId sessionId, const wchar_t *pCase)
	{
		auto spSession = CClientLifecycleProbe::FindSession(client, sessionId);
		Check(spSession != nullptr && CClientLifecycleProbe::IsConnectPending(*spSession), pCase);
	}

	BOOL PASCAL RejectConnectEx(SOCKET, const sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED)
	{
		WSASetLastError(WSAEINVAL);
		return FALSE;
	}

	void VerifyLifecycle(DWORD workers)
	{
		constexpr BYTE kPacketCode = 0x77;
		CClientManager client;
		Check(!client.Init({0, kDefaultFixtureRecvBufferBytes}) && CClientLifecycleProbe::GetCachedConnectEx(client) == nullptr,
			L"invalid worker count leaves no cache");
		Check(client.Init({workers, kDefaultFixtureRecvBufferBytes}), L"manager initialization succeeds");
		const auto pConnectEx = CClientLifecycleProbe::GetCachedConnectEx(client);
		Check(pConnectEx != nullptr, L"initialization caches ConnectEx before any connection");
		Check(!client.Init({workers, kDefaultFixtureRecvBufferBytes}) && CClientLifecycleProbe::GetCachedConnectEx(client) == pConnectEx,
			L"duplicate initialization preserves cache");
		USHORT port = 0;
		SOCKET listener = BindLoopback(port);
		Check(listen(listener, SOMAXCONN) == 0, L"listen");
		Check(CClientLifecycleProbe::ReplaceConnectEx(client, RejectConnectEx) == pConnectEx, L"install immediate failure fixture");
		Check(client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds) == 0,
			L"immediate ConnectEx failure returns no ID");
		Check(CClientLifecycleProbe::ReplaceConnectEx(client, pConnectEx) == RejectConnectEx, L"restore real ConnectEx");
		Check(!CClientLifecycleProbe::GetFirstSessionEntry(client).second && CClientLifecycleProbe::GetMailboxCount(client) == 0,
			L"immediate failure rolls back both tables");
		const auto first = client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds);
		const auto second = client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds);
		Check(first != 0 && second != 0 && first != second, L"two accepted attempts with distinct IDs");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) == pConnectEx, L"multiple attempts share the initialized cache");
		SOCKET firstPeer = AcceptPeer(listener);
		SOCKET secondPeer = AcceptPeer(listener);
		Check(WaitEvent(client, first).m_kind == EClientEvent::Connected && WaitEvent(client, second).m_kind == EClientEvent::Connected,
			L"both attempts complete");
		Check(client.Disconnect(first), L"close one session");
		const auto firstRetireDeadline = GetTickCount64() + 3000;
		while (CClientLifecycleProbe::FindSession(client, first))
		{
			Check(GetTickCount64() < firstRetireDeadline, L"first session retires without event polling");
			Sleep(1);
		}
		Check(!client.Disconnect(first), L"old ID cannot disconnect");

		const auto next = client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds);
		Check(next != 0 && next != first && next != second, L"reconnect returns a new ID");
		SOCKET nextPeer = AcceptPeer(listener);

		auto spMessage = CSerializableBuffer::MakeShared();
		Check(spMessage != nullptr, L"allocate test packet");
		*spMessage << static_cast<BYTE>(42);
		PreparePacket(*spMessage, kPacketCode);
		const int frameBytes = spMessage->GetFullSize();
		Check(send(nextPeer, spMessage->GetBufferPtr(), frameBytes, 0) == frameBytes, L"peer sends a frame before app polls");
		Check(WaitEvent(client, next).m_kind == EClientEvent::Connected, L"Connected precedes the first packet");
		SClientEvent packet = WaitEvent(client, next);
		BYTE value = 0;
		Check(packet.m_kind == EClientEvent::Packet && packet.m_spPacket != nullptr && packet.m_spPacket->IsHeapAllocated(),
			L"new mailbox owns its first heap packet");
		auto spReceived = packet.m_spPacket;
		packet = {};
		*spReceived >> value;
		Check(value == 42 && !client.TryPopEvent(second, packet), L"receive stays in its own mailbox");
		ExpectOnlyTerminal(client, first, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		Check(CClientLifecycleProbe::GetMailboxCount(client) == 2,
			L"terminal consumption removes only the retired mailbox");

		std::weak_ptr wpRejected = spMessage;
		Check(!client.SendPacket(first, std::move(spMessage)) && wpRejected.expired(), L"stale ID does not retain a shared packet");
		auto makePacket = []
		{
			auto spPacket = CSerializableBuffer::MakeShared();
			Check(spPacket != nullptr, L"allocate a temporary send packet");
			*spPacket << BYTE{42};
			return spPacket;
		};
		Check(!client.SendPacket(second, nullptr) && client.SendPacket(second, makePacket()), L"survivor sends a temporary owner");
		SPacketHeader header{};
		const auto deadline = GetTickCount64() + 7000;
		std::array<WSAPOLLFD, 2> peers{{ { firstPeer, POLLRDNORM, 0 }, { secondPeer, POLLRDNORM, 0 } }};
		bool hasPacket = false;
		while (!hasPacket && GetTickCount64() < deadline)
		{
			Check(WSAPoll(peers.data(), static_cast<ULONG>(peers.size()), 50) != SOCKET_ERROR, L"poll survivor packet");
			for (auto &peer : peers)
			{
				if (peer.fd == INVALID_SOCKET || !(peer.revents & (POLLRDNORM | POLLHUP | POLLERR)))
					continue;
				int received = recv(peer.fd, reinterpret_cast<char *>(&header), sizeof(header), MSG_WAITALL);
				if (received == 0)
					peer.fd = INVALID_SOCKET;
				else
					hasPacket = received == sizeof(header) && header.code == kPacketCode && header.len == 1;
			}
		}
		Check(hasPacket, L"survivor sends real framed bytes");

		USHORT refusedPort = 0;
		SOCKET nonListener = BindLoopback(refusedPort);
		const auto refusedId = client.Connect("127.0.0.1", refusedPort, kPacketCode, kConnectTimeoutMilliseconds);
		Check(refusedId != 0, L"refused attempt accepted for async I/O");
		SClientEvent refused = WaitEvent(client, refusedId);
		Check(refused.m_kind == EClientEvent::ConnectFailed
			&& (refused.m_error == ERROR_CONNECTION_REFUSED || refused.m_error == WSAECONNREFUSED), L"asynchronous refusal");
		Check(!client.TryPopEvent(refusedId, refused), L"refusal reported once");

		const auto deadlineId = client.Connect("127.0.0.1", port, kPacketCode, 0);
		Check(deadlineId != 0, L"accept a ConnectEx with an immediate deadline");
		ExpectOnlyTerminal(client, deadlineId, EClientEvent::ConnectFailed, WSAETIMEDOUT);

		// accept하지 않아 대기열을 채우면 후속 연결을 Connecting 상태로 유지할 수 있다.
		USHORT saturatedPort = 0;
		SOCKET saturatedListener = BindLoopback(saturatedPort);
		Check(listen(saturatedListener, 0) == 0, L"listen with a minimal backlog");
		const auto filler = client.Connect("127.0.0.1", saturatedPort, kPacketCode, kConnectTimeoutMilliseconds);
		Check(filler != 0 && WaitEvent(client, filler).m_kind == EClientEvent::Connected, L"fill listener backlog");

		const auto timedOut = client.Connect("127.0.0.1", saturatedPort, kPacketCode, 1000);
		CheckConnectPending(client, timedOut, L"timeout fixture has outstanding ConnectEx");
		Check(timedOut != 0 && !client.SendPacket(timedOut, makePacket()), L"send rejected while Connecting");
		ExpectOnlyTerminal(client, timedOut, EClientEvent::ConnectFailed, WSAETIMEDOUT);

		const auto canceled = client.Connect("127.0.0.1", saturatedPort, kPacketCode, kConnectTimeoutMilliseconds);
		CheckConnectPending(client, canceled, L"ConnectEx still outstanding before Disconnect");
		Check(client.Disconnect(canceled), L"cancel a pending connection");
		ExpectOnlyTerminal(client, canceled, EClientEvent::ConnectFailed, ERROR_OPERATION_ABORTED);
		const auto retireDeadline = GetTickCount64() + 3000;
		while (CClientLifecycleProbe::FindSession(client, canceled) || CClientLifecycleProbe::FindSession(client, timedOut))
		{
			Check(GetTickCount64() < retireDeadline, L"previous failed connections are retired before the Stop fixture");
			Sleep(1);
		}

		const auto stopPending = client.Connect("127.0.0.1", saturatedPort, kPacketCode, kConnectTimeoutMilliseconds);
		CheckConnectPending(client, stopPending, L"ConnectEx still outstanding before Stop");
		auto [blockerId, spStopBlocker] = CClientLifecycleProbe::GetFirstSessionEntry(client);
		const auto sendTarget = blockerId == next ? second : next;
		Check(spStopBlocker != nullptr && blockerId != sendTarget, L"Stop has a distinct blocker and send target");
		auto spRejected = CSerializableBuffer::MakeShared();
		Check(spRejected != nullptr, L"allocate unframed shutdown packet");
		*spRejected << static_cast<BYTE>(73);
		std::array<char, sizeof(SPacketHeader) + 1> originalFrame{};
		memset(spRejected->GetBufferPtr(), 0, spRejected->GetHeaderSize());
		memcpy(originalFrame.data(), spRejected->GetBufferPtr(), originalFrame.size());
		{
			// 스냅샷의 첫 세션에서 Stop을 멈춰 다른 세션이 닫히기 전에 송신 접수 경계를 확인한다.
			auto blockerSessionStateLock = CClientLifecycleProbe::LockSession(*spStopBlocker);
			std::thread stopper([&client] { client.Stop(); });
			const auto stopDeadline = GetTickCount64() + 3000;
			while (CClientLifecycleProbe::IsAccepting(client))
			{
				Check(GetTickCount64() < stopDeadline, L"Stop closes admission while a session lock is held");
				Sleep(1);
			}
			if (blockerId != stopPending)
				CheckConnectPending(client, stopPending, L"ConnectEx still outstanding after Stop closes admission");
			Check(!client.SendPacket(sendTarget, spRejected), L"Stop must reject send to a not-yet-closed session");
			Check(memcmp(originalFrame.data(), spRejected->GetBufferPtr(), originalFrame.size()) == 0,
				L"shutdown rejection leaves caller packet unframed");
			blockerSessionStateLock.unlock();
			stopper.join();
		}
		spStopBlocker.reset();
		ExpectOnlyTerminal(client, stopPending, EClientEvent::ConnectFailed, ERROR_OPERATION_ABORTED);
		ExpectOnlyTerminal(client, filler, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		ExpectOnlyTerminal(client, next, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		std::weak_ptr wpReceived = spReceived;
		Check(spReceived->GetDataSize() == 0 && spReceived.use_count() == 1, L"Net packet outlives mailbox and Stop");
		spReceived.reset();
		Check(wpReceived.expired(), L"Net receive packet releases its last owner");
		Check(!client.SendPacket(second, makePacket()) && !client.Disconnect(next), L"no sessions remain after Stop");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) == nullptr, L"Stop clears the ConnectEx cache");
		Check(client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds) == 0, L"Stop rejects new attempts");
		Check(client.Init({workers, kDefaultFixtureRecvBufferBytes}), L"manager restarts after draining pending receives");
		Check(CClientLifecycleProbe::GetCachedConnectEx(client) != nullptr, L"reinitialization restores the ConnectEx cache");
		const auto restarted = client.Connect("127.0.0.1", port, kPacketCode, kConnectTimeoutMilliseconds);
		Check(restarted > stopPending && WaitEvent(client, restarted).m_kind == EClientEvent::Connected,
			L"IDs remain fresh across Stop/Init");
		ExpectOnlyTerminal(client, second, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		Check(client.SendPacket(restarted, makePacket()), L"restarted session accepts a send");
		client.Stop();
		Check(CClientLifecycleProbe::GetMailboxCount(client) == 1, L"Stop returns with the terminal event still unread");
		ExpectOnlyTerminal(client, restarted, EClientEvent::Disconnected, ERROR_OPERATION_ABORTED);
		Check(CClientLifecycleProbe::GetMailboxCount(client) == 0, L"reading the final terminal event removes the final mailbox");
		closesocket(firstPeer);
		closesocket(secondPeer);
		closesocket(nextPeer);
		closesocket(nonListener);
		closesocket(saturatedListener);
		closesocket(listener);
		g_pLogger->WriteLogConsole(ELogLevel::Info,
			L"PASS: %lu worker(s): simultaneous sessions, mailbox order/isolation, reconnect, refusal, timeout, cancel, Stop", workers);
	}
}

int main(void)
{
	g_pLogger = CLogger::GetInstance();
	VerifyDecodeAndValidateChecksum();
	WSADATA wsaData{};
	Check(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0, L"fixture Winsock startup");
	VerifyLifecycle(1);
	VerifyLifecycle(2);
	VerifyClientTick(false, true);
	VerifyClientTick(true, true);
	VerifyClientTick(true, false);
	for (const auto mode : { EPacketMode::Net, EPacketMode::Lan })
	{
		VerifyRecvRing(mode);
		VerifySendBatch(mode);
	}
	WSACleanup();
	fflush(nullptr);
	// 기존 정적 풀 소멸은 이 테스트 범위가 아니다.
	ExitProcess(0);
}
