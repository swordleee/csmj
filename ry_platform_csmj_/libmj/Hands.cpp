#include "Hands.h"

void CHands::clearHandCards()
{
	m_handCards.clear();
}

void CHands::addHandCards(const CARDS_t& cards)
{
	std::copy(cards.begin(), cards.end(), std::back_inserter(m_handCards));

	__sortHandCards();
}
void CHands::addHandCard(const Card& card)
{
	m_handCards.push_back(card);

	__sortHandCards();
}

void CHands::delHandCard(const Card& card)
{
	CARDS_t::iterator iter = std::find(m_handCards.begin(), m_handCards.end(), card);
	if (iter != m_handCards.end())
	{
		m_handCards.erase(iter);
	}
}

const CARDS_t& CHands::getHandCards() const
{
	return m_handCards;
}

void CHands::clearEatCards()
{
	m_eatCards.clear();
}

void CHands::addEatCard(const SEatCard& eat)
{
	m_eatCards.push_back(eat);
}

void CHands::delEatCard(const SEatCard& eat)
{
	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->type == eat.type && iter->firstCard == eat.firstCard && iter->eatCard == eat.eatCard)
		{
			m_eatCards.erase(iter);
			break;
		}
	}
}

const EATCARDS_t& CHands::getEatCards() const
{
	return m_eatCards;
}

void CHands::__sortHandCards()
{
	std::sort(m_handCards.begin(), m_handCards.end(), Card_Lesser);
}

void CHands::clearOutCards()
{
	m_outCards.clear();
}

void CHands::pushOutCard(const Card& card)
{
	m_outCards.push_back(card);
}

void CHands::popOutCard()
{
	if (m_outCards.empty())
		return;

	m_outCards.pop_back();
}

const CARDS_t& CHands::getOutCards() const
{
	return m_outCards;
}
