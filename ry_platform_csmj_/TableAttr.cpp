#include "TableAttr.h"

#include "base/Entry.h"
#include "Helper.h"

using namespace evwork;
using namespace tinyredis;

CTableAttr::CTableAttr(int tid)
: m_tid(tid)
, m_tidAlias(0)
{
}

CTableAttr::~CTableAttr()
{
}

bool CTableAttr::init()
{
	CRedisClient* pRedis = g_entry.room_db.getRedis(0);

	CResult result(true);
	result = pRedis->command("hgetall htid:%d", m_tid);

	if (!result)
	{
		LOG(Error, "[CTableAttr::%s] #error# hgetall htid:%d failed", __FUNCTION__, m_tid);
		return false;
	}

	if (!result.isArray())
	{
		LOG(Error, "[CTableAttr::%s] #error# hgetall htid:%d, result not array", __FUNCTION__, m_tid);
		return false;
	}

	if (result.getArraySize() == 0)
	{
		LOG(Error, "[CTableAttr::%s] #error# hgetall htid:%d, no data", __FUNCTION__, m_tid);
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

	if (hashResult.getValue<int>("delete", 0) == 1)
	{
		LOG(Error, "[CTableAttr::%s] #error# hgetall htid:%d, has delete", __FUNCTION__, m_tid);
		return false;
	}

	m_tidAlias = hashResult.getValue<int>("room_id", 0);
	m_OWUid = hashResult.getValue<int>("owner_uid", -1);
	m_totalRound = hashResult.getValue<int>("total_round", 0);
	m_playRound = hashResult.getValue<int>("play_round", 0);
	m_usedRoomCard = hashResult.getValue<int>("room_card", 0);
	m_bBankerWinScore = (hashResult.getValue<int>("bankerwin_score", 0) != 0 ? true : false);
	m_zhaniaoCount = hashResult.getValue<int>("zhaniao_count", 0);
	m_changecount = hashResult.getValue<int>("change_count", 0);

	LOG(Info, "[CTableAttr::%s] tid:[%d] owner_uid:[%d] total_round:[%d] play_round:[%d] room_card:[%d] bankerwin_score:[%d] zhaniao_count:[%d] change_count:[%d]", __FUNCTION__,
		m_tid, m_OWUid, m_totalRound, m_playRound, m_usedRoomCard, m_bBankerWinScore, m_zhaniaoCount, m_changecount);

	return true;
}

void CTableAttr::del()
{
	{
		CRedisClient* pRedis = g_entry.room_db.getRedis(0);

		CResult result(true);
		//result = pRedis->command("hset htid:%d delete %d", m_tid, 1);
		result = pRedis->command("del htid:%d", m_tid);

		if (!result)
		{
			LOG(Error, "[CTableAttr::%s] #error# tid:[%d] del failed", __FUNCTION__, m_tid);
		}
	}
}


int CTableAttr::getTid()
{
	return m_tid;
}

int CTableAttr::getTidAlias()
{
	return m_tidAlias;
}

int CTableAttr::getOWUid()
{
	return m_OWUid;
}

int CTableAttr::getTotalRound()
{
	return m_totalRound;
}

int CTableAttr::getPlayRound()
{
	return m_playRound;
}

int CTableAttr::getUsedRoomCard()
{
	return m_usedRoomCard;
}

void CTableAttr::incPlayRound(int num)
{
	m_playRound += num;

	{
		CRedisClient* pRedis = g_entry.room_db.getRedis(0);

		CResult result(true);
		result = pRedis->command("hset htid:%d play_round %d", m_tid, m_playRound);
	}
}

bool CTableAttr::isBankerWinScore()
{
	return m_bBankerWinScore;
}

int CTableAttr::getZhaniaoCount()
{
	return m_zhaniaoCount;
}

bool CTableAttr::isRoomChanged()
{
	return (m_changecount > 0);
}

bool CTableAttr::changeOWUid(int uid)
{
	{
		CRedisClient* pRedis = g_entry.room_db.getRedis(0);

		CResult result(true);
		result = pRedis->command("hmset htid:%d owner_uid %d change_count %d", m_tid, uid, m_changecount+1);

		if (!result)
		{
			LOG(Error, "[CTableAttr::%s] #error# hmset htid:%d owner_uid %d change_count %d failed", __FUNCTION__, m_tid, uid, m_changecount+1);
			return false;
		}
	}

	LOG(Info, "[CTableAttr::%s] hmset htid:%d owner_uid %d change_count %d ok", __FUNCTION__, m_tid, uid, m_changecount+1);

	m_OWUid = uid;
	++m_changecount;
	return true;
}
