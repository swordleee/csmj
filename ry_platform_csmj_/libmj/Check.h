#pragma once

#include "Card.h"
#include "Hands.h"
#include <map>

#define QIPAIHU_SIXI					0x01
#define QIPAIHU_BANBANHU				0x02
#define QIPAIHU_QUEYISE					0x04
#define QIPAIHU_66SHUN					0x08
#define QIPAIHU_SIXI_x2					0x10
#define QIPAIHU_SIXI_x3					0x20
#define QIPAIHU_QUEYISE_x2				0x40
#define QIPAIHU_66SHUN_x2				0x80

#define HU_XIAOHU                       0x0001      // 小胡
#define HU_PENGPENGHU                   0x0002      // 碰碰胡
#define HU_JIANGJIANGHU                 0x0004      // 将将胡
#define HU_QINGYISE                     0x0008      // 清一色
#define HU_QUANQIUREN                   0x0010      // 全求人
#define HU_7DUIZI                       0x0020      // 7对子
#define HU_HAOHUA7DUI                   0x0040      // 豪华7对子
#define HU_CHAOHAOHUA7DUI               0x0080      // 超豪华7对子

class CCheck
{
public:
	CCheck() {}
	virtual ~CCheck() {}

	CCheck(const CHands& hands);

	void setHandCards(const CARDS_t& cards);
	void addHandCard(const Card& card);
	void delHandCard(const Card& card);

	void setEatCards(const EATCARDS_t& eats);
	void addEatCard(const SEatCard& eat);
	void delEatCard(const SEatCard& eat);

	bool isQiBaiHu_SiXi(EATCARDS_t& eats);
	bool isQiBaiHu_BanbanHu();
	bool isQiBaiHu_QueYiSe(int& queCount);
	bool isQiBaiHu_66Shun(EATCARDS_t& eats);

	bool isJiangCard(const Card& card);

	bool canEat(const Card& card, EATCARDS_t& eats);

	bool canPeng(const Card& card);

	bool canBu(const Card& card);
	bool canBu(EATCARDS_t& eats);

	bool canGang(const Card& card);
	bool canGang(EATCARDS_t& eats);

	bool canHu(const Card& card, EATCARDS_t& eats);
	bool canHu(EATCARDS_t& eats);

	int getHuFlag();

	bool canTing();
	bool canTing(CARDS_t& cards);

private:

	void __calcCardCount();

	int __getCardCount(const Card& card);

	void __opCardCount(const Card& card, int cnt);

	Card __remainCard();
	bool __checkHu(EATCARDS_t& eats);

	bool __is7Duizi(int& type, EATCARDS_t& eats);
	bool __isPengpengHu(EATCARDS_t& eats);
	bool __isJiangjiangHu(EATCARDS_t& eats);
	bool __isQuanqiuren(EATCARDS_t& eats);
	bool __isQingyise();

private:
	typedef std::map<Card, int> MAP_CARD_COUNT_t;

	CARDS_t m_handCards;
	EATCARDS_t m_eatCards;
	MAP_CARD_COUNT_t m_mapCardCount;

	struct SLocalVal
	{
		Card m_jiangCard;
		bool m_bQingyise;
		int m_huFlag;

		void reset()
		{
			m_jiangCard = 0;
			m_bQingyise = false;
			m_huFlag = 0;
		}
	};
	SLocalVal m_local;
};
