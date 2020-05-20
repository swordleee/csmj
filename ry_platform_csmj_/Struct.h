#pragma once

#include "libmj/Card.h"

#include <sstream>

// 玩家执行操作参数结构

struct SActionParam
{
	int type;
	CARDS_t cards;

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
