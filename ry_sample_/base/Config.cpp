#include "Config.h"

#include "Entry.h"

#include <fstream>
#include <json/json.h>

using namespace evwork;

#define DEF_CONFIG_FILE		"./ry_sample.conf"

void CConfig::loadConf()
{
	std::ifstream in(DEF_CONFIG_FILE, std::ifstream::binary); 
	if (!in)
	{
		LOG(Error, "not find config file:[%s]", DEF_CONFIG_FILE);
		printf("not find config file:[%s] \n", DEF_CONFIG_FILE);
		exit (-1);
	}

	Json::Reader reader;
	if (!reader.parse(in, g_entry.conf))
	{
		LOG(Error, "config file:[%s] format error!", DEF_CONFIG_FILE);
		printf("config file:[%s] format error! \n", DEF_CONFIG_FILE);
		exit (-1);
	}
}
