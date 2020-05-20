#pragma once 

#include <map>
#include <set>

#include "libmj/Decks.h"
#include "libmj/Hands.h"
#include "libmj/Check.h"

#include <json/json.h>

#define INIT_SCORE	1000	// 玩家初使化积分

struct SOptionAction
{
	int type;
	std::vector<CARDS_t> arrCards;

	SOptionAction()
	{
		type = 0;
	}
};

typedef std::map<int, SOptionAction> SOptionActionS;

struct SSelectAction
{
	int type;
	CARDS_t cards;

	SSelectAction()
	{
		clear();
	}

	void clear()
	{
		type = 0;
		cards.clear();
	}

	std::string toString() const
	{
		std::stringstream ss;
		ss << "type:[" << type << "] cards:[";

		for (CARDS_t::const_iterator c_iter = cards.begin(); c_iter != cards.end(); ++c_iter)
		{
			ss << c_iter->getVal() << ",";
		}

		ss << "]";
		return ss.str();
	}
};

struct SSeatLocal
{
	int binduid;
	int qibaihu;
	int ting;	
	int score;

	CHands hands;
	SOptionActionS optionActions;
	SSelectAction selectAction;

	SSeatLocal()
	{
		binduid = -1;
		qibaihu = 0;
		ting = 0;
		score = INIT_SCORE;
	}
};

struct SBalanceInfo
{
	Card huCard;
	int cardSeatId;
	bool isWin;
	std::map<int, std::set<int> > mapSeatHuType;

	std::map<int, std::map<int,int> > mapSeatPaoCount;

	std::string strZhaniao;
	std::string strScore;

	SBalanceInfo()
	{
		reset();
	}

	void reset()
	{
		huCard = Card();
		cardSeatId = -1;
		isWin = true;
		mapSeatHuType.clear();

		strZhaniao = "";
		strScore = "";
	}

	void incPaoCount(int seatid, int paoType, int count)
	{
		mapSeatPaoCount[seatid][paoType] += count;
	}
};

struct SRecords
{
	std::vector< std::pair<int, std::string> > vecTypeRecord;

	void reset()
	{
		vecTypeRecord.clear();
	}
};

class CTableLocal
{
public:
	CTableLocal(int tid);
	virtual ~CTableLocal();

	void load();

	void update();

	void del();

	SSeatLocal& getSeatLocal(int seat);

	void cleanAction();

	void cleanRound();

private:

	void __reset();

	void __Serialize(std::string& strJson);
	void __unSerialize(const std::string& strJson);

public:
	typedef std::map<int, SSeatLocal> MAP_SEAT_LOCAL_t;

	int m_tid;

	int m_gameState;

	int m_bankerSeatId;

	int m_sendCardSeatId;

	int m_putCardSeatId;

	Card m_sendCard;

	Card m_putCard;

	int m_checkSelf; 

	int m_sendCardFlag;

	CDecks m_decks;

	MAP_SEAT_LOCAL_t m_mapSeatLocal;

	SBalanceInfo m_balanceInfo;

	SRecords m_records;
};
