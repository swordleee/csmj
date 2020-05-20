#include "Entry.h"

#include "../Game.h"
#include "../Dispatch.h"

using namespace evwork;

SEntry g_entry;

int main(int argc, char* argv[])
{
	//-------------------------------------------------------------------------
	// libevwork初使化

	signal(SIGPIPE, SIG_IGN);

	CSyslogReport LG;
	CEVLoop LP;
	CConnManager CM;
	CWriter WR;

	CEnv::setLogger(&LG);
	CEnv::setEVLoop(&LP);
	CEnv::setLinkEvent(&CM);
	CEnv::setConnManager(&CM);
	CEnv::setWriter(&WR);

	LP.init();

	//-------------------------------------------------------------------------
	// 应用程序初使化

	CEnv::getEVParam().uConnTimeout = 120;

	CJsonData __DE;
	__DE.setPacketLimit(16*1024); // 设置最大请求包长度 
	CEnv::setDataEvent(&__DE);

	CJsonMFC __MFC;
	__DE.setAppContext(&__MFC);

	CConfig::loadConf();
	CDB::initDB();

	g_entry.pGame = CGame::createGame();
	CDispatch __DP;

	CM.addLE(g_entry.pGame);

	__MFC.addEntry(IGame::getFormEntries(), g_entry.pGame);
	__MFC.addEntry(CDispatch::getFormEntries(), &__DP);

	// 启动侦听
	CListenConn __LS(g_entry.conf["game"]["port"].asInt(), g_entry.conf["game"]["host"].asString());

	printf("listen on %u... \n", g_entry.conf["game"]["port"].asInt());
	LOG(Info, "listen on %u... ", g_entry.conf["game"]["port"].asInt());

	//-------------------------------------------------------------------------
	// 启动事件循环

	LP.runLoop();

	return 0;
}
