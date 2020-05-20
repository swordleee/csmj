#pragma once 

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

#define MJ_COLOR_WZ						1			// 万字
#define MJ_COLOR_BZ						2			// 饼字
#define MJ_COLOR_TZ						3			// 条字

class Card
{
public:
	Card()
		: m_val(-1)
	{
	}

	Card(int val)
		: m_val(-1)
	{
		if (__isChangShaMj(val))
		{
			m_val = val;
		}
	}

	Card(int col, int po)
		: m_val(-1)
	{
		if ( __isChangShaMj( col*10 + po ) )
		{
			m_val = col*10 + po;
		}
	}

	Card(const Card& card)
	{
		m_val = card.m_val;
	}

	bool isValid() const
	{
		return (m_val != -1);
	}

	int getVal() const
	{
		return m_val;
	}

	int getColor() const
	{
		if (m_val == -1)
			return -1;

		return m_val/10;
	}

	int getPoint() const
	{
		if (m_val == -1)
			return -1;

		return m_val%10;
	}

	Card& operator = (const Card& card)
	{
		m_val = card.m_val;
		return *this;
	}

	bool operator == (const Card& card) const
	{
		return (m_val == card.m_val);
	}

	bool operator < (const Card& card) const
	{
		return (m_val < card.m_val);
	}

	Card operator + (int n) const
	{
		return Card( m_val + n );
	}

	Card operator - (int n) const
	{
		return Card( m_val - n );
	}

private:
	
	bool __isChangShaMj(int val)
	{
		int col = val/10;
		int po = val%10;

		return (col >= MJ_COLOR_WZ && col <= MJ_COLOR_TZ && po >= 1 && po <= 9);
	}

private:
	int m_val;
};

inline bool Card_Lesser(const Card& a, const Card& b)
{
	return a < b;
}

typedef std::vector<Card> CARDS_t;

inline std::string toCardsString(const CARDS_t& cards)
{
	std::stringstream ss;
	for (CARDS_t::const_iterator c_iter = cards.begin(); c_iter != cards.end(); ++c_iter)
	{
		ss << c_iter->getVal() << ",";
	}
	return ss.str();
}
