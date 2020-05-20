#pragma once 

#include "libevwork/FormDef.h"

class CDispatch
	: public evwork::PHClass
{
public:
	DECLARE_FORM_MAP;

	void onEnterRoom(evwork::Jpacket& packet, evwork::IConn* pConn);
};
