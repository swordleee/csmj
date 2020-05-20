#pragma once 

#include <map>

class CTableAttr
{
public:
	CTableAttr(int tid);
	virtual ~CTableAttr();

	bool init();

	void del();

	int getTid();
	int getTidAlias();
	int getOWUid();
	int getTotalRound();
	int getPlayRound();
	int getUsedRoomCard();

	void incPlayRound(int num);
	
	bool isBankerWinScore();
	int getZhaniaoCount();

	bool isRoomChanged();
	bool changeOWUid(int uid);

private:
	int m_tid;
	int m_tidAlias;

	int m_OWUid;
	int m_usedRoomCard;
	int m_totalRound;
	int m_playRound;

	bool m_bBankerWinScore;
	int m_zhaniaoCount;

	int m_changecount;
};
