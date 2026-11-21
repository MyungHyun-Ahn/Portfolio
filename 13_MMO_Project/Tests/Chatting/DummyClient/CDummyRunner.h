#pragma once

namespace Chatting
{
	class CDummyRunner final
	{
	public:
		[[nodiscard]]
		int Run();

	private:
		[[nodiscard]]
		const char 		*loadConfig(const std::filesystem::path &path);

		void 			runClients(const std::wstring &runMarker);

		[[nodiscard]]
		bool 			saveSummary(const std::filesystem::path &path, const std::string &runId, long long startUnixMilliseconds, int exitCode) const;

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
