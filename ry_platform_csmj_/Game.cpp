#include "Game.h"

#include "base/Entry.h"

#include "Protocol.h"
#include "Player.h"

using namespace evwork;
using namespace tinyredis;

IGame* CGame::createGame()
{
	return new CGame();
}

CGame::CGame()
{
	// 装载私有化数据
	__initLocal();

	init();
}
CGame::~CGame()
{
	// 销毁私有化数据
	__destroyLocal();
}

bool CGame::init()
{
	return true;
}

bool CGame::__loginCheck(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	// 暂时跳过密码检查
	//return true;

	Json::Value &val = packet.tojson();

	int uid = -1, zid = 0, vid = 0;
	std::string skey = "";

	try
	{
		uid = val.get("uid",-1).asInt();
		skey = val.get("skey", "").asString();
		zid = val.get("zid", 0).asInt();
		vid = val.get("vid", 0).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CGame::%s] #error# uid:[%d] format invalid!", __FUNCTION__, uid);
		return false;
	}

	CRedisClient* pRedis = g_entry.main_db.getRedis(uid);
	
	CResult result(true);
	result = pRedis->command("hget hu:%d skey", uid);

	std::string strRet;
	if (result.get())
	{
		result.getString(strRet);
	}
	
	// 登录认证失败，发送错误码
	if (!result || skey != strRet)
	{
		if (!result)
		{
			LOG(Error, "[CGame::%s] #error# hget hu:%d skey failed", __FUNCTION__, uid);
		}
		else
		{
			LOG(Error, "[CGame::%s] #error# uid:%d auth failed", __FUNCTION__, uid);
		}

		//g_entry.pGame->sendCode(pConn, CLIENT_LOGIN_REQ, CODE_LOGIN_AUTH);
		// => 
		// 改用旧的兼容性协议
		{
			Jpacket packet;
			packet.val["cmd"] = SERVER_COMPATIBLE_LOGIN_UC;
			packet.val["code"] = 506;
			packet.val["type"] = 1; // 验证失败
			packet.end();

			g_entry.pGame->sendPacket(pConn, packet.tostring()); 
		}

		return false;
	}

	//// 检查zid
	//if (zid == 0 && uid > 10000)
	//{
	//	CResult result(true);
	//	result = pRedis->command("hget hu:%d zid", uid);

	//	std::string strRet;
	//	if (result.get() && result.isString())
	//	{
	//		result.getString(strRet);
	//	}

	//	int currentzid = atoi(strRet.c_str());
	//	int myzid = g_entry.conf["tables"]["zid"].asInt();
	//	if (currentzid > 0 && myzid > 0 && currentzid != myzid)
	//	{
	//		LOG(Error, "[CGame::%s] #error# uid:%d current zid:%d != myzid:%d", __FUNCTION__, uid, currentzid, myzid);

	//		// 重新引导到正确的zid
	//		Jpacket packet;
	//		packet.val["cmd"] = 4002;
	//		packet.val["code"] = 506;
	//		packet.val["zid"] = currentzid;
	//		packet.end();

	//		g_entry.pGame->sendPacket(pConn, packet.tostring()); 
	//		return false;
	//	}
	//}

	return true;
}

IPlayer* CGame::__createPlayer(int uid, evwork::IConn* pConn)
{
	CPlayer* pPlayer = new CPlayer(uid, pConn);
	if (!pPlayer->init())
	{
		delete pPlayer;

		g_entry.pGame->sendCode(pConn, CLIENT_LOGIN_REQ, CODE_LOGIN_INIT);
		return NULL;
	}

	return pPlayer;
}

void CGame::__destroyPlayer(IPlayer* pPlayer)
{
	delete pPlayer;
}

CTable* CGame::getTable(int tid)
{
	MAP_TID_TABLE_t::iterator iter = m_mapTidTable.find(tid);
	if (iter == m_mapTidTable.end())
	{
		CTable* pTable = new CTable(tid);
		if (!pTable->init())
		{
			LOG(Error, "[CGame::%s] #error# load table:[%d] failed", __FUNCTION__, tid);

			delete pTable;
			return NULL;
		}

		LOG(Info, "[CGame::%s] load table:[%d] success", __FUNCTION__, tid);

		m_mapTidTable[tid] = pTable;
		return pTable;
	}

	return iter->second;
}

void CGame::coolTable(int tid)
{
	MAP_TID_TABLE_t::iterator iter = m_mapTidTable.find(tid);
	if (iter == m_mapTidTable.end())
	{
		LOG(Error, "[CGame::%s] #error# cool table:[%d], not find, bug!!!", __FUNCTION__, tid);
		return;
	}

	delete iter->second;
	m_mapTidTable.erase(iter);
}

// 初使化本地数据
bool CGame::__initLocal()
{
	//// 创建统计模块
	//g_entry.setTagPtr("statics", new CStatics());

	m_evTimerShowTid.data = this;
	ev_timer_init(&m_evTimerShowTid, CGame::__cbTimerShowTid, 10, 10);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerShowTid);

	return true;
}

// 销毁本地数据
void CGame::__destroyLocal()
{
	//// 销毁统计模块
	//delete (CStatics*)g_entry.getTagPtr("statics");

	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerShowTid);
}

void CGame::__showActiveTid()
{
	std::stringstream ssTid;

	for (MAP_TID_TABLE_t::iterator iter = m_mapTidTable.begin(); iter != m_mapTidTable.end(); ++iter)
	{
		ssTid << iter->first << ",";
	}

	LOG(Info, "[CGame::%s] active count:[%d] tids:[%s]", __FUNCTION__, m_mapTidTable.size(), ssTid.str().c_str());
}

void CGame::__cbTimerShowTid(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CGame* pThis = (CGame*)w->data;

	pThis->__showActiveTid();
}
