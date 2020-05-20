#include "Dispatch.h"

#include "base/Entry.h"

#include "Protocol.h"
#include "Game.h"

using namespace evwork;

BEGIN_FORM_MAP(CDispatch)
	ON_REQUEST_CONN(CLIENT_CHAT_REQ, &CDispatch::onChatReq)
	ON_REQUEST_CONN(CLIENT_UINFO_UPDATE, &CDispatch::onUinfoUpdate)
	ON_REQUEST_CONN(CLIENT_PLAYER_SHARE, &CDispatch::onPlayerShare)

	ON_REQUEST_CONN(CLIENT_ENTER_ROOM, &CDispatch::onEnterRoom)
	ON_REQUEST_CONN(CLIENT_DISSOLVE_ROOM, &CDispatch::onDissolveRoom)
	ON_REQUEST_CONN(CLIENT_ACK_DISSOLVE_ROOM, &CDispatch::onAckDissolveRoom)
	ON_REQUEST_CONN(CLIENT_IAM_READY, &CDispatch::onIamReay)
	ON_REQUEST_CONN(CLIENT_PLAYER_PUTCARD, &CDispatch::onPlayerPutCard)
	ON_REQUEST_CONN(CLIENT_PLAYER_ACTION, &CDispatch::onPlayerAction)
	ON_REQUEST_CONN(CLIENT_PLAYER_TING_TP, &CDispatch::onPlayerTingTP)
	ON_REQUEST_CONN(CLIENT_TICK_PLAYER, &CDispatch::onTickPlayer)
	ON_REQUEST_CONN(CLIENT_TRANS_OWNER, &CDispatch::onTransOwnner)
	
END_FORM_MAP()

void CDispatch::onChatReq(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int _type = 0;
	int _index = 0;
	std::string _text = "";
	int _chatid = 0;

	try
	{
		_type = val.get("type", 0).asInt();
		_index = val.get("index", 0).asInt();
		_text = val.get("text", "").asString();
		_chatid = val.get("chatid", 0).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] type:[%d] index:[%d] text:[%s] chatid:[%d] ...", __FUNCTION__, 
		pPlayer->getUid(), _type, _index, _text.c_str(), _chatid);

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerChat(pPlayer, _type, _index, _text, _chatid);
}

void CDispatch::onUinfoUpdate(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] uinfo update ...", __FUNCTION__, pPlayer->getUid());

	// 消息回调
}

void CDispatch::onPlayerShare(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	std::string _data = "";
	try
	{
		_data = val.get("data", "").asString();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] share data:[%s] ...", __FUNCTION__, pPlayer->getUid(), _data.c_str());

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerShare(pPlayer, _data);
}

void CDispatch::onEnterRoom(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;

	if (tidNow != -1)
	{
		CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

		pTable->recoverPlayer(pPlayer);
		return;
	}

	Json::Value &val = packet.tojson();

	int _tid = -1;
	try
	{
		_tid = val.get("tid", -1).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	if (_tid == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] tid:[-1] invalid ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(_tid);
	if (pTable == NULL)
	{
		g_entry.pGame->sendCode(pConn, CLIENT_ENTER_ROOM, CODE_TABLE_NOTEXIST);
		return;
	}

	// 消息回调
	pTable->enterPlayer(pPlayer);
}

void CDispatch::onDissolveRoom(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbDissolveRoom(pPlayer);
}

void CDispatch::onAckDissolveRoom(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int _ack = 0;
	try
	{
		_ack = val.get("ack", 0).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] ack:[%d] ...", __FUNCTION__, pPlayer->getUid(), _ack);

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbAckDissolveRoom(pPlayer, _ack);
}

void CDispatch::onIamReay(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int _ready = 0;
	try
	{
		_ready = val.get("ready", 0).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] ready:[%d] ...", __FUNCTION__, pPlayer->getUid(), _ready);

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerReady(pPlayer, _ready);
}

void CDispatch::onPlayerPutCard(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int _card = -1;
	try
	{
		_card = val.get("card", -1).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	if (_card == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] card:[-1] invalid", __FUNCTION__, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CDispatch::%s] uid:[%d] card:[%d] ...", __FUNCTION__, pPlayer->getUid(), _card);

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerPutCard(pPlayer, _card);
}

void CDispatch::onPlayerAction(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	SActionParam actionParam;

	try
	{
		actionParam.type = val.get("type", -1).asInt();

		Json::Value& jsCards = val["cards"];
		if (jsCards.isArray())
		{
			for (int i = 0; i < (int)jsCards.size(); ++i)
			{
				actionParam.cards.push_back( jsCards[i].asInt() );
			}
		}
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerAction(pPlayer, actionParam);
}

void CDispatch::onPlayerTingTP(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbPlayerTingTP(pPlayer);
}

void CDispatch::onTickPlayer(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int seatid = -1;

	try
	{
		seatid = val.get("seatid", -1).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbTickPlayer(pPlayer, seatid);
}

void CDispatch::onTransOwnner(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	int tidNow = pPlayer->m_tid;
	if (tidNow == -1)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] not in room ...", __FUNCTION__, pPlayer->getUid());
		return;
	}

	Json::Value &val = packet.tojson();

	int seatid = -1;

	try
	{
		seatid = val.get("seatid", -1).asInt();
	}
	catch(...)
	{
		LOG(Error, "[CDispatch::%s] #error# uid:[%d] format error!", __FUNCTION__, pPlayer->getUid());
		return;
	}

	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

	// 消息回调
	pTable->cbTransOwnner(pPlayer, seatid);
}
