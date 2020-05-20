#pragma once

#include "Card.h"

class CDecks
{
public:
	CDecks() {}
	virtual ~CDecks() {}

	void clear();
	void fillCards();
	bool fetchCard(Card& card);
	int fetchCards(CARDS_t& cards, int count);
	void pushCard(const Card& card);

	int cardCount() const;
	const CARDS_t& getCards() const;

	static CARDS_t randomCards(int count);

	void fillCards(const CARDS_t& cards);

private:
	CARDS_t m_cards;
};
