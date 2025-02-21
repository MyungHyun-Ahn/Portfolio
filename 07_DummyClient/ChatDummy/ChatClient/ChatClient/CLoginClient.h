#pragma once

namespace LOGIN_CLIENT
{
	class CUser
	{
	public:
		friend class CLoginClient;

		void Init(const INT64 accountNo, const WCHAR *ID, const WCHAR *Nickname);
		void Clear() noexcept;


	public:
		inline static CUser *Alloc() noexcept
		{
			// �Ҵ��ϰ� �ʱ�ȭ�ؼ� ��ȯ
			CUser *pUser = s_sUserPool.Alloc();
			pUser->Clear();
			return pUser;
		}

		inline static void Free(CUser *delUser) noexcept
		{
			s_sUserPool.Free(delUser);
		}

	private:
		INT64 m_iAccountNo;
		WCHAR m_ID[20];
		WCHAR m_Nickname[20];
		inline static CSingleMemoryPool<CUser> s_sUserPool = CSingleMemoryPool<CUser>();
	};

	class CLoginClient :
		public NETWORK_CLIENT::CNetClient
	{
	public:
		CLoginClient() noexcept;

		void LoginPacketReq(const UINT64 sessionID) noexcept;

		// CNetClient��(��) ���� ��ӵ�
		void OnConnect(const UINT64 sessionID) noexcept override;
		void OnDisconnect(const UINT64 sessionID) noexcept override;
		void OnRecv(const UINT64 sessionID, CSerializableBufferView<FALSE> *message) noexcept override;
		void RegisterContentTimerEvent() noexcept override;

	private:
		std::unordered_map<UINT64, CUser *>		m_umapUsers;
		SRWLOCK									m_srwLockUserMap;

	};

	extern CLoginClient *g_LoginClient;
}

