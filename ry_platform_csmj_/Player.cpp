#include "Player.h"

#include "base/Entry.h"

#include "Protocol.h"
#include "Helper.h"
#include "Table.h"
#include "Game.h"

#include <algorithm>

using namespace evwork;
using namespace tinyredis;

CPlayer* CPlayer::getPlayer(int uid)
{
	CPlayer* pPlayer = new CPlayer(uid, NULL);
	if (!pPlayer->init())
	{
		delete pPlayer;
		return NULL;
	}
	return pPlayer;
}
void CPlayer::freePlayer(CPlayer* p)
{
	if (p)
		delete p;
}

CPlayer::CPlayer(int uid, evwork::IConn* pConn)
: IPlayer(uid, pConn)
, m_tid(-1)
, m_seatid(-1)
{
}
CPlayer::~CPlayer()
{
}

// 初使化玩家
bool CPlayer::init()
{
	return updateInfo();
}

// 更新资料
bool CPlayer::updateInfo()
{
	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	result = pRedis->command("hgetall hu:%d", m_uid);

	if (!result)
	{
		LOG(Error, "[CPlayer::%s] #error# hgetall hu:%d failed", __FUNCTION__, m_uid);
		return false;
	}

	if (!result.isArray())
	{
		LOG(Error, "[CPlayer::%s] #error# hgetall hu:%d, result not array", __FUNCTION__, m_uid);
		return false;
	}

	if (result.getArraySize() == 0)
	{
		LOG(Error, "[CPlayer::%s] #error# hgetall hu:%d, no data", __FUNCTION__, m_uid);
		return false;
	}

	CHashResult hashResult;

	for (size_t i = 0; i < result.getArraySize()-1; i+=2)
	{
		CResult subResultField(false), subResultValue(false);
		subResultField = result.getSubReply(i);
		subResultValue = result.getSubReply(i+1);

		std::string strField, strValue;
		subResultField.getString(strField);
		subResultValue.getString(strValue);

		hashResult.addKV(strField, strValue);
	}

	m_name = hashResult.getValue("name", "");
	m_sex = hashResult.getValue<int>("sex", 0);
	m_avatar = hashResult.getValue("avatar", "");
	m_avatar_auth = hashResult.getValue<int>("avatar_auth", 0);
	m_ps = hashResult.getValue("ps", "");
	m_money = hashResult.getValue<long long int>("money", 0);
	m_vlevel = hashResult.getValue<int>("vlevel", 0);
	m_exp = hashResult.getValue<int>("exp", 0);
	m_gid = hashResult.getValue<int>("cid", -1);

	m_rmb = hashResult.getValue<int>("rmb", 0);

	LOG(Info, "[CPlayer::%s] uid:[%d] name:[%s] money:[%lld] gid:[%d]", __FUNCTION__, m_uid, m_name.c_str(), m_money, m_gid);

	return true;
}

// 设置zid
void CPlayer::setZid(int zid)
{
	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	if (zid > 0)
		result = pRedis->command("hset hu:%d zid %d", m_uid, zid);
	else
		result = pRedis->command("hdel hu:%d zid", m_uid);
}
void CPlayer::setVid(int vid)
{
	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	if (vid > 0)
		result = pRedis->command("hset hu:%d vid %d", m_uid, vid);
	else
		result = pRedis->command("hdel hu:%d vid", m_uid);
}

// 更新金币
bool CPlayer::updateMoney()
{
	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	result = pRedis->command("hget hu:%d money", m_uid);

	if (!result || !result.isString())
	{
		LOG(Error, "[CPlayer::%s] #error# hget hu:%d money failed", __FUNCTION__, m_uid);
		return false;
	}

	std::string strValue;
	result.getString(strValue);

	m_money = convert<long long int>(strValue);
	return true;
}

// 更新游戏场次
bool CPlayer::incPlayCount()
{
	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	{
		CResult result(true);
		result = pRedis->command("hincrby hu:%d total_board %d", m_uid, 1);

		if (!result || !result.isInteger())
		{
			LOG(Error, "[CPlayer::%s] #error# hincrby hu:%d total_board %d failed", __FUNCTION__, m_uid, 1);
			return false;
		}
	}

	{
		CResult result(true);
		result = pRedis->command("hincrby hu:%d total_win %d", m_uid, 1);

		if (!result || !result.isInteger())
		{
			LOG(Error, "[CPlayer::%s] #error# hincrby hu:%d total_win %d failed", __FUNCTION__, m_uid, 1);
			return false;
		}
	}

	return true;
}

// 操作房卡
bool CPlayer::incRoomCard(int card, int& curCard)
{
	if (card == 0)
		return true;

	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	result = pRedis->command("hincrby hu:%d room_card %d", m_uid, card);

	if (!result || !result.isInteger())
	{
		LOG(Error, "[CPlayer::%s] #error# hincrby hu:%d room_card %d failed", __FUNCTION__, m_uid, card);
		return false;
	}

	curCard = (int)result.getInteger();

	LOG(Info, "[CPlayer::%s] hincrby hu:%d room_card %d => %d", __FUNCTION__, m_uid, card, curCard);

	return true;
}

// 操作金币
bool CPlayer::incMoney(long long int llMoney)
{
	if (llMoney == 0)
		return true;

	CRedisClient* pRedis = g_entry.main_db.getRedis(m_uid);

	CResult result(true);
	result = pRedis->command("hincrby hu:%d money %lld", m_uid, llMoney);

	if (!result || !result.isInteger())
	{
		LOG(Error, "[CPlayer::%s] #error# hincrby hu:%d money %lld failed", __FUNCTION__, m_uid, llMoney);
		return false;
	}

	long long int old_money = m_money;

	m_money = result.getInteger();
	
	if (m_money < 0)
	{
		LOG(Error, "[CPlayer::%s] #error# hincrby hu:%d money:%lld op %lld => now money:%lld < 0, so set 0!!!", __FUNCTION__, m_uid, old_money, llMoney, m_money);

		CResult result(true);
		result = pRedis->command("hmset hu:%d money 0", m_uid);
	}

	return true;
}

// 进入桌子，初使化相关资源
void CPlayer::initForEnterTable()
{
	//setZid( g_entry.conf["tables"]["zid"].asInt() );
	//setVid( g_entry.conf["tables"]["vid"].asInt() );

	// 场次+1
	incPlayCount();
}

// 退出桌子，清理相关资源
void CPlayer::cleanForExitTable()
{
	//setZid(0);
	//setVid(0);
}

CTable* CPlayer::__getTable()
{
	if (m_tid == -1)
		return NULL;

	return ((CGame*)g_entry.pGame)->getTable(m_tid);
}


// 通知玩家登录成功
void CPlayer::cbLoginSuccess()
{
	LOG(Info, "[CPlayer::%s] #login# uid:[%d]", __FUNCTION__, m_uid);

	//g_entry.pGame->sendCode(m_pConn, CLIENT_LOGIN_REQ, CODE_SUCCESS);
	// =>
	// 改用旧的兼容性协议
	{
		Jpacket packet;
		packet.val["cmd"] = SERVER_COMPATIBLE_LOGIN_UC;
		packet.val["code"] = 0;
		packet.end();

		g_entry.pGame->sendPacket(m_pConn, packet.tostring()); 
	}
}

// 尝试玩家退出，参数表示是否发送退出响应，返回true表示退出成功
bool CPlayer::cbTryLogout(bool bReponse)
{
	LOG(Info, "[CPlayer::%s] #login# uid:[%d] send response:[%d] ...", __FUNCTION__, m_uid, bReponse);

	CTable* pTable = __getTable();

	// 已经进入桌游
	if (pTable)
	{
		// 游戏还在进行中
		if (!pTable->exitPlayer(this))
		{
			if (bReponse)
			{
				g_entry.pGame->sendCode(m_pConn, CLIENT_LOGOUT_REQ, CODE_GAMEING);
			}

			LOG(Error, "[CPlayer::%s] #error# #login# uid:[%d] send response:[%d] failed", __FUNCTION__, m_uid, bReponse);
			return false;
		}
	}

	// 退出成功

	if (bReponse)
	{
		g_entry.pGame->sendCode(m_pConn, CLIENT_LOGOUT_REQ, CODE_SUCCESS);
	}

	LOG(Info, "[CPlayer::%s] #login# uid:[%d] send response:[%d] ok", __FUNCTION__, m_uid, bReponse);
	return true;
}


// 恢复客户端
void CPlayer::cbRecoverClient()
{
	LOG(Info, "[CPlayer::%s] #login# uid:[%d] recover new conn:[%d]", __FUNCTION__, m_uid, m_pConn->getcid());

	CTable* pTable = __getTable();

	if (pTable)
	{
		// 流程上要求，取消
		//pTable->recoverPlayer(this);
	}
}

// 托管玩家
void CPlayer::cbTrustPlayer()
{
	LOG(Info, "[CPlayer::%s] #login# uid:[%d] set conn:[NULL]", __FUNCTION__, m_uid);

	// 暂时不需要做其他处理
}

// 通知客户端被踢出
void CPlayer::cbTickClient()
{
	LOG(Info, "[CPlayer::%s] #login# uid:[%d] tick old conn:[%d]", __FUNCTION__, m_uid, m_pConn->getcid());

	g_entry.pGame->sendCode(m_pConn, CLIENT_LOGIN_REQ, CODE_OTHER_LOGIN);
}
