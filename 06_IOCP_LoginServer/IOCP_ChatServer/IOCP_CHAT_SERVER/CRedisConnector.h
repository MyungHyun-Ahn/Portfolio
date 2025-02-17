#pragma once
class CRedisConnector
{
public:
	CRedisConnector()
	{
		m_redisClient.connect(REDIS_SETTING::IP, REDIS_SETTING::PORT);
	}

	~CRedisConnector()
	{
		m_redisClient.disconnect();
	}

	void Set(const INT64 key, const char *value) noexcept
	{
		m_redisClient.set(std::to_string(key), value);
		m_redisClient.sync_commit();
	}

	void Get(const INT64 key, char *value, int valueLen) noexcept
	{
		auto reply = m_redisClient.get(std::to_string(key));
		m_redisClient.sync_commit();
		auto result = reply.get();
		if (result.is_string())
		{
			memcpy_s(value, valueLen, result.as_string().c_str(), valueLen);
		}
		else
		{
			g_Logger->WriteLog(L"SYSTEM", L"Redis", LOG_LEVEL::ERR, L"Redis get failed");
			throw std::runtime_error("Redis에서 값을 가져올 수 없습니다.");
		}
	}

	void Del(const INT64 key) noexcept
	{
		m_redisClient.del({ std::to_string(key) });
		m_redisClient.sync_commit();
	}

	inline static CRedisConnector *GetRedisConnector() noexcept
	{
		thread_local CRedisConnector redisConnector;
		return &redisConnector;
	}



private:
	cpp_redis::client m_redisClient;
};

