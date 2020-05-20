#pragma once 

#include "libevwork/FormDef.h"

class CDispatch
	: public evwork::PHClass
{
public:
	DECLARE_FORM_MAP;

	// 协议处理

	void onChatReq(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onUinfoUpdate(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onPlayerShare(evwork::Jpacket& packet, evwork::IConn* pConn);

	void onEnterRoom(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onDissolveRoom(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onAckDissolveRoom(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onIamReay(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onPlayerPutCard(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onPlayerAction(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onPlayerTingTP(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onTickPlayer(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onTransOwnner(evwork::Jpacket& packet, evwork::IConn* pConn);
};
