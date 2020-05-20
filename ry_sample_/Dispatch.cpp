#include "Dispatch.h"

#include "base/Entry.h"
#include "Game.h"
#include "Protocol.h"

using namespace evwork;

BEGIN_FORM_MAP(CDispatch)
	ON_REQUEST_CONN(CLIENT_ENTER_ROOM, &CDispatch::onEnterRoom)
END_FORM_MAP()

void CDispatch::onEnterRoom(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	// 找玩家
	CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[CDispatch::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	// 当前桌子
	int tidNow = pPlayer->m_tid;

	if (tidNow != -1)
	{
		CTable* pTable = ((CGame*)g_entry.pGame)->getTable(tidNow);

		pTable->recoverPlayer(pPlayer);
		return;
	}

	// 取参数
	Json::Value& val = packet.tojson();

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

	// 查询房间
	CTable* pTable = ((CGame*)g_entry.pGame)->getTable(_tid);
	if (pTable == NULL)
	{
		g_entry.pGame->sendCode(pConn, CLIENT_ENTER_ROOM, CODE_TABLE_NOTEXIST);
		return;
	}

	// 消息回调
	pTable->enterPlayer(pPlayer);
}
