#include "Decks.h"

void CDecks::clear()
{
	m_cards.clear();
}

void CDecks::fillCards()
{
	clear();

	for (int c = MJ_COLOR_WZ; c <= MJ_COLOR_TZ; ++c)
	{
		for (int p = 1; p <= 9; ++p)
		{
			for (int i = 0; i < 4; ++i)
			{
				m_cards.push_back( Card(c, p) );
			}
		}
	}

	srand(unsigned(time(NULL)));
	std::random_shuffle(m_cards.begin(), m_cards.end());
}

bool CDecks::fetchCard(Card& card)
{
	if (m_cards.empty())
		return false;

	card = *m_cards.begin();
	m_cards.erase(m_cards.begin());

	return true;
}

int CDecks::fetchCards(CARDS_t& cards, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Card card;
		if (!fetchCard(card))
			break;

		cards.push_back(card);
	}

	return cards.size();
}

void CDecks::pushCard(const Card& card)
{
	if (!card.isValid())
		return;

	m_cards.push_back(card);
}

int	CDecks::cardCount() const
{
	return m_cards.size();
}

const CARDS_t& CDecks::getCards() const
{
	return m_cards;
}

CARDS_t CDecks::randomCards(int count)
{
	if (count <= 0)
		return CARDS_t();

	CARDS_t deckCards;

	for (int c = MJ_COLOR_WZ; c <= MJ_COLOR_TZ; ++c)
	{
		for (int p = 1; p <= 9; ++p)
		{
			for (int i = 0; i < 4; ++i)
			{
				deckCards.push_back( Card(c, p) );
			}
		}
	}

	srand(unsigned(time(NULL)));
	std::random_shuffle(deckCards.begin(), deckCards.end());

	CARDS_t cards;
	std::copy(deckCards.begin(), deckCards.begin()+count, std::back_inserter(cards));

	return cards;
}

void CDecks::fillCards(const CARDS_t& cards)
{
	m_cards = cards;
}
