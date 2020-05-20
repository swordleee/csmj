#include "TableLocal.h"

#include "base/Entry.h"

using namespace evwork;
using namespace tinyredis;

#define TABLE_LOCAL_EXPIRE	3600*24*7	// 房间状态数据保存时长

CTableLocal::CTableLocal(int tid)
: m_tid(tid)
{
	__reset();
}
CTableLocal::~CTableLocal()
{
}

void CTableLocal::load()
{
	CRedisClient* pRedis = g_entry.conf_db.getRedis(0);

	CResult result(true);
	result = pRedis->command("get tablelocal:%d", m_tid);

	if (!result)
	{
		LOG(Error, "[CTableLocal::%s] #error# get tablelocal:%d failed", __FUNCTION__, m_tid);
		return;
	}

	if (!result.isString())
	{
		LOG(Info, "[CTableLocal::%s] get tablelocal:%d no data!", __FUNCTION__, m_tid);
		return;
	}

	std::string strJson;
	result.getString(strJson);

	//LOG(Debug, "[CTableLocal::%s] get tablelocal:%d json => %s", __FUNCTION__, m_tid, strJson.c_str());

	__unSerialize(strJson);
}

void CTableLocal::update()
{
	std::string strJson;
	__Serialize(strJson);

	//LOG(Debug, "[CTableLocal::%s] tablelocal:%d make json => %s", __FUNCTION__, m_tid, strJson.c_str());

	{
		CRedisClient* pRedis = g_entry.conf_db.getRedis(0);

		CResult result(true);
		result = pRedis->command("set tablelocal:%d %b", m_tid, strJson.data(), strJson.size());

		//if (result.get())
		//{
		//	CResult result2(true);
		//	result2 = pRedis->command("expire tablelocal:%d %d", m_tid, TABLE_LOCAL_EXPIRE);
		//}
	}
}

void CTableLocal::del()
{
	{
		CRedisClient* pRedis = g_entry.conf_db.getRedis(0);

		CResult result(true);
		result = pRedis->command("del tablelocal:%d", m_tid);

		if (!result)
		{
			LOG(Error, "[CTableLocal::%s] #error# del tablelocal:%d failed", __FUNCTION__, m_tid);
		}
	}
}

SSeatLocal& CTableLocal::getSeatLocal(int seat)
{
	return m_mapSeatLocal[seat];
}

void CTableLocal::cleanAction()
{
	for (MAP_SEAT_LOCAL_t::iterator iter = m_mapSeatLocal.begin(); iter != m_mapSeatLocal.end(); ++iter)
	{
		SSeatLocal& seatLocal = iter->second;

		seatLocal.optionActions.clear();
		seatLocal.selectAction.clear();
	}
}

void CTableLocal::cleanRound()
{
	std::map<int, int> mapSeatBindUid;
	std::map<int, int> mapSeatScore;

	for (MAP_SEAT_LOCAL_t::iterator iter = m_mapSeatLocal.begin(); iter != m_mapSeatLocal.end(); ++iter)
	{
		mapSeatBindUid[iter->first] = iter->second.binduid;
		mapSeatScore[iter->first] = iter->second.score;
	}

	{
		m_sendCardSeatId = -1;
		m_putCardSeatId = -1;
		m_sendCard = Card();
		m_putCard= Card();
		m_checkSelf = 1;
		m_sendCardFlag = 0;

		m_decks.clear();
		m_mapSeatLocal.clear();

		m_balanceInfo.reset();

		m_records.reset();
	}

	for (std::map<int, int>::iterator iter = mapSeatBindUid.begin(); iter != mapSeatBindUid.end(); ++iter)
	{
		m_mapSeatLocal[iter->first].binduid = iter->second;
	}

	for (std::map<int, int>::iterator iter = mapSeatScore.begin(); iter != mapSeatScore.end(); ++iter)
	{
		m_mapSeatLocal[iter->first].score = iter->second;
	}
}


void CTableLocal::__reset()
{
	m_gameState = 0;
	m_bankerSeatId = -1;

	m_sendCardSeatId = -1;
	m_putCardSeatId = -1;
	m_sendCard = Card();
	m_putCard = Card();
	m_checkSelf = 1;
	m_sendCardFlag = 0;

	m_decks.clear();
	m_mapSeatLocal.clear();

	m_balanceInfo.reset();

	m_records.reset();
}

void CTableLocal::__Serialize(std::string& strJson)
{
	Json::Value jsData;

	jsData["gamestate"] = m_gameState;

	jsData["banker_seatid"] = m_bankerSeatId;

	jsData["sendcard_seatid"] = m_sendCardSeatId;

	jsData["putcard_seatid"] = m_putCardSeatId;

	jsData["sendcard"] = m_sendCard.getVal();
	
	jsData["putcard"] = m_putCard.getVal();

	jsData["checkself"] = m_checkSelf;

	jsData["sendcardflag"] = m_sendCardFlag;

	const CARDS_t& decks = m_decks.getCards();
	for (CARDS_t::const_iterator c_iter = decks.begin(); c_iter != decks.end(); ++c_iter)
	{
		jsData["decks"].append(c_iter->getVal());
	}

	for (MAP_SEAT_LOCAL_t::iterator iter = m_mapSeatLocal.begin(); iter != m_mapSeatLocal.end(); ++iter)
	{
		SSeatLocal& seatLocal = iter->second;

		if (iter->first == -1)
			continue;

		Json::Value jsSeat;
		jsSeat["seatid"] = iter->first;

		jsSeat["binduid"] = seatLocal.binduid;

		jsSeat["qibaihu"] = seatLocal.qibaihu;

		jsSeat["ting"] = seatLocal.ting;

		jsSeat["score"] = seatLocal.score;

		const CARDS_t& handCards = seatLocal.hands.getHandCards();
		for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
		{
			jsSeat["hands"].append( c_iter->getVal() );
		}

		const EATCARDS_t& eatCards = seatLocal.hands.getEatCards();
		for (EATCARDS_t::const_iterator c_iter = eatCards.begin(); c_iter != eatCards.end(); ++c_iter)
		{
			Json::Value jsEat;
			jsEat["type"] = (int)c_iter->type;
			jsEat["first"] = c_iter->firstCard.getVal();
			jsEat["eat"] = c_iter->eatCard.getVal();

			jsSeat["eats"].append(jsEat);
		}

		const CARDS_t& outCards = seatLocal.hands.getOutCards();
		for (CARDS_t::const_iterator c_iter = outCards.begin(); c_iter != outCards.end(); ++c_iter)
		{
			jsSeat["outcards"].append( c_iter->getVal() );
		}

		for (SOptionActionS::iterator iterOption = seatLocal.optionActions.begin(); iterOption != seatLocal.optionActions.end(); ++iterOption)
		{
			SOptionAction& optionAction = iterOption->second;

			Json::Value jsOptionAction;
			jsOptionAction["type"] = optionAction.type;

			for (std::vector<CARDS_t>::iterator iterCards = optionAction.arrCards.begin(); iterCards != optionAction.arrCards.end(); ++iterCards)
			{
				Json::Value jsCards;

				for (CARDS_t::iterator iterCard = (*iterCards).begin(); iterCard != (*iterCards).end(); ++iterCard)
					jsCards["cards"].append( iterCard->getVal() );

				jsOptionAction["options"].append(jsCards);
			}

			jsSeat["optionactions"].append(jsOptionAction);
		}

		{
			Json::Value jsSelectAction;
			jsSelectAction["type"] = seatLocal.selectAction.type;
			for (CARDS_t::iterator iterCard = seatLocal.selectAction.cards.begin(); iterCard != seatLocal.selectAction.cards.end(); ++iterCard)
				jsSelectAction["cards"].append( iterCard->getVal() );

			jsSeat["selectaction"] = jsSelectAction;
		}

		jsData["seatlocals"].append(jsSeat);
	}

	jsData["balance"]["zhaniao"] = m_balanceInfo.strZhaniao;
	jsData["balance"]["score"] = m_balanceInfo.strScore;

	for (std::map<int, std::map<int,int> >::iterator iterSeat = m_balanceInfo.mapSeatPaoCount.begin(); 
		iterSeat != m_balanceInfo.mapSeatPaoCount.end(); ++iterSeat)
	{
		Json::Value jsSeat;
		jsSeat["seatid"] = iterSeat->first;

		for (std::map<int,int>::iterator iterPao = iterSeat->second.begin(); iterPao != iterSeat->second.end(); ++iterPao)
		{
			Json::Value jsPao;
			jsPao["paotype"] = iterPao->first;
			jsPao["count"] = iterPao->second;

			jsSeat["paocount"].append(jsPao);
		}

		jsData["balance"]["paocounts"].append(jsSeat);
	}

	for (std::vector< std::pair<int, std::string> >::iterator iterRecord = m_records.vecTypeRecord.begin(); iterRecord != m_records.vecTypeRecord.end(); ++iterRecord)
	{
		Json::Value jsRecord;
		jsRecord["type"] = iterRecord->first;
		jsRecord["record"] = iterRecord->second;

		jsData["records"].append(jsRecord);
	}

	Json::FastWriter writer;
	strJson = writer.write(jsData);
}

void CTableLocal::__unSerialize(const std::string& strJson)
{
	Json::Reader reader;
	Json::Value jsData;
	if (!reader.parse(strJson, jsData))
	{
		LOG(Error, "[CTableLocal::%s] #error# parse tablelocal:%d failed", __FUNCTION__, m_tid);
		return;
	}

	m_gameState = jsData.get("gamestate", 0).asInt();

	m_bankerSeatId = jsData.get("banker_seatid", -1).asInt();

	m_sendCardSeatId = jsData.get("sendcard_seatid", -1).asInt();

	m_putCardSeatId = jsData.get("putcard_seatid", -1).asInt();

	m_sendCard = jsData.get("sendcard", -1).asInt();

	m_putCard = jsData.get("putcard", -1).asInt();

	m_checkSelf = jsData.get("checkself", 1).asInt();

	m_sendCardFlag = jsData.get("sendcardflag", 0).asInt();

	for (int i = 0; i < (int)jsData["decks"].size(); i++) 
	{
		m_decks.pushCard( jsData["decks"][i].asInt() );
	}

	for (int i = 0; i < (int)jsData["seatlocals"].size(); ++i) 
	{
		Json::Value& jsSeat = jsData["seatlocals"][i];

		int seatid = jsSeat.get("seatid", -1).asInt();

		if (seatid == -1)
			continue;

		SSeatLocal& seatLocal = m_mapSeatLocal[seatid];

		seatLocal.binduid = jsSeat.get("binduid", -1).asInt();

		seatLocal.qibaihu = jsSeat.get("qibaihu", 0).asInt();

		seatLocal.ting = jsSeat.get("ting", 0).asInt();

		seatLocal.score = jsSeat.get("score", 0).asInt();

		for (int j = 0; j < (int)jsSeat["hands"].size(); ++j)
		{
			seatLocal.hands.addHandCard( jsSeat["hands"][j].asInt() );
		}

		for (int j = 0; j < (int)jsSeat["eats"].size(); ++j)
		{
			Json::Value& jsEat = jsSeat["eats"][j];

			SEatCard eat;
			eat.type = (EEatType)jsEat["type"].asInt();
			eat.firstCard = jsEat["first"].asInt();
			eat.eatCard = jsEat["eat"].asInt();

			seatLocal.hands.addEatCard(eat);
		}

		for (int j = 0; j < (int)jsSeat["outcards"].size(); ++j)
		{
			seatLocal.hands.pushOutCard( jsSeat["outcards"][j].asInt() );
		}

		for (int j = 0; j < (int)jsSeat["optionactions"].size(); ++j)
		{
			Json::Value& jsOptionAction = jsSeat["optionactions"][j];

			SOptionAction optionAction;
			optionAction.type = jsOptionAction.get("type", 0).asInt();

			if (optionAction.type == 0)
				continue;

			for (int k = 0; k < (int)jsOptionAction["options"].size(); ++k)
			{
				Json::Value& jsCards = jsOptionAction["options"][k];

				CARDS_t cards;

				for (int l = 0; l < (int)jsCards["cards"].size(); ++l)
					cards.push_back( jsCards["cards"][l].asInt() );

				optionAction.arrCards.push_back(cards);
			}

			seatLocal.optionActions[optionAction.type] = optionAction;
		}

		seatLocal.selectAction.type = jsSeat["selectaction"].get("type", 0).asInt();
		for (int j = 0; j < (int)jsSeat["selectaction"]["cards"].size(); ++j)
		{
			seatLocal.selectAction.cards.push_back( jsSeat["selectaction"]["cards"][j].asInt() );
		}
	}

	m_balanceInfo.strZhaniao = jsData["balance"]["zhaniao"].asString();
	m_balanceInfo.strScore = jsData["balance"]["score"].asString();

	for (int i = 0; i < (int)jsData["balance"]["paocounts"].size(); ++i)
	{
		Json::Value& jsSeat = jsData["balance"]["paocounts"][i];

		int seatid = jsSeat["seatid"].asInt();

		for (int j = 0; j < (int)jsSeat["paocount"].size(); ++j)
		{
			Json::Value& jsPao = jsSeat["paocount"][j];

			m_balanceInfo.mapSeatPaoCount[seatid][jsPao["paotype"].asInt()] = jsPao["count"].asInt();
		}
	}

	for (int i = 0; i < (int)jsData["records"].size(); ++i)
	{
		Json::Value& jsRecord = jsData["records"][i];
		m_records.vecTypeRecord.push_back( std::make_pair(jsRecord.get("type", -1).asInt(), jsRecord.get("record", "").asString()) );
	}
}
