#pragma once

namespace Chatting
{
	class CDummyRunner final
	{
	public:
		[[nodiscard]]
		int Run(std::chrono::steady_clock::time_point processStart);

	private:
		void 			runClients(const ClientNetworkLib::SClientNetworkSettings &networkSettings, const std::wstring &runMarker,
			std::chrono::steady_clock::time_point deadline);

		[[nodiscard]]
		bool 			saveSummary(const std::filesystem::path &path, const std::string &runId,
			const ClientNetworkLib::SClientNetworkSettings &networkSettings, long long startUnixMilliseconds, int exitCode) const;

		struct SAppWorkerResult
		{
			DWORD m_threadId = 0;
			size_t m_clientCount = 0;
			size_t m_firstReady = 0;
			UINT64 m_terminals = 0;
			UINT64 m_actionsCompleted = 0;
		};

		std::vector<SAppWorkerResult> 	m_appWorkers;
		SDummyConfig 					m_config;
		SDummyRunStats 					m_stats;
	};
}
