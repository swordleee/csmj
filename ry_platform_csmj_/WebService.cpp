#include "WebService.h"

#include "base/Entry.h"

using namespace evwork;
using namespace tinyredis;

#define WEBSERVICE_LIST_NAME	"webservice_list"

bool CWebService::notifyWebService(const std::string& packet)
{
	CRedisClient* pRedis = g_entry.pub_db.getRedis(0);

	if (pRedis == NULL)
	{
		LOG(Error, "[CWebService::%s] #error# no pub_db redis!", __FUNCTION__);
		return false;
	}

	CResult result(true);
	result = pRedis->command("LPUSH %s %b", WEBSERVICE_LIST_NAME, packet.data(), packet.size());

	if (!result)
	{
		LOG(Error, "[CWebService::%s] #error# push list:[%s] packet:[%s] failed", __FUNCTION__, WEBSERVICE_LIST_NAME, packet.data());
		return false;
	}

	return true;
}
