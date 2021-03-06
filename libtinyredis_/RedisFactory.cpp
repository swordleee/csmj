#include "RedisFactory.h"

#include "ConsistentHash.h"

using namespace tinyredis;

CRedisMap::CRedisMap(const VEC_REDIS_PARAM_t& vecParam)
{
	for (VEC_REDIS_PARAM_t::const_iterator c_iter = vecParam.begin(); c_iter != vecParam.end(); ++c_iter)
	{
		CRedisClient* pNew = new CRedisClient(c_iter->m_strIp, c_iter->m_uPort16, c_iter->m_strPass, c_iter->m_uMiniSeconds);
		m_mapIdxRedis.insert(std::make_pair(m_mapIdxRedis.size(), pNew));
	}
}

CRedisMap::~CRedisMap()
{
	for (MAP_IDX_REDIS_t::iterator iter = m_mapIdxRedis.begin(); iter != m_mapIdxRedis.end(); ++iter)
	{
		delete iter->second;
	}

	m_mapIdxRedis.clear();
}

CRedisClient* CRedisMap::getRedis(uint32_t uKey)
{
	uint32_t uIdx = __toIndex(uKey);

	MAP_IDX_REDIS_t::iterator iter = m_mapIdxRedis.find(uIdx);

	if (iter == m_mapIdxRedis.end())
		return NULL;

	return iter->second;
}

CRedisClient* CRedisMap::getRedis(const std::string& strKey)
{
	uint32_t uKey = __toKey(strKey);

	return getRedis(uKey);
}

uint32_t CRedisMap::__toIndex(uint32_t uKey)
{
	return (uKey % m_mapIdxRedis.size());
}

uint32_t CRedisMap::__toKey(const std::string& strKey)
{
	return CHashFunction::fnvHash(strKey.data(), strKey.size());
}


CRedisFactory::CRedisFactory()
: m_pRedisMap(NULL)
{
}

CRedisFactory::~CRedisFactory()
{
	__cleanRedisMap();
}

void CRedisFactory::addRedis(const std::string& strIp, uint16_t uPort16, const std::string& strPass, uint32_t uMiniSeconds)
{
	m_vecRedisParam.push_back( SRedisParam(strIp, uPort16, strPass, uMiniSeconds) );
}

CRedisClient* CRedisFactory::getRedis(uint32_t uKey)
{
	return __getRedisMap()->getRedis(uKey);
}

CRedisClient* CRedisFactory::getRedis(const std::string& strKey)
{
	return __getRedisMap()->getRedis(strKey);
}

CRedisMap* CRedisFactory::__getRedisMap()
{
	if (m_pRedisMap == NULL)
		m_pRedisMap = new CRedisMap(m_vecRedisParam);

	return m_pRedisMap;
}

void CRedisFactory::__cleanRedisMap()
{
	if (m_pRedisMap)
	{
		delete m_pRedisMap;
		m_pRedisMap = NULL;
	}
}
