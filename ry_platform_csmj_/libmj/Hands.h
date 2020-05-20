#pragma once

#include "Card.h"

enum EEatType
{
	ectEat = 1,	// 吃牌
	ectPeng,	// 碰牌
	ectGang,	// 明杠
	ectAnGang,	// 暗杠
	ectDui,		// 对子
	ectSingle,	// 单牌

	ectNone = -1
};

struct SEatCard
{
	EEatType type;

	Card firstCard;
	Card eatCard;

	SEatCard()
	{
		type = ectNone;
	}
};

typedef std::vector<SEatCard> EATCARDS_t;

class CHands
{
public:
	CHands() {}
	virtual ~CHands() {}

	void clearHandCards();
	void addHandCards(const CARDS_t& cards);
	void addHandCard(const Card& card);
	void delHandCard(const Card& card);
	const CARDS_t& getHandCards() const;

	void clearEatCards();
	void addEatCard(const SEatCard& eat);
	void delEatCard(const SEatCard& eat);
	const EATCARDS_t& getEatCards() const;

	void clearOutCards();
	void pushOutCard(const Card& card);
	void popOutCard();
	const CARDS_t& getOutCards() const;

private:

	void __sortHandCards();

private:
	CARDS_t m_handCards;

	EATCARDS_t m_eatCards;

	CARDS_t m_outCards;
};
