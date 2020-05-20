#include "Game.h"

#include "base/Entry.h"

#include "Protocol.h"
#include "Player.h"

#include <assert.h>

using namespace evwork;
using namespace tinyredis;

// 创建Game对象
IGame* CGame::createGame()
{
	return new CGame();
}

CGame::CGame()
{
	init();
}
CGame::~CGame()
{
}

// 游戏初使化
bool CGame::init()
{
	for (int i = g_entry.conf["tables"]["begin"].asInt(); i < g_entry.conf["tables"]["end"].asInt(); i++)
	{
		CTable* pTable = new CTable(i);
		if (!pTable->init())
		{
			LOG(Error, "[CGame::%s] #error# init table:[%d] failed", __FUNCTION__, i);
			exit(-1);
		}

		m_mapTidTable[i] = pTable;

		m_mapTidNumber[i] = 0;
		m_mapNumberTids[0].insert(i);
	}

	return true;
}

// 登录认证
bool CGame::__loginCheck(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	// 暂时跳过密码检查
	//return true;

	Json::Value &val = packet.tojson();

	int uid = val.get("uid",-1).asInt();
	std::string skey = val.get("skey", "").asString();
	int zid = val.get("zid", 0).asInt();
	//int vid = val.get("vid", 0).asInt();

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

		g_entry.pGame->sendCode(pConn, CLIENT_LOGIN_REQ, CODE_LOGIN_AUTH);
		return false;
	}

	// 检查zid
	if (zid == 0 && uid > 10000)
	{
		CResult result(true);
		result = pRedis->command("hget hu:%d zid", uid);

		std::string strRet;
		if (result.get() && result.isString())
		{
			result.getString(strRet);
		}

		int currentzid = atoi(strRet.c_str());
		int myzid = g_entry.conf["tables"]["zid"].asInt();
		if (currentzid > 0 && myzid > 0 && currentzid != myzid)
		{
			LOG(Error, "[CGame::%s] #error# uid:%d current zid:%d != myzid:%d", __FUNCTION__, uid, currentzid, myzid);

			// 重新引导到正确的zid
			Jpacket packet;
			packet.val["cmd"] = 4002;
			packet.val["code"] = 506;
			packet.val["zid"] = currentzid;
			packet.end();

			g_entry.pGame->sendPacket(pConn, packet.tostring()); 
			return false;
		}
	}

	return true;
}

// 创建新玩家
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

// 销毁玩家
void CGame::__destroyPlayer(IPlayer* pPlayer)
{
	delete pPlayer;
}

// 选桌子，tid为-1，表示随机选
CTable* CGame::getTable(int tid)
{
	// 随机找桌子
	if (tid == -1)
	{
		for (MAP_NUMBER_SETTID_t::reverse_iterator r_iter = m_mapNumberTids.rbegin(); r_iter != m_mapNumberTids.rend(); ++r_iter)
		{
			std::set<int>& setTid = r_iter->second;

			for (std::set<int>::iterator iterTid = setTid.begin(); iterTid != setTid.end(); ++iterTid)
			{
				MAP_TID_TABLE_t::iterator iterTable = m_mapTidTable.find(*iterTid);
				if (iterTable == m_mapTidTable.end())
				{
					LOG(Error, "[CGame::%s] #error# tid:[%d] not find, bug!", __FUNCTION__, *iterTid);
					continue;
				}
				
				// 找到空闲桌子
				CTable* pTable = iterTable->second;
				if (!pTable->isFulled())
					return pTable;
			}
		}
	}
	// 指定找桌子
	else
	{
		MAP_TID_TABLE_t::iterator iter = m_mapTidTable.find(tid);
		if (iter != m_mapTidTable.end())
			return iter->second;
	}

	LOG(Error, "[CGame::%s] #error# not find table:[%d]", __FUNCTION__, tid);
	return NULL;
}

// 换桌子，tid为-1，表示随机选
CTable* CGame::getOtherTable(int tid)
{
	for (MAP_NUMBER_SETTID_t::reverse_iterator r_iter = m_mapNumberTids.rbegin(); r_iter != m_mapNumberTids.rend(); ++r_iter)
	{
		std::set<int>& setTid = r_iter->second;

		for (std::set<int>::iterator iterTid = setTid.begin(); iterTid != setTid.end(); ++iterTid)
		{
			if (*iterTid == tid)
				continue;

			MAP_TID_TABLE_t::iterator iterTable = m_mapTidTable.find(*iterTid);
			if (iterTable == m_mapTidTable.end())
			{
				LOG(Error, "[CGame::%s] #error# tid:[%d] not find, bug!", __FUNCTION__, *iterTid);
				continue;
			}

			// 找到空闲桌子
			CTable* pTable = iterTable->second;
			if (!pTable->isFulled())
				return pTable;
		}
	}

	LOG(Error, "[CGame::%s] #error# not find table:[%d]", __FUNCTION__, tid);
	return NULL;
}

// 改变桌子人数
void CGame::changeTableNumber(int tid, int number)
{
	MAP_TID_TABLE_t::iterator iter = m_mapTidTable.find(tid);
	if (iter == m_mapTidTable.end())
	{
		LOG(Error, "[CGame::%s] #error# no table:[%d], bug!!!", __FUNCTION__, tid);
		return;
	}

	if (number == 0)
	{
		LOG(Error, "[CGame::%s] #error# tid:[%d] op number is 0, bug!!!", __FUNCTION__, tid);
		return;
	}

	MAP_TID_NUMBER_t::iterator iter1 = m_mapTidNumber.find(tid);
	
	int numberOld = iter1->second;
	int numberNew = numberOld + number;

	if (numberNew < 0)
	{
		LOG(Error, "[CGame::%s] #error# tid:[%d] old number:[%d] inc number:[%d], bug!!!", __FUNCTION__, tid, numberOld, number);
		assert(0);
	}

	m_mapTidNumber.erase(tid);
	m_mapNumberTids[numberOld].erase(tid);

	m_mapTidNumber[tid] = numberNew;
	m_mapNumberTids[numberNew].insert(tid);
}
