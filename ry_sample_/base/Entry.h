#pragma once 

#include "libevwork/EVWork.h"
#include "libevwork/JsonData.h"
#include "libevwork/JsonMFC.h"

#include "libtinyredis/RedisFactory.h"

#include "Config.h"
#include "DB.h"
#include "EventLog.h"
#include "IGame.h"

struct SEntry
{
	Json::Value conf;

	tinyredis::CRedisFactory main_db;
	tinyredis::CRedisFactory group_db;
	tinyredis::CRedisFactory pub_db;
	tinyredis::CRedisFactory log_db;
	tinyredis::CRedisFactory conf_db;

	CEventLog eventlog;

	IGame* pGame;

	std::map<std::string, void*> mapTagPtr;

	void setTagPtr(const std::string& strTag, void* pPtr)
	{
		mapTagPtr[strTag] = pPtr;
	}
	void* getTagPtr(const std::string& strTag)
	{
		std::map<std::string, void*>::iterator iter = mapTagPtr.find(strTag);
		if (iter != mapTagPtr.end())
			return iter->second;
		return NULL;
	}
};

extern SEntry g_entry;
