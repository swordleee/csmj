#include "Check.h"

CCheck::CCheck(const CHands& hands)
{
	const CARDS_t& handCards = hands.getHandCards();
	const EATCARDS_t& eatCards = hands.getEatCards();

	setHandCards(handCards);
	setEatCards(eatCards);
}

void CCheck::setHandCards(const CARDS_t& cards)
{
	m_handCards = cards;

	__calcCardCount();
}

void CCheck::addHandCard(const Card& card)
{
	if (!card.isValid())
		return;

	m_handCards.push_back(card);

	__opCardCount(card, 1);
}
void CCheck::delHandCard(const Card& card)
{
	if (!card.isValid())
		return;

	CARDS_t::iterator iter = std::find(m_handCards.begin(), m_handCards.end(), card);
	if (iter != m_handCards.end())
	{
		m_handCards.erase(iter);

		__opCardCount(card, -1);
	}
}

void CCheck::setEatCards(const EATCARDS_t& eats)
{
	m_eatCards = eats;
}

void CCheck::addEatCard(const SEatCard& eat)
{
	m_eatCards.push_back(eat);
}
void CCheck::delEatCard(const SEatCard& eat)
{
	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->type == eat.type && iter->firstCard == eat.firstCard && iter->eatCard == eat.eatCard)
		{
			m_eatCards.erase(iter);
			return;
		}
	}
}

bool CCheck::isQiBaiHu_SiXi(EATCARDS_t& eats)
{
	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second == 4)
		{
			SEatCard eat;
			eat.type = ectGang;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;

			eats.push_back(eat);
		}
	}

	return (!eats.empty());
}

bool CCheck::isQiBaiHu_BanbanHu()
{
	for (CARDS_t::iterator iter = m_handCards.begin(); iter != m_handCards.end(); ++iter)
	{
		if ( isJiangCard( *iter) )
			return false;
	}

	return true;
}

bool CCheck::isQiBaiHu_QueYiSe(int& queCount)
{
	std::map<int, int> mapColorCount;

	for (CARDS_t::iterator iter = m_handCards.begin(); iter != m_handCards.end(); ++iter)
	{
		mapColorCount[ iter->getColor() ]++;
	}

	queCount = (3 - mapColorCount.size());

	return (mapColorCount.size() < 3);
}

bool CCheck::isQiBaiHu_66Shun(EATCARDS_t& eats)
{
	EATCARDS_t eatsSiXi;
	bool bSiXi = isQiBaiHu_SiXi(eatsSiXi);

	int count = 0;

	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second >= 3)
		{
			if (bSiXi)
			{
				bool bPass = false;
				for (EATCARDS_t::iterator iterSiXi = eatsSiXi.begin(); iterSiXi != eatsSiXi.end(); ++iterSiXi)
				{
					if (iter->first == iterSiXi->firstCard)
					{
						bPass = true;
						break;
					}
				}
				if (bPass)
					continue;
			}

			//if (count < 2)
			{
				SEatCard eat;
				eat.type = ectPeng;
				eat.firstCard = iter->first;
				eat.eatCard = iter->first;

				eats.push_back(eat);
			}

			count++;
		}
	}

	return (count >= 2);
}

bool CCheck::isJiangCard(const Card& card)
{
	if (!card.isValid())
		return false;

	return (card.getPoint() == 2 || card.getPoint() == 5 || card.getPoint() == 8);
}

bool CCheck::canEat(const Card& card, EATCARDS_t& eats)
{
	if (!card.isValid())
		return false;

	bool bRet = false;

	if (card.getPoint() <= 7)
	{
		if ( __getCardCount( card + 1 ) > 0 && __getCardCount( card + 2 ) > 0 )
		{
			SEatCard eat;
			eat.type = ectEat;
			eat.firstCard = card;
			eat.eatCard = card;
			eats.push_back(eat);

			bRet = true;
		}
	}

	if (card.getPoint() >= 3)
	{
		if ( __getCardCount( card - 2 ) > 0 && __getCardCount( card - 1 ) > 0 )
		{
			SEatCard eat;
			eat.type = ectEat;
			eat.firstCard = card - 2;
			eat.eatCard = card;
			eats.push_back(eat);

			bRet = true;
		}
	}

	if (card.getPoint() > 1 && card.getPoint() < 9 )
	{
		if ( __getCardCount( card - 1 ) > 0 && __getCardCount( card + 1 ) > 0 )
		{
			SEatCard eat;
			eat.type = ectEat;
			eat.firstCard = card - 1;
			eat.eatCard = card;
			eats.push_back(eat);

			bRet = true;
		}
	}

	return bRet;
}

bool CCheck::canPeng(const Card& card)
{
	if (!card.isValid())
		return false;

	return (__getCardCount( card ) >= 2);
}

bool CCheck::canBu(const Card& card)
{
	if (!card.isValid())
		return false;

	return ( __getCardCount(card) == 3 );
}

bool CCheck::canBu(EATCARDS_t& eats)
{
	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second == 4)
		{
			SEatCard eat;
			eat.type = ectAnGang;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;
			eats.push_back(eat);
		}
	}

	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->type == ectPeng && __getCardCount(iter->firstCard) == 1)
		{
			SEatCard eat;
			eat.type = ectGang;
			eat.firstCard = iter->firstCard;
			eat.eatCard = iter->firstCard;
			eats.push_back(eat);
		}
	}

	return (!eats.empty());
}

bool CCheck::canGang(const Card& card)
{
	if (!canBu(card))
		return false;
	
	for (int i = 0; i < 3; ++i)
		delHandCard(card);

	{
		SEatCard eat;
		eat.type = ectGang;
		eat.firstCard = card;
		eat.eatCard = card;
		addEatCard(eat);
	}

	bool bRet = canTing();

	for (int i = 0; i < 3; ++i)
		addHandCard(card);

	{
		SEatCard eat;
		eat.type = ectGang;
		eat.firstCard = card;
		eat.eatCard = card;
		delEatCard(eat);
	}

	return bRet;
}

bool CCheck::canGang(EATCARDS_t& eats)
{
	EATCARDS_t _eats;
	if (!canBu(_eats))
		return false;

	for (EATCARDS_t::iterator iter = _eats.begin(); iter != _eats.end(); ++iter)
	{
		SEatCard& _eat = (*iter);

		if (_eat.type == ectAnGang)
		{
			for (int i = 0; i < 4; ++i)
				delHandCard(_eat.firstCard);

			{
				SEatCard eat;
				eat.type = ectAnGang;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				addEatCard(eat);
			}
		}
		else if (_eat.type == ectGang)
		{
			for (int i = 0; i < 1; ++i)
				delHandCard(_eat.firstCard);

			{
				SEatCard eat;
				eat.type = ectPeng;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				delEatCard(eat);

				eat.type = ectGang;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				addEatCard(eat);
			}
		}

		bool bRet = canTing();

		if (_eat.type == ectAnGang)
		{
			for (int i = 0; i < 4; ++i)
				addHandCard(_eat.firstCard);

			{
				SEatCard eat;
				eat.type = ectAnGang;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				delEatCard(eat);
			}
		}
		else if (_eat.type == ectGang)
		{
			for (int i = 0; i < 1; ++i)
				addHandCard(_eat.firstCard);

			{
				SEatCard eat;
				eat.type = ectGang;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				delEatCard(eat);

				eat.type = ectPeng;
				eat.firstCard = _eat.firstCard;
				eat.eatCard = _eat.firstCard;
				addEatCard(eat);
			}
		}

		if (bRet)
		{
			SEatCard eat;
			eat.type = _eat.type;
			eat.firstCard = _eat.firstCard;
			eat.eatCard = _eat.firstCard;
			eats.push_back(eat);
		}
	}

	return (!eats.empty());
}

bool CCheck::canHu(const Card& card, EATCARDS_t& eats)
{
	if (!card.isValid())
		return false;
 
	addHandCard(card);

	EATCARDS_t _eats;
	bool bRet = canHu(_eats);

	if (bRet)
	{
		for (EATCARDS_t::iterator iter = _eats.begin(); iter != _eats.end(); ++iter)
		{
			if (iter->type == ectEat && (iter->firstCard == card || iter->firstCard+1 == card || iter->firstCard+2 == card) )
			{
				SEatCard eat;
				eat.type = ectEat;
				eat.firstCard = iter->firstCard;
				eat.eatCard = card;
				eats.push_back(eat);
			}

			if (iter->type == ectPeng && iter->firstCard == card)
			{
				SEatCard eat;
				eat.type = ectPeng;
				eat.firstCard = iter->firstCard;
				eat.eatCard = card;
				eats.push_back(eat);
			}

			if (iter->type == ectDui && iter->firstCard == card)
			{
				SEatCard eat;
				eat.type = ectDui;
				eat.firstCard = iter->firstCard;
				eat.eatCard = card;
				eats.push_back(eat);
			}
		}
	}

	delHandCard(card);

	return bRet;
}

bool CCheck::canHu(EATCARDS_t& eats)
{
	m_local.reset();

	if (__isPengpengHu(eats))
	{
		m_local.m_huFlag |= HU_PENGPENGHU;
	}

	if (__isJiangjiangHu(eats))
	{
		m_local.m_huFlag |= HU_JIANGJIANGHU;
	}

	int type = 0;
	if (__is7Duizi(type, eats))
	{
		if (type == 1)
			m_local.m_huFlag |= HU_7DUIZI;
		else if (type == 2)
			m_local.m_huFlag |= HU_HAOHUA7DUI;
		else if (type == 3)
			m_local.m_huFlag |= HU_CHAOHAOHUA7DUI;
	}

	if (__isQuanqiuren(eats))
	{
		m_local.m_huFlag |= HU_QUANQIUREN;
	}

	m_local.m_bQingyise = __isQingyise();

	if (m_local.m_huFlag > 0 && m_local.m_bQingyise)
	{
		m_local.m_huFlag |= HU_QINGYISE;
	}

	if (m_local.m_huFlag == 0)
	{
		if (__checkHu(eats))
		{
			if (!m_local.m_bQingyise)
				m_local.m_huFlag |= HU_XIAOHU;
			else
				m_local.m_huFlag |= HU_QINGYISE;
		}
	}

	return (m_local.m_huFlag > 0);
}

int CCheck::getHuFlag()
{
	return m_local.m_huFlag;
}

bool CCheck::canTing()
{
	CARDS_t cards;
	return canTing(cards);
}
bool CCheck::canTing(CARDS_t& cards)
{
	for (int color = MJ_COLOR_WZ; color <= MJ_COLOR_TZ; ++color)
	{
		for (int point = 1; point <= 9; ++point)
		{
			Card card(color, point);

			if (__getCardCount(card) == 4)
				continue;

			EATCARDS_t eats;
			if ( canHu(card, eats) )
			{
				cards.push_back(card);
			}
		}
	}

	return (!cards.empty());
}

void CCheck::__calcCardCount()
{
	m_mapCardCount.clear();

	for (CARDS_t::iterator iter = m_handCards.begin(); iter != m_handCards.end(); ++iter)
	{
		m_mapCardCount[ *iter ]++;
	}
}

int CCheck::__getCardCount(const Card& card)
{
	MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.find(card);
	if (iter == m_mapCardCount.end())
		return 0;

	return iter->second;
}

void CCheck::__opCardCount(const Card& card, int cnt)
{
	m_mapCardCount[card] += cnt;

	if (m_mapCardCount[card] == 0)
	{
		m_mapCardCount.erase(card);
	}
}

Card CCheck::__remainCard()
{
	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second > 0)
		{
			return iter->first;
		}
	}

	return Card();
}

bool CCheck::__checkHu(EATCARDS_t& eats)
{
	Card card = __remainCard();

	if (!card.isValid())
		return true;

	if ( __getCardCount(card) >= 3 )
	{
		__opCardCount(card, -3);

		bool bRet = __checkHu(eats);

		__opCardCount(card, 3);

		if (bRet)
		{
			SEatCard eat;
			eat.type = ectPeng;
			eat.firstCard = card;
			eat.eatCard = card;
			eats.push_back(eat);

			return true;
		}
	}

	if ( __getCardCount(card) >= 2 && 
		 ( (m_local.m_bQingyise && !m_local.m_jiangCard.isValid()) ||
		   (isJiangCard(card) && !m_local.m_jiangCard.isValid()) )
		)
	{
		m_local.m_jiangCard = card;
		__opCardCount(card, -2);

		bool bRet = __checkHu(eats);

		__opCardCount(card, 2);

		if (bRet)
		{
			SEatCard eat;
			eat.type = ectDui;
			eat.firstCard = card;
			eat.eatCard = card;
			eats.push_back(eat);

			return true;
		}
		else
		{
			m_local.m_jiangCard = 0;
		}
	}

	if (card.getPoint() <= 7 && __getCardCount(card+1) > 0 && __getCardCount(card+2) > 0)
	{
		__opCardCount(card, -1);
		__opCardCount(card+1, -1);
		__opCardCount(card+2, -1);

		bool bRet = __checkHu(eats);

		__opCardCount(card, 1);
		__opCardCount(card+1, 1);
		__opCardCount(card+2, 1);

		if (bRet)
		{
			SEatCard eat;
			eat.type = ectEat;
			eat.firstCard = card;
			eat.eatCard = card;
			eats.push_back(eat);

			return true;
		}
	}

	return false;
}

bool CCheck::__is7Duizi(int& type, EATCARDS_t& eats)
{
	if (m_handCards.size() != 14)
		return false;

	int n2Count = 0, n4Count = 0;

	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second == 2)
		{
			SEatCard eat;
			eat.type = ectDui;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;
			eats.push_back(eat);

			n2Count++;
		}
		else if (iter->second == 4)
		{
			SEatCard eat;
			eat.type = ectGang;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;
			eats.push_back(eat);

			n2Count += 2;
			n4Count++;
		}
	}

	if (n2Count != 7)
		return false;

	type = 1 + n4Count;
	return true;
}

bool CCheck::__isPengpengHu(EATCARDS_t& eats)
{
	int jiangCount = 0;

	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if (iter->second == 2)
		{
			if (jiangCount > 0)
				return false;

			SEatCard eat;
			eat.type = ectDui;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;
			eats.push_back(eat);

			jiangCount++;
		}
		else if (iter->second == 3)
		{
			SEatCard eat;
			eat.type = ectPeng;
			eat.firstCard = iter->first;
			eat.eatCard = iter->first;
			eats.push_back(eat);
		}
		else
			return false;
	}

	if (jiangCount != 1)
		return false;

	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->type != ectPeng && iter->type != ectGang && iter->type != ectAnGang)
			return false;
	}

	return true;
}

bool CCheck::__isJiangjiangHu(EATCARDS_t& eats)
{
	for (MAP_CARD_COUNT_t::iterator iter = m_mapCardCount.begin(); iter != m_mapCardCount.end(); ++iter)
	{
		if ( !isJiangCard(iter->first) )
			return false;

		SEatCard eat;

		if (iter->second == 1)
		{
			eat.type = ectSingle;
		}
		else if (iter->second == 2)
		{
			eat.type = ectDui;
		}
		else if (iter->second == 3)
		{
			eat.type = ectPeng;
		}
		else if (iter->second == 4)
		{
			eat.type = ectGang;
		}

		eat.firstCard = iter->first;
		eat.eatCard = iter->first;
		eats.push_back(eat);
	}

	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->type == ectEat)
			return false;

		if ( !isJiangCard(iter->firstCard) )
			return false;
	}

	return true;
}

bool CCheck::__isQuanqiuren(EATCARDS_t& eats)
{
	if (m_handCards.size() == 2 && m_handCards[0] == m_handCards[1])
	{
		SEatCard eat;
		eat.type = ectDui;
		eat.firstCard = m_handCards[0];
		eat.eatCard = m_handCards[1];
		eats.push_back(eat);

		return true;
	}

	return false;
}

bool CCheck::__isQingyise()
{
	int colorFirst = -1;

	for (CARDS_t::iterator iter = m_handCards.begin(); iter != m_handCards.end(); ++iter)
	{
		if (colorFirst == -1)
		{
			colorFirst = iter->getColor();
			continue;
		}

		if (iter->getColor() != colorFirst)
			return false;
	}

	for (EATCARDS_t::iterator iter = m_eatCards.begin(); iter != m_eatCards.end(); ++iter)
	{
		if (iter->firstCard.getColor() != colorFirst)
			return false;
	}

	return true;
}
