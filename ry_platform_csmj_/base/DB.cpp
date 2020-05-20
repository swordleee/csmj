#include "DB.h"

#include "Entry.h"

void CDB::initDB()
{
	g_entry.conf_db.addRedis(g_entry.conf["conf-db"]["host"].asString(), g_entry.conf["conf-db"]["port"].asInt(), g_entry.conf["conf-db"]["pass"].asString(), 1000);

	for (int i = 0; i < (int)g_entry.conf["main-db"].size(); i++) 
	{
		g_entry.main_db.addRedis(g_entry.conf["main-db"][i]["host"].asString(), g_entry.conf["main-db"][i]["port"].asInt(), g_entry.conf["main-db"][i]["pass"].asString(), 1000);
	}

	g_entry.log_db.addRedis(g_entry.conf["eventlog-db"]["host"].asString(), g_entry.conf["eventlog-db"]["port"].asInt(), g_entry.conf["eventlog-db"]["pass"].asString(), 1000);

	for (int i = 0; i < (int)g_entry.conf["pub-db"].size(); i++) 
	{
		g_entry.pub_db.addRedis(g_entry.conf["pub-db"][i]["host"].asString(), g_entry.conf["pub-db"][i]["port"].asInt(), g_entry.conf["pub-db"][i]["pass"].asString(), 1000);
	}

	g_entry.room_db.addRedis(g_entry.conf["room-db"]["host"].asString(), g_entry.conf["room-db"]["port"].asInt(), g_entry.conf["room-db"]["pass"].asString(), 1000);
}
