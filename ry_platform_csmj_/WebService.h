#pragma once

#include <string>

class CWebService
{
public:
	CWebService() {}
	virtual ~CWebService() {}

	static bool notifyWebService(const std::string& packet);
};
