#include "Table.h"

#include "base/Entry.h"

#include "Game.h"
#include "Protocol.h"
#include "Helper.h"

#include "include/base64/Base64.h"
#include "WebService.h"

using namespace evwork;

#define OWNER_SEATID				1
#define DEAMON_INTERVAL				2
#define COOL_TABLE_SECONDS			600
#define GANGHU_ASK_SECONDS			3
#define DISSOLVE_SESSION_EXPIRE		180 // 解散房间超时

#define QISHOUHU_SECONDS			0.1
#define PLAY_CHARDS_SECONDS			0.1
#define PLAY_CHARDS_QISHOU_SECONDS	3
#define GOON_ROUND_SECONDS			2

CTable::CTable(int tid)
: m_tid(tid)
, m_tableAttr(tid)
, m_tableLocal(tid)
, m_pDissolveSession(NULL)
{
	int r = m_random(0, DEAMON_INTERVAL*1000, m_tid);
	float fr = r/(float)1000;

	m_evTimerDeamon.data = this;
	ev_timer_init(&m_evTimerDeamon, CTable::__cbTimerDeamon, DEAMON_INTERVAL + fr, DEAMON_INTERVAL);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDeamon);
}

CTable::~CTable()
{
	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDeamon);

	if (m_pDissolveSession)
	{
		delete m_pDissolveSession;
		m_pDissolveSession = NULL;
	}
}

int CTable::getTid()
{
	return m_tid;
}

bool CTable::init()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];
		seat.seatid = SSeat::getSeatId(i);
		seat.uid = -1;
		seat.occupied = false;
		seat.ready = false;
	}

	if (!m_tableAttr.init())
	{
		LOG(Error, "[CTable::%s] #error# table:[%d] tableattr init failed", __FUNCTION__, m_tid);
		return false;
	}

	m_tableLocal.load();

	//if (m_tableLocal.getSeatLocal(OWNER_SEATID).binduid == -1)
	//{
	//	LOG(Info, "[CTable::%s] table:[%d] bind owner:[%d]", __FUNCTION__, m_tid, m_tableAttr.getOWUid());

	//	m_tableLocal.getSeatLocal(OWNER_SEATID).binduid = m_tableAttr.getOWUid();

	//	CPlayer* pPlayer = CPlayer::getPlayer( m_tableAttr.getOWUid() );
	//	if (pPlayer)
	//	{
	//		pPlayer->setVid( g_entry.conf["tables"]["vid"].asInt() );
	//		pPlayer->setZid(m_tid);
	//		CPlayer::freePlayer(pPlayer);
	//	}		
	//}
	__tryBindOwnner();

	__initTimerLocal();

	if (m_tableLocal.m_gameState == S_INIT)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_INIT", __FUNCTION__, m_tid);
	}
	else if (m_tableLocal.m_gameState == S_FIRSTCARDS)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_FIRSTCARDS", __FUNCTION__, m_tid);

		__startQiShouHu();
	}
	else if (m_tableLocal.m_gameState == S_QISHOUHU)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_QISHOUHU", __FUNCTION__, m_tid);

		__startPlayCards(false);
	}
	else if (m_tableLocal.m_gameState == S_PLAY)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_PLAY", __FUNCTION__, m_tid);
	}
	else if (m_tableLocal.m_gameState == S_END)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_END", __FUNCTION__, m_tid);

		__startGoonRound();
	}
	else if (m_tableLocal.m_gameState == S_GOON)
	{
		LOG(Info, "[CTable::%s] table:[%d] S_GOON", __FUNCTION__, m_tid);
	}

	LOG(Info, "[CTable::%s] tid:[%d] owner:[%d] init ok", __FUNCTION__, m_tid, m_tableAttr.getOWUid());
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
// 框架模板

bool CTable::enterPlayer(CPlayer* pPlayer)
{
	if (pPlayer->m_tid != -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] has seat in tid:[%d] seatid:[%d]", __FUNCTION__, 
			m_tid, pPlayer->getUid(), pPlayer->m_tid, pPlayer->m_seatid);
		return false;
	}

	int seatid = __chooseSeatId( pPlayer->getUid() );
	if (seatid == -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] no free seat", __FUNCTION__, m_tid, pPlayer->getUid());

		g_entry.pGame->sendCode(pPlayer, CLIENT_ENTER_ROOM, CODE_SEAT_NOFREE);
		return false;
	}

	SSeat& seat = __getSeatById(seatid);

	seat.uid = pPlayer->getUid();
	seat.occupied = true;

	pPlayer->m_tid = m_tid;
	pPlayer->m_seatid = seat.seatid;

	pPlayer->initForEnterTable();

	__broadPlayerEnter(pPlayer);

	__sendTableInfo(pPlayer, false);

	__sendReentryInfo(pPlayer);
	__sendDissolveSession(pPlayer);

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] ok", __FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_seatid);
	return true;
}

bool CTable::exitPlayer(CPlayer* pPlayer)
{
	if (pPlayer->m_tid != m_tid)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] real seat in tid:[%d] seatid:[%d]", 
			__FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_tid, pPlayer->m_seatid);
		return false;
	}

	//// 游戏时不能退出
	//if (__isGameing())
	//{
	//	LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] gamestate:[%d], but is gameing", 
	//		__FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_seatid, m_tableLocal.m_gameState);
	//	return false;
	//}

	SSeat& seat = __getSeatById(pPlayer->m_seatid);

	int curSeatId = seat.seatid;

	pPlayer->cleanForExitTable();

	pPlayer->m_tid = -1;
	pPlayer->m_seatid = -1;

	seat.uid = -1;
	seat.occupied = false;
	seat.ready = false;

	bool bSeatBinded = (m_tableLocal.getSeatLocal(curSeatId).binduid != -1);

	__broadPlayerExit(pPlayer, curSeatId, bSeatBinded);

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] binded:[%d] exit ok", __FUNCTION__, m_tid, pPlayer->getUid(), curSeatId, bSeatBinded);
	return true;
}

void CTable::recoverPlayer(CPlayer* pPlayer)
{
	__sendTableInfo(pPlayer, true);

	__sendReentryInfo(pPlayer);
	__sendDissolveSession(pPlayer);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
// 消息回调/辅助

void CTable::cbPlayerChat(CPlayer* pPlayer, int type, int index, const std::string& text, int chatid)
{
	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] type:[%d] index:[%d] chatid:[%d] text:[%s]", __FUNCTION__, 
		m_tid, pPlayer->getUid(), type, index, chatid, text.c_str());

	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_CHAT_BC;
	packet_r.val["uid"] = pPlayer->getUid();
	packet_r.val["seatid"] = pPlayer->m_seatid;
	packet_r.val["type"] = type;
	packet_r.val["index"] = index;
	packet_r.val["text"] = text;
	packet_r.val["chatid"] = chatid;
	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::cbPlayerShare(CPlayer* pPlayer, const std::string& data)
{
	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] data:[%s]", __FUNCTION__, m_tid, pPlayer->getUid(), data.c_str());

	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_PLAYER_SHARE;
	packet_r.val["seatid"] = pPlayer->m_seatid;
	packet_r.val["data"] = data;
	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::cbPlayerReady(CPlayer* pPlayer, int type)
{
	if (__isGameing())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] type:[%d] disable, because is gameing", __FUNCTION__,
			m_tid, pPlayer->getUid(), type);
		return;
	}

	SSeat& seat = __getSeatById(pPlayer->m_seatid);
	seat.ready = type;

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_PLAYER_READY_BC;
		packet_r.val["seatid"] = pPlayer->m_seatid;
		packet_r.val["type"] = type;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	if (m_tableLocal.m_gameState == S_INIT)
	{
		__initTableEntry();
	}
}

void CTable::cbPlayerPutCard(CPlayer* pPlayer, Card card)
{
	if (m_tableLocal.m_gameState != S_PLAY)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] card:[%d] disable, because not play", __FUNCTION__,
			m_tid, pPlayer->getUid(), card.getVal());
		return;
	}

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];
		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);
		if (!seatLocal.optionActions.empty())
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] card:[%d] disable, because somebody has action", __FUNCTION__,
				m_tid, pPlayer->getUid(), card.getVal());
			return;
		}
	}

	if (pPlayer->m_seatid != m_tableLocal.m_putCardSeatId)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] not putcardseatid:[%d], disable!", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, m_tableLocal.m_putCardSeatId);
		return;
	}
	
	if (m_tableLocal.m_putCard.isValid())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] has putcard, disable!", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, m_tableLocal.m_putCardSeatId);
		return;
	}

	if (!card.isValid())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] card:[%d], invalid!", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, card.getVal());
		return;
	}

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId);

	if (!__checkCardFromHands(seatLocal, card))
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] card:[%d], not from hands!", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, card.getVal());
		return;
	}

	if (seatLocal.ting == 1 && !(card == m_tableLocal.m_sendCard) )
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] is ting, must put card:[%d]", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, m_tableLocal.m_putCardSeatId, m_tableLocal.m_sendCard.getVal());

		g_entry.pGame->sendCode(pPlayer, CLIENT_PLAYER_PUTCARD, CODE_CANNOT_CHANCARDS);
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] putcard:[%d]", __FUNCTION__,
		m_tid, pPlayer->getUid(), pPlayer->m_seatid, card.getVal());

	m_tempVar.cleanPassHu(pPlayer->m_seatid);

	seatLocal.hands.delHandCard(card);
	seatLocal.hands.pushOutCard(card);
	m_tableLocal.m_putCard = card;

	// 牌局记录：玩家出牌
	__recordPutcard(m_tableLocal.m_putCardSeatId, card, seatLocal.hands.getHandCards());

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_PLAYER_PUTCARD_BC;
		packet_r.val["seatid"] = pPlayer->m_seatid;
		packet_r.val["card"] = card.getVal();

		const CARDS_t& handCards = seatLocal.hands.getHandCards();

		int zeroCount = 13 - handCards.size();
		for (int i = 0; i < zeroCount; ++i)
			packet_r.val["hands"].append(0);

		for (CARDS_t::const_iterator c_iter =  handCards.begin(); c_iter != handCards.end(); ++c_iter)
		{
			if (seat.seatid == pPlayer->m_seatid)
				packet_r.val["hands"].append( c_iter->getVal() );
			else
				packet_r.val["hands"].append( -1 );
		}

		packet_r.val["hands"].append(0);

		packet_r.end();

		__sendPacketToSeatId(seat.seatid, packet_r.tostring());
	}

	{
		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId);
		CCheck check(seatLocal.hands);

		if (check.canTing())
		{
			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_PLAYER_TING_UC;
			packet_r.val["seatid"] = m_tableLocal.m_putCardSeatId;
			packet_r.end();

			__sendPacketToSeatId(m_tableLocal.m_putCardSeatId, packet_r.tostring());
		}
	}


	bool bOptionAction = false;
	int nextSeatId = m_tableLocal.m_putCardSeatId;

	for (int i = 1; i < MAX_SEAT; ++i)
	{
		nextSeatId = SSeat::getNextSeatid(nextSeatId);
		SSeatLocal& seatLocalN = m_tableLocal.getSeatLocal(nextSeatId);

		CCheck check(seatLocalN.hands);

		EATCARDS_t eatCards;
		if (i == 1 && seatLocalN.ting == 0 && check.canEat(card, eatCards))
		{
			SOptionAction& optionAction = seatLocalN.optionActions[ACTION_CHI];
			optionAction.type = ACTION_CHI;
			for (EATCARDS_t::iterator iter = eatCards.begin(); iter != eatCards.end(); ++iter)
			{
				SEatCard& eat = (*iter);

				CARDS_t cards;
				if (eat.firstCard == card)
				{
					cards.push_back(card + 1);
					cards.push_back(card + 2);
				}
				else if (eat.firstCard == card - 2)
				{
					cards.push_back(card - 2);
					cards.push_back(card - 1);
				}
				else if (eat.firstCard == card - 1)
				{
					cards.push_back(card - 1);
					cards.push_back(card + 1);
				}

				optionAction.arrCards.push_back(cards);
			}
		}

		if (seatLocalN.ting == 0 && check.canPeng(card))
		{
			SOptionAction& optionAction = seatLocalN.optionActions[ACTION_PENG];
			optionAction.type = ACTION_PENG;
		}

		if (seatLocalN.ting == 0 && check.canBu(card))
		{
			if (m_tableLocal.m_decks.cardCount() >= 2)
			{
				SOptionAction& optionAction = seatLocalN.optionActions[ACTION_BU];
				optionAction.type = ACTION_BU;
			}
		}

		if (seatLocalN.ting == 0 && check.canGang(card))
		{
			if (m_tableLocal.m_decks.cardCount() >= 3)
			{
				SOptionAction& optionAction = seatLocalN.optionActions[ACTION_GANG];
				optionAction.type = ACTION_GANG;
			}
		}

		EATCARDS_t huCards;
		if (!m_tempVar.isCannotHu(nextSeatId) && check.canHu(card, huCards))
		{
			SOptionAction& optionAction = seatLocalN.optionActions[ACTION_HU];
			optionAction.type = ACTION_HU;
		}

		if (!seatLocalN.optionActions.empty())
		{
			SOptionAction& optionAction = seatLocalN.optionActions[ACTION_PASS];
			optionAction.type = ACTION_PASS;

			bOptionAction = true;
			__sendOptionActions(nextSeatId, seatLocalN.optionActions);
		}
	}

	if (bOptionAction)
	{
		m_tableLocal.update();
	}
	else
	{
		m_tableLocal.m_sendCardSeatId = SSeat::getNextSeatid(m_tableLocal.m_putCardSeatId);
		m_tableLocal.m_sendCard = Card();
		m_tableLocal.m_sendCardFlag = 0;

		LOG(Info, "[CTable::%s] tid:[%d] no player can option, so sendcard to next player seatid:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

		m_tableLocal.update();

		__sendCardLogic();
	}
}

void CTable::cbPlayerAction(CPlayer* pPlayer, const SActionParam& action)
{
	if (m_tableLocal.m_gameState != S_PLAY)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] action:[%s] disable, because not play", __FUNCTION__,
			m_tid, pPlayer->getUid(), action.toString().c_str());
		return;
	}

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(pPlayer->m_seatid);

	SOptionActionS::iterator iterOptionAction = seatLocal.optionActions.find(action.type);
	if (iterOptionAction == seatLocal.optionActions.end())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] action:[%s] disable, not in options", __FUNCTION__,
			m_tid, pPlayer->getUid(), pPlayer->m_seatid, action.toString().c_str());
		return;
	}

	if (seatLocal.selectAction.type != 0)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] has select, repeat", __FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_seatid);
		return;
	}

	if (!iterOptionAction->second.arrCards.empty())
	{
		SOptionAction& optionAction = iterOptionAction->second;

		bool bFind = false;
		for (std::vector<CARDS_t>::iterator iter = optionAction.arrCards.begin(); iter != optionAction.arrCards.end(); ++iter)
		{
			if ( *iter == action.cards )
			{
				bFind = true;
				break;
			}
		}

		if (!bFind)
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] action:[%s] disable, cards not compare", __FUNCTION__,
				m_tid, pPlayer->getUid(), pPlayer->m_seatid, action.toString().c_str());
			return;
		}
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] action:[%s]", __FUNCTION__, m_tid, pPlayer->getUid(), action.toString().c_str());

	{
		SOptionActionS::iterator iterOptionAction1 = seatLocal.optionActions.find(ACTION_HU);
		//SOptionActionS::iterator iterOptionAction2 = seatLocal.optionActions.find(ACTION_GANGHU);
		if (iterOptionAction1 != seatLocal.optionActions.end()/* ||
			iterOptionAction2 != seatLocal.optionActions.end()*/)
		{
			if (m_tableLocal.m_putCard.isValid() && action.type != ACTION_HU/* && action.type != ACTION_GANGHU*/)
			{
				LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] action:[%s] ######PASS HU######", __FUNCTION__, m_tid, pPlayer->getUid(), action.toString().c_str());
				m_tempVar.setPassHu(pPlayer->m_seatid);
			}
		}
	}

	seatLocal.selectAction.type = action.type;
	seatLocal.selectAction.cards = action.cards;

	std::vector<int> vecDecideSeatId;
	int decideActionType = __decideOptionAction(vecDecideSeatId);
	if (decideActionType == -1)
	{
		LOG(Info, "[CTable::%s] tid:[%d] wait other player action!", __FUNCTION__, m_tid);
		return;
	}

	std::map<int, SSelectAction> mapSeatSelectAction;
	for (std::vector<int>::iterator iter = vecDecideSeatId.begin(); iter != vecDecideSeatId.end(); ++iter)
	{
		int decideSeatId = (*iter);

		SSeatLocal& seatLocalDecide = m_tableLocal.getSeatLocal(decideSeatId);
		mapSeatSelectAction[decideSeatId] = seatLocalDecide.selectAction;

		LOG(Info, "[CTable::%s] tid:[%d] decide select action, seatid:[%d] action:[%s]", __FUNCTION__, 
			m_tid, decideSeatId, seatLocalDecide.selectAction.toString().c_str());
	}

	if (decideActionType == ACTION_GANG || decideActionType == ACTION_BU)
	{
		if (__checkMyGangBuButOtherHu(mapSeatSelectAction))
		{
			LOG(Info, "[CTable::%s] tid:[%d] action has been other qiang hu, asking...", __FUNCTION__, m_tid);
			return;
		}
	}

	m_tableLocal.cleanAction();


	switch (decideActionType)
	{
	case ACTION_PASS:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __passCurAction()", __FUNCTION__, m_tid, pPlayer->getUid());
			__passCurAction();
		}
		break;
	case ACTION_HAIDI_YAO:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __haidiYaoCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__haidiYaoCardLogic(false);
		}
		break;
	case ACTION_HAIDI_BUYAO:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __haidiBuYaoLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__haidiBuYaoLogic();
		}
		break;
	case ACTION_CHI:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __chiCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__chiCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_PENG:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __pengCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__pengCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_GANG:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __gangCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__gangCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_BU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __buCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__buCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_HU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __huCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__huCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_GANGHU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __ganghuCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__ganghuCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_HAIDIHU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __haidihuCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__haidihuCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_QIANGGANGHU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __qiangganghuCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__qiangganghuCardLogic(mapSeatSelectAction);
		}
		break;
	case ACTION_QIANGBUHU:
		{
			LOG(Info,  "[CTable::%s] tid:[%d] uid:[%d] call __qiangbuhuCardLogic()", __FUNCTION__, m_tid, pPlayer->getUid());
			__qiangbuhuCardLogic(mapSeatSelectAction);
		}
		break;
	}
}

void CTable::cbPlayerTingTP(CPlayer* pPlayer)
{
	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] get ting cards", __FUNCTION__, m_tid, pPlayer->getUid());

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(pPlayer->m_seatid);
	CCheck check(seatLocal.hands);

	CARDS_t tingCards;
	check.canTing(tingCards);

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_PLAYER_TING_TP_UC;
		packet_r.val["seatid"] = pPlayer->m_seatid;

		for (CARDS_t::iterator iter = tingCards.begin(); iter != tingCards.end(); ++iter)
		{
			packet_r.val["tingcards"].append( iter->getVal() );
		}

		packet_r.end();

		g_entry.pGame->sendPacket(pPlayer, packet_r.tostring());
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] get ting cards:[%s]", __FUNCTION__, m_tid, pPlayer->getUid(), toCardsString(tingCards).c_str());
}

void CTable::cbTickPlayer(CPlayer* pPlayer, int seatid)
{
	if (m_tableLocal.m_gameState != S_INIT)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, gamestate not S_INIT", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	if (pPlayer->getUid() != m_tableAttr.getOWUid())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, not owner", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	SSeat tickSeat = __getSeatById(seatid);
	if (seatid == pPlayer->m_seatid || tickSeat.seatid == -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, can't tick mine or seatid invalid", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	CPlayer* pPlayerTick = (CPlayer*)g_entry.pGame->findPlayerByUid(tickSeat.uid);
	if (pPlayerTick == NULL)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, other not online", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] tick ok", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);


	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_PLAYER_TICKED_BC;
		packet_r.val["seatid"] = seatid;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	g_entry.pGame->tickPlayer(pPlayerTick);
}

void CTable::cbTransOwnner(CPlayer* pPlayer, int seatid)
{
	if (m_tableLocal.m_gameState != S_INIT)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, gamestate not S_INIT", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	if (pPlayer->getUid() != m_tableAttr.getOWUid())
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, not owner", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	SSeat tickSeat = __getSeatById(seatid);
	if (seatid == pPlayer->m_seatid || tickSeat.seatid == -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, can't trans mine or seatid invalid", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	CPlayer* pPlayerTrans = (CPlayer*)g_entry.pGame->findPlayerByUid(tickSeat.uid);
	if (pPlayerTrans == NULL)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] seatid:[%d] disable, other not online", __FUNCTION__, m_tid, pPlayer->getUid(), seatid);
		return;
	}

	if (!m_tableAttr.changeOWUid(pPlayerTrans->getUid()))
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] trans ownner from uid:[%d]@seatid:[%d] => uid:[%d]@seatid:[%d] failed", __FUNCTION__, m_tid, 
			pPlayer->getUid(), pPlayer->getUid(), pPlayer->m_seatid, pPlayerTrans->getUid(), pPlayerTrans->m_seatid);
		return;
	}

	__transBindOwnner(pPlayer->m_seatid, pPlayer->getUid(), pPlayerTrans->m_seatid, pPlayerTrans->getUid());

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_OWNER_TRANSED_BC;
		packet_r.val["from_seatid"] = pPlayer->m_seatid;
		packet_r.val["to_seatid"] = seatid;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] trans ownner from uid:[%d]@seatid:[%d] => uid:[%d]@seatid:[%d] ok", __FUNCTION__, m_tid, 
		pPlayer->getUid(), pPlayer->getUid(), pPlayer->m_seatid, pPlayerTrans->getUid(), pPlayerTrans->m_seatid);
}

void CTable::cbDissolveRoom(CPlayer* pPlayer)
{
	if (m_tableLocal.m_gameState == S_OVER)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] disable, gamestate is S_OVER", __FUNCTION__, m_tid, pPlayer->getUid());
		return;
	}

	if (m_tableLocal.m_gameState == S_INIT)
	{
		if (pPlayer->getUid() == m_tableAttr.getOWUid())
		{
			LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] owner:[%d]", __FUNCTION__, m_tid, pPlayer->getUid(), m_tableAttr.getOWUid());

			{
				Jpacket packet_r;
				packet_r.val["cmd"] = SERVER_DISSOLVE_ROOM_RESULT_BC;
				packet_r.val["result"] = 1;
				packet_r.end();

				__broadPacket(NULL, packet_r.tostring());
			}

			__switchToState(S_OVER);
		}
		else
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] in S_INIT, not owner", __FUNCTION__, m_tid, pPlayer->getUid());
		}

		return;
	}

	if (m_pDissolveSession)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] disable, last session has create", __FUNCTION__, m_tid, pPlayer->getUid());
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] owner:[%d]", __FUNCTION__, m_tid, pPlayer->getUid(), m_tableAttr.getOWUid());

	{
		m_pDissolveSession = new CDissolveSession(this, pPlayer->m_seatid, DISSOLVE_SESSION_EXPIRE);
		m_pDissolveSession->askDissolve();
	}
}

void CTable::cbAckDissolveRoom(CPlayer* pPlayer, int ack)
{
	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] ack:[%d]", __FUNCTION__, m_tid, pPlayer->getUid(), ack);

	if (m_pDissolveSession == NULL)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] ack:[%d] disable, beacause no session", __FUNCTION__, m_tid, pPlayer->getUid(), ack);
		return;
	}

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_ACK_DISSOLVE_ROOM_BC;
		packet_r.val["seatid"] = pPlayer->m_seatid;
		packet_r.val["ack"] = ack;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	m_pDissolveSession->setSeatAck(pPlayer->m_seatid, ack);
}

void CTable::cbDissolveRoomResult(int resul)
{
	LOG(Info, "[CTable::%s] tid:[%d] result:[%d]", __FUNCTION__, m_tid, resul);

	{
		delete m_pDissolveSession;
		m_pDissolveSession = NULL;
	}

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_DISSOLVE_ROOM_RESULT_BC;
		packet_r.val["result"] = resul;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	if (resul == 0)
		return;

	if (m_tableLocal.m_gameState == S_OVER)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] has in S_OVER", __FUNCTION__, m_tid);
		return;
	}

	__broadTotalBalance();

	if (resul == 1)
	{
		__stopQiShouHu();
		__stopPlayCards();
		__stopGoonRound();

		__stopGangHuAsk();

		__switchToState(S_OVER);
	}
}

//----------------------------------------------------------------------------------------------------------------------------------------------------
// 业务逻辑辅助

void CTable::__initTableEntry()
{
	LOG(Info, "[CTable::%s] tid:[%d] init ...", __FUNCTION__, m_tid);

	if (!__isAllReady())
	{
		LOG(Info, "[CTable::%s] tid:[%d] not all player ready", __FUNCTION__, m_tid);
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] game ready", __FUNCTION__, m_tid);

	__bindPlayers();

	__firstCards(0);
}

void CTable::__goonTableEntry()
{
	LOG(Info, "[CTable::%s] tid:[%d] goon ...", __FUNCTION__, m_tid);

	if (!__isAllReady())
	{
		LOG(Info, "[CTable::%s] tid:[%d] not all player ready", __FUNCTION__, m_tid);
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] game goon", __FUNCTION__, m_tid);

	__firstCards(1);
}

void CTable::__overTableEntry()
{
	LOG(Info, "[CTable::%s] tid:[%d] over ...", __FUNCTION__, m_tid);

	if (m_tableAttr.getPlayRound() == 0)
	{
		LOG(Info, "[CTable::%s] #room_card# tid:[%d] +room_card[%d] to uid:[%d]", __FUNCTION__, m_tid, m_tableAttr.getUsedRoomCard(), m_tableAttr.getOWUid());

		CPlayer* pPlayer = CPlayer::getPlayer( m_tableAttr.getOWUid() );
		if (pPlayer)
		{
			int curCard = 0;
			if ( pPlayer->incRoomCard(m_tableAttr.getUsedRoomCard(), curCard) )
			{
				LOG(Info, "[CTable::%s] #room_card# tid:[%d] +room_card[%d] to uid:[%d] => now card:[%d]", __FUNCTION__, 
					m_tid, m_tableAttr.getUsedRoomCard(), m_tableAttr.getOWUid(), curCard);

				g_entry.eventlog.commit_eventlog(GAME_TYPE_CHANGSHA/*长沙麻将*/, m_tableAttr.getOWUid(), m_tid, m_tableAttr.getUsedRoomCard(), curCard, 2, 1, false);
			}
			else
			{
				LOG(Error, "[CTable::%s] #error# #room_card# tid:[%d] +room_card[%d] to uid:[%d] failed", __FUNCTION__, 
					m_tid, m_tableAttr.getUsedRoomCard(), m_tableAttr.getOWUid());
			}

			CPlayer::freePlayer(pPlayer);
		}
	}
	else
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SYS_SCORE1_UPLOAD;
		packet_r.val["tid"] = m_tid;
		packet_r.val["tidalias"] = m_tableAttr.getTidAlias();

		for (int i = 0; i < MAX_SEAT; ++i)
		{
			SSeat& seat = m_seats[i];

			CPlayer* pPlayer = CPlayer::getPlayer( m_tableLocal.getSeatLocal(seat.seatid).binduid );
			if (pPlayer)
			{
				Json::Value jsScore;

				jsScore["uid"] = pPlayer->getUid();
				jsScore["name"] = pPlayer->m_name;
				jsScore["seatid"] = seat.seatid;
				jsScore["score"] = m_tableLocal.getSeatLocal(seat.seatid).score - INIT_SCORE;

				packet_r.val["scores"].append(jsScore);

				CPlayer::freePlayer(pPlayer);
			}
		}

		packet_r.end();

		if ( !CWebService::notifyWebService( packet_r.val.toStyledString() ) )
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] notify webservice SYS_SCORE1_UPLOAD failed", __FUNCTION__, m_tid);
		}
	}

	__tickAllPlayer();

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (m_tableLocal.getSeatLocal(seat.seatid).binduid == -1)
			continue;

		CPlayer* pPlayer = CPlayer::getPlayer( m_tableLocal.getSeatLocal(seat.seatid).binduid );
		if (pPlayer)
		{
			LOG(Info, "[CTable::%s] #bind# tid:[%d] set uid:[%d] vid(%d) zid(%d)", __FUNCTION__, 
				m_tid, pPlayer->getUid(), 0, 0);

			pPlayer->setVid(0);
			pPlayer->setZid(0);
			CPlayer::freePlayer(pPlayer);
		}
	}

	{
		LOG(Info, "[CTable::%s] #bind# tid:[%d] owner:[%d] free table", __FUNCTION__, m_tid, m_tableAttr.getOWUid());

		m_tableAttr.del();
		m_tableLocal.del();
		((CGame*)g_entry.pGame)->coolTable(m_tid);
	}
}

void CTable::__firstCards(int bankerType)
{
	if (bankerType == 0)
	{
		m_tableLocal.m_bankerSeatId = __getOwnnerSeatId();

		LOG(Info, "[CTable::%s] tid:[%d] random banker seatid:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_bankerSeatId);
	}
	else
	{
		LOG(Info, "[CTable::%s] tid:[%d] continue banker seatid:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_bankerSeatId);
	}

	LOG(Info, "[CTable::%s] tid:[%d] round:[%d] first cards", __FUNCTION__, m_tid, m_tableAttr.getPlayRound()+1);

	m_tableLocal.m_decks.fillCards();

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		CARDS_t cards;
		if (seat.seatid == m_tableLocal.m_bankerSeatId)
			m_tableLocal.m_decks.fetchCards(cards, 14);
		else
			m_tableLocal.m_decks.fetchCards(cards, 13);

		m_tableLocal.getSeatLocal(seat.seatid).hands.addHandCards(cards);

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] firstcards:[%s]", __FUNCTION__, m_tid, seat.seatid, toCardsString(cards).c_str());
	}
//-------------
/*
	if (m_tableAttr.getPlayRound() == 0)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,11,12,12,12,13,13,13,13,14,16,17,21,21,21,22,22,22,23,23,23,23,24,25,26,31,31,31,32,32,32,33,33,34,34,35,35,36,18,18,18,18,11,12,14,14,15,15,16,16,17,
			14,15,15,16,17,17,19,19,19,19,21,22,24,24,24,25,25,25,26,26,26,27,27,27,27,28,28,28,28,29,29,29,29,
			31,32,33,33,34,34,35,35,36,36,36,37,37,37,37,38,38,38,38,39,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 1)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,12,12,13,13,14,14,21,21,22,22,23,21,21,22,22,23,23,14,14,31,31,32,32,33,31,31,32,32,11,11,12,12,24,24,25,25,35,13,13,15,15,15,16,16,16,26,18,19,28,39,
			15,16,17,17,17,17,18,18,18,19,19,19,23,24,24,25,25,26,26,26,27,27,27,27,28,28,28,29,29,29,29,33,33,
			33,34,34,34,34,35,35,35,36,36,36,36,37,37,37,37,38,38,38,38,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 2)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,12,12,12,12,14,14,21,21,22,22,23,21,21,22,22,23,23,14,14,31,31,32,32,33,31,31,32,32,11,11,13,13,24,24,25,25,35,13,13,15,15,15,16,16,16,26,18,19,28,39,
			15,16,17,17,17,17,18,18,18,19,19,19,23,24,24,25,25,26,26,26,27,27,27,27,28,28,28,29,29,29,29,33,33,
			33,34,34,34,34,35,35,35,36,36,36,36,37,37,37,37,38,38,38,38,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 3)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,12,12,13,13,14,14,15,15,16,16,17,21,21,22,22,23,23,14,14,31,31,32,32,33,31,31,32,32,11,11,12,12,24,24,25,25,35,13,13,21,21,15,16,22,22,26,18,19,28,39,
			15,16,17,17,17,18,18,18,19,19,19,23,23,24,24,25,25,26,26,26,27,27,27,27,28,28,28,29,29,29,29,33,33,
			33,34,34,34,34,35,35,35,36,36,36,36,37,37,37,37,38,38,38,38,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 4)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,12,12,12,12,14,14,14,14,15,15,16,21,21,22,22,23,23,21,21,31,31,32,32,33,31,31,32,32,11,11,13,13,24,24,25,25,35,13,13,22,22,15,23,16,16,26,18,19,28,39,
			15,16,17,17,17,17,18,18,18,19,19,19,23,24,24,25,25,26,26,26,27,27,27,27,28,28,28,29,29,29,29,33,33,
			33,34,34,34,34,35,35,35,36,36,36,36,37,37,37,37,38,38,38,38,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 5)
	{
		CARDS_t deckCards;

		int cards[] = {12,12,15,15,18,18,22,22,25,25,28,28,32,13,14,15,16,17,18,19,21,21,22,23,24,25,26,27,28,29,32,31,32,33,34,35,36,37,38,26,26,29,15,16,19,21,22,25,39,27,28,29,
			11,11,11,11,12,12,13,13,13,14,
			14,14,16,16,17,17,17,18,19,19,
			21,23,23,23,24,24,24,26,27,27,
			29,31,31,31,32,33,33,33,34,34,
			34,35,35,35,36,36,36,37,37,37,
			38,38,38,39,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 6)
	{
		CARDS_t deckCards;

		int cards[] = {32,32,31,31,31,33,33,33,34,35,36,37,38,
			11,12,13,14,15,16,17,18,19,21,22,23,24,
			26,27,28,28,29,22,25,26,26,29,15,15,16,
			12,12,18,19,14,37,37,35,35,34,34,39,39,

			11,11,11,12,13,13,13,14,14,15,
			16,16,17,17,17,18,18,19,19,21,
			21,21,22,22,23,23,23,24,24,24,
			25,25,25,26,27,27,27,28,28,29,
			29,31,32,32,33,34,35,36,36,36,
			37,38,38,38,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else if (m_tableAttr.getPlayRound() == 7)
	{
		CARDS_t deckCards;

		int cards[] = {11,11,11,21,21,17,17,18,18,25,25,39,39,
			21,17,25,26,28,15,16,16,22,22,27,27,35,
			18,25,27,32,33,33,26,26,15,15,19,19,34,
			11,33,34,31,27,28,13,13,19,19,35,38,36,

			12,12,12,12,13,13,14,14,14,14,
			15,16,16,17,18,21,22,22,23,23,
			23,23,24,24,24,24,26,28,28,29,
			29,29,29,31,31,31,32,32,32,33,
			34,34,35,35,36,36,36,37,37,37,
			37,38,38,38,39,39};
		for (int i = 0; i < sizeof(cards)/sizeof(int); ++i)
			deckCards.push_back(cards[i]);

		m_tableLocal.m_decks.fillCards(deckCards);
	}
	else
	{
		m_tableLocal.m_decks.fillCards();
	}
	
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		CARDS_t cards;
		m_tableLocal.m_decks.fetchCards(cards, 13);

		if (seat.seatid == m_tableLocal.m_bankerSeatId)
		{
			Card card;
			m_tableLocal.m_decks.fetchCard(card);
			cards.push_back(card);
		}

		m_tableLocal.getSeatLocal(seat.seatid).hands.addHandCards(cards);

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] firstcards:[%s]", __FUNCTION__, m_tid, seat.seatid, toCardsString(cards).c_str());
	}
*/
//----------

	// 牌局记录：初使化
	__recordInit();

	// 牌局记录：开始发牌
	__recordFirstcard(bankerType);

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_FIRSTCARD_UC;
		packet_r.val["play_round"] = m_tableAttr.getPlayRound() + 1;
		packet_r.val["banker_seatid"] = m_tableLocal.m_bankerSeatId;
		packet_r.val["banker_type"] = bankerType;
		packet_r.val["decks_count"] = m_tableLocal.m_decks.cardCount();

		for (int j = 0; j < MAX_SEAT; ++j)
		{
			SSeat& seat2 = m_seats[j];

			const CARDS_t& handCards = m_tableLocal.getSeatLocal(seat2.seatid).hands.getHandCards();

			Json::Value jsCards;
			jsCards["seatid"] = seat2.seatid;

			for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
			{
				if (seat2.seatid == seat.seatid)
					jsCards["cards"].append(c_iter->getVal());
				else
					jsCards["cards"].append(-1);
			}

			if (jsCards["cards"].size() < 14)
				jsCards["cards"].append(0);

			packet_r.val["player_cards"].append(jsCards);
		}

		packet_r.end();

		__sendPacketToSeatId(seat.seatid, packet_r.tostring());
	}

	__switchToState(S_FIRSTCARDS);

	__broadGameState();

	m_tableLocal.update();

	__startQiShouHu();
}

void CTable::__qishouHu()
{
	LOG(Info, "[CTable::%s] tid:[%d] qishouhu", __FUNCTION__, m_tid);

	bool bHasQiShouHu = false;

	// 牌局记录：起牌胡
	Json::Value jsRecord;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		CCheck check(m_tableLocal.getSeatLocal(seat.seatid).hands);

		// 牌局记录：起牌胡
		Json::Value jsSeat;
		jsSeat["seatid"] = seat.seatid;

		for (int j = 0; j < MAX_SEAT; ++j)
		{
			SSeat& seat2 = m_seats[j];

			int type = 0;

			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_FIRSTCARD_HU_BC;
			packet_r.val["seatid"] = seat.seatid;

			EATCARDS_t eatsSixi;
			if (check.isQiBaiHu_SiXi(eatsSixi))
			{
				if (eatsSixi.size() == 1)
					type |= QIPAIHU_SIXI;
				else if (eatsSixi.size() == 2)
					type |= QIPAIHU_SIXI_x2;
				else if (eatsSixi.size() == 3)
					type |= QIPAIHU_SIXI_x3;

				for (EATCARDS_t::iterator iter = eatsSixi.begin(); iter != eatsSixi.end(); ++iter)
				{
					Json::Value jsQiBaiHu;
					jsQiBaiHu["type"] = HU_TYPE_SIXI;

					CHands handsTmp;
					handsTmp.addHandCards( m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards() );

					SEatCard& eat = (*iter);

					Json::Value jsEat;
					jsEat["type"] = (int)eat.type;
					jsEat["first"] = eat.firstCard.getVal();
					jsEat["eat"] = eat.eatCard.getVal();

					jsQiBaiHu["eats"].append(jsEat);

					for (int i = 0; i < 4; ++i)
						handsTmp.delHandCard(eat.firstCard);

					const CARDS_t& handCardsTmp = handsTmp.getHandCards();

					int handCardsCount = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards().size();

					int zeroCount = 0;
					if (handCardsCount == 13)
						zeroCount = 13 - handCardsTmp.size();
					else
						zeroCount = 14 - handCardsTmp.size();

					for (int i = 0; i < zeroCount; ++i)
						jsQiBaiHu["hands"].append(0);

					for (CARDS_t::const_iterator c_iter = handCardsTmp.begin(); c_iter != handCardsTmp.end(); ++c_iter)
					{
						if (seat2.seatid == seat.seatid)
							jsQiBaiHu["hands"].append( c_iter->getVal() );
						else
							jsQiBaiHu["hands"].append( -1 );
					}

					if (handCardsCount == 13)
						jsQiBaiHu["hands"].append(0);

					//for (int i = 0; i < (int)eatsSixi.size(); ++i)
						packet_r.val["qibaihu"].append(jsQiBaiHu);

					if (seat2.seatid == seat.seatid)
					{
						//for (int i = 0; i < (int)eatsSixi.size(); ++i)
							jsSeat["qibaihu"].append(jsQiBaiHu);
					}
				}
			}

			if (check.isQiBaiHu_BanbanHu())
			{
				type |= QIPAIHU_BANBANHU;

				Json::Value jsQiBaiHu;
				jsQiBaiHu["type"] = HU_TYPE_BANBAN;

				const CARDS_t& handCards = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards();

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					//if (seat2.seatid == seat.seatid)
						jsQiBaiHu["hands"].append( c_iter->getVal() );
					//else
					//	jsQiBaiHu["hands"].append( -1 );
				}

				if (handCards.size() == 13)
					jsQiBaiHu["hands"].append(0);

				packet_r.val["qibaihu"].append(jsQiBaiHu);

				// 牌局记录：起牌胡
				if (seat2.seatid == seat.seatid)
				{
					jsSeat["qibaihu"].append(jsQiBaiHu);
				}
			}

			int queCount = 0;
			if (check.isQiBaiHu_QueYiSe(queCount))
			{
				if (queCount == 2)
					type |= QIPAIHU_QUEYISE_x2;
				else
					type |= QIPAIHU_QUEYISE;

				Json::Value jsQiBaiHu;
				jsQiBaiHu["type"] = HU_TYPE_QUEYISE;

				const CARDS_t& handCards = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards();

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					//if (seat2.seatid == seat.seatid)
						jsQiBaiHu["hands"].append( c_iter->getVal() );
					//else
					//	jsQiBaiHu["hands"].append( -1 );
				}

				if (handCards.size() == 13)
					jsQiBaiHu["hands"].append(0);

				for (int i = 0; i < queCount; ++i)
					packet_r.val["qibaihu"].append(jsQiBaiHu);

				// 牌局记录：起牌胡
				if (seat2.seatid == seat.seatid)
				{
					for (int i = 0; i < queCount; ++i)
						jsSeat["qibaihu"].append(jsQiBaiHu);
				}
			}

			EATCARDS_t eats66shu;
			if (check.isQiBaiHu_66Shun(eats66shu))
			{
				int shu66Count = (eats66shu.size() / 2);
				if (shu66Count == 2)
					type |= QIPAIHU_66SHUN_x2;
				else 
					type |= QIPAIHU_66SHUN;

				//for (EATCARDS_t::iterator iter = eats66shu.begin(); iter != eats66shu.end(); ++iter)
				for (int index = 0; index < shu66Count; ++index)
				{
					Json::Value jsQiBaiHu;
					jsQiBaiHu["type"] = HU_TYPE_66SHUN;

					CHands handsTmp;
					handsTmp.addHandCards( m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards() );

					{
						SEatCard& eat = eats66shu[index*2]; //(*iter);

						Json::Value jsEat;
						jsEat["type"] = (int)eat.type;
						jsEat["first"] = eat.firstCard.getVal();
						jsEat["eat"] = eat.eatCard.getVal();

						jsQiBaiHu["eats"].append(jsEat);

						for (int i = 0; i < 3; ++i)
							handsTmp.delHandCard(eat.firstCard);
					}
					{
						SEatCard& eat = eats66shu[index*2+1]; //(*iter);

						Json::Value jsEat;
						jsEat["type"] = (int)eat.type;
						jsEat["first"] = eat.firstCard.getVal();
						jsEat["eat"] = eat.eatCard.getVal();

						jsQiBaiHu["eats"].append(jsEat);

						for (int i = 0; i < 3; ++i)
							handsTmp.delHandCard(eat.firstCard);
					}

					const CARDS_t& handCardsTmp = handsTmp.getHandCards();

					int handCardsCount = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards().size();

					int zeroCount = 0;
					if (handCardsCount == 13)
						zeroCount = 13 - handCardsTmp.size();
					else
						zeroCount = 14 - handCardsTmp.size();

					for (int i = 0; i < zeroCount; ++i)
						jsQiBaiHu["hands"].append(0);

					for (CARDS_t::const_iterator c_iter = handCardsTmp.begin(); c_iter != handCardsTmp.end(); ++c_iter)
					{
						if (seat2.seatid == seat.seatid)
							jsQiBaiHu["hands"].append( c_iter->getVal() );
						else
							jsQiBaiHu["hands"].append( -1 );
					}

					if (handCardsCount == 13)
						jsQiBaiHu["hands"].append(0);

					//for (int i = 0; i < shu66Count; ++i)
						packet_r.val["qibaihu"].append(jsQiBaiHu);

					// 牌局记录：起牌胡
					if (seat2.seatid == seat.seatid)
					{
						//for (int i = 0; i < shu66Count; ++i)
							jsSeat["qibaihu"].append(jsQiBaiHu);
					}
				}
			}

			if (type != 0)
			{
				bHasQiShouHu = true;

				if (seat2.seatid == seat.seatid)
				{
					LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] qishouhu type:[0x%04x]", __FUNCTION__, m_tid, seat.seatid, type);

					m_tableLocal.getSeatLocal(seat.seatid).qibaihu = type;

					// 牌局记录：起牌胡
					jsRecord["qibaihus"].append(jsSeat);
				}

				packet_r.end();

				__sendPacketToSeatId(seat2.seatid, packet_r.tostring());
			}
		}
	}

	// 牌局记录：起牌胡
	__recordQibaihu(jsRecord);

	__switchToState(S_QISHOUHU);

	m_tableLocal.update();

	__startPlayCards(bHasQiShouHu);
}

void CTable::__playCards()
{
	m_tableLocal.m_sendCardSeatId = -1;
	m_tableLocal.m_sendCard = Card();

	m_tableLocal.m_putCardSeatId = m_tableLocal.m_bankerSeatId;
	m_tableLocal.m_putCard = Card();
	m_tableLocal.m_checkSelf = 1;

	LOG(Info, "[CTable::%s] tid:[%d] play, set putseatid:[%d] sendseatid:[%d]", __FUNCTION__, 
		m_tid, m_tableLocal.m_putCardSeatId, m_tableLocal.m_sendCardSeatId);

	__switchToState(S_PLAY);

	m_tableLocal.update();

	__putCardLogic();
}

void CTable::__putCardLogic()
{
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_CUR_PUTPLAYER_BC;
		packet_r.val["seatid"] = m_tableLocal.m_putCardSeatId;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId);

	const CARDS_t& tmpHandCards = seatLocal.hands.getHandCards();
	LOG(Info, "[CTable::%s] === tid:[%d] putcardseatid:[%d] handscards:[%s]", __FUNCTION__, m_tid, m_tableLocal.m_putCardSeatId, toCardsString(tmpHandCards).c_str());

	CCheck check(seatLocal.hands);

	bool bHu = false, bGang = false, bBu = false;
	EATCARDS_t eatHuCards, eatGangCards, eatBuCards;

	if (m_tableLocal.m_checkSelf == 1)
	{
		bHu = check.canHu(eatHuCards);

		if (seatLocal.ting == 0)
		{
			int decksCount = m_tableLocal.m_decks.cardCount();
			if (decksCount >= 3)
			{
				bGang = check.canGang(eatGangCards);
			}
			if (decksCount >= 2)
			{
				bBu = check.canBu(eatBuCards);
			}
		}
	}
	else if (m_tableLocal.m_checkSelf == 2)
	{
		if (seatLocal.ting == 0)
		{
			int decksCount = m_tableLocal.m_decks.cardCount();
			if (decksCount >= 3)
			{
				bGang = check.canGang(eatGangCards);
			}
			if (decksCount >= 2)
			{
				bBu = check.canBu(eatBuCards);
			}
		}
	}

	if (!bHu && !bGang && !bBu)
	{
		LOG(Info, "[CTable::%s] tid:[%d] putcardseatid:[%d] checkself:[%d] ting:[%d] can't hu|gang|bu, so notify putcard", __FUNCTION__, 
			m_tid, m_tableLocal.m_putCardSeatId, m_tableLocal.m_checkSelf, seatLocal.ting);

		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_NOTIFY_PUTCARD_UC;
		packet_r.end();

		__sendPacketToSeatId(m_tableLocal.m_putCardSeatId, packet_r.tostring());
		return;
	}


	if (bHu)
	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_HU];
		optionAction.type = ACTION_HU;
	}

	if (bGang)
	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_GANG];
		optionAction.type = ACTION_GANG;

		for (EATCARDS_t::iterator iter = eatGangCards.begin(); iter != eatGangCards.end(); ++iter)
		{
			CARDS_t cards;

			if (iter->type == ectGang)
			{
				cards.push_back(iter->firstCard);
			}
			else
			{
				for (int i = 0; i < 4; ++i)
					cards.push_back(iter->firstCard);
			}

			optionAction.arrCards.push_back(cards);
		}
	}

	if (bBu)
	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_BU];
		optionAction.type = ACTION_BU;

		for (EATCARDS_t::iterator iter = eatBuCards.begin(); iter != eatBuCards.end(); ++iter)
		{
			CARDS_t cards;

			if (iter->type == ectGang)
			{
				cards.push_back(iter->firstCard);
			}
			else
			{
				for (int i = 0; i < 4; ++i)
					cards.push_back(iter->firstCard);
			}

			optionAction.arrCards.push_back(cards);
		}
	}

	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_PASS];
		optionAction.type = ACTION_PASS;
	}

	__sendOptionActions(m_tableLocal.m_putCardSeatId, seatLocal.optionActions);

	m_tableLocal.update();
}

void CTable::__sendCardLogic()
{
	int decksCount = m_tableLocal.m_decks.cardCount();

	if (decksCount == 0)
	{
		LOG(Info, "[CTable::%s] tid:[%d] deck empty, call __liujuLogic()", __FUNCTION__, m_tid);

		__liujuLogic();
		return;
	}

	if (decksCount == 1)
	{
		{
			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_HAIDICARD_BC;
			packet_r.end();

			__broadPacket(NULL, packet_r.tostring());
		}

		LOG(Info, "[CTable::%s] tid:[%d] haidi card, call __haidiCardLogic()", __FUNCTION__, m_tid);

		__haidiCardLogic();
		return;
	}

	Card card;
	m_tableLocal.m_decks.fetchCard(card);
	m_tableLocal.getSeatLocal(m_tableLocal.m_sendCardSeatId).hands.addHandCard(card);
	m_tableLocal.m_sendCard = card;

	LOG(Info, "[CTable::%s] tid:[%d] sendcardseatid:[%d] card:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId, card.getVal());

	// 牌局记录：玩家发牌
	__recordSendcard(m_tableLocal.m_sendCardSeatId, card, m_tableLocal.m_decks.cardCount());

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_SEND_CARD_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.val["card"] = (seat.seatid == m_tableLocal.m_sendCardSeatId) ? card.getVal() : -1;
		packet_r.val["decks_count"] = m_tableLocal.m_decks.cardCount();
		packet_r.end();

		__sendPacketToSeatId(seat.seatid, packet_r.tostring());
	}

	m_tableLocal.m_putCardSeatId = m_tableLocal.m_sendCardSeatId;
	m_tableLocal.m_putCard = Card();
	m_tableLocal.m_checkSelf = 1;

	m_tableLocal.update();

	__putCardLogic();
}

void CTable::__gangSendCardLogic()
{
	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

	CARDS_t cards;
	m_tableLocal.m_decks.fetchCards(cards, 2);

	if (cards.size() == 0)
	{
		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d], deck empty, so call __liujuLogic()", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

		__liujuLogic();
		return;
	}

	// 牌局记录：杠上发牌
	__recordGangcard(m_tableLocal.m_sendCardSeatId,  cards.size() < 1 ? 0 : cards[0],  cards.size() < 2 ? 0 : cards[1], m_tableLocal.m_decks.cardCount());

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_GANG_SEND_CARD_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.val["cards"].append( cards.size() < 1 ? 0 : cards[0].getVal()  );
		packet_r.val["cards"].append( cards.size() < 2 ? 0 : cards[1].getVal() );
		packet_r.val["decks_count"] = m_tableLocal.m_decks.cardCount();
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	m_tempVar.gangCards = cards;

	__startGangHuAsk();
}

void CTable::__gangHuAskLogic()
{
	LOG(Info, "[CTable::%s] tid:[%d] ...", __FUNCTION__, m_tid);

	CARDS_t cards = m_tempVar.gangCards;

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_sendCardSeatId);

	for (CARDS_t::iterator iter = cards.begin(); iter != cards.end(); ++iter)
	{
		CCheck check(seatLocal.hands);

		EATCARDS_t eatCards;
		if (check.canHu(*iter, eatCards))
		{
			LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] gang hu card:[%d], send notify", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId, iter->getVal());

			SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_sendCardSeatId);
			{
				SOptionAction& optionAction = seatLocal.optionActions[ACTION_GANGHU];
				optionAction.type = ACTION_GANGHU;
				CARDS_t cards;
				cards.push_back(*iter);
				optionAction.arrCards.push_back(cards);
			}

			__sendOptionActions(m_tableLocal.m_sendCardSeatId, seatLocal.optionActions);
			return;
		}
	}

	bool bOptionAction = false;
	int nextSeatId = m_tableLocal.m_sendCardSeatId;

	for (int i = 1; i < MAX_SEAT; ++i)
	{
		nextSeatId = SSeat::getNextSeatid(nextSeatId);
		SSeatLocal& seatLocalN = m_tableLocal.getSeatLocal(nextSeatId);

		for (CARDS_t::iterator iter = cards.begin(); iter != cards.end(); ++iter)
		{
			CCheck check(seatLocalN.hands);

			EATCARDS_t eatCards;
			if (check.canHu(*iter, eatCards))
			{
				LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] gang hu card:[%d], send notify", __FUNCTION__, m_tid, nextSeatId, iter->getVal());

				{
					SOptionAction& optionAction = seatLocalN.optionActions[ACTION_GANGHU];
					optionAction.type = ACTION_GANGHU;
					CARDS_t cards;
					cards.push_back(*iter);
					optionAction.arrCards.push_back(cards);
				}

				bOptionAction = true;
				__sendOptionActions(nextSeatId, seatLocalN.optionActions);
				break;
			}
		}
	}

	if (!bOptionAction)
	{
		m_tableLocal.m_sendCardSeatId = SSeat::getNextSeatid(m_tableLocal.m_sendCardSeatId);
		m_tableLocal.m_sendCard = Card();
		m_tableLocal.m_sendCardFlag = 0;

		m_tableLocal.update();

		m_tempVar.gangCards.clear();

		__sendCardLogic();
	}
}

void CTable::__haidiCardLogic()
{
	LOG(Info, "[CTable::%s] tid:[%d] notify sendcardseatid:[%d] ACTION_HAIDI_YAO|ACTION_HAIDI_BUYAO", __FUNCTION__,
		m_tid, m_tableLocal.m_sendCardSeatId);

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_HAIDI_ASK_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_sendCardSeatId);
	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_HAIDI_YAO];
		optionAction.type = ACTION_HAIDI_YAO;
	}
	{
		SOptionAction& optionAction = seatLocal.optionActions[ACTION_HAIDI_BUYAO];
		optionAction.type = ACTION_HAIDI_BUYAO;
	}

	__sendOptionActions(m_tableLocal.m_sendCardSeatId, seatLocal.optionActions);
}

bool CTable::__checkMyGangBuButOtherHu(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	if (m_tableLocal.m_putCard.isValid())
		return false;

	std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin();

	int actionSeatId = c_iter->first;
	const SSelectAction& selectAction = c_iter->second;

	if (selectAction.cards.size() != 1)
		return false;

	bool bHasHuAction = false;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (seat.seatid == actionSeatId)
			continue;

		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		CCheck check(seatLocal.hands);

		EATCARDS_t eatHuCards;
		if (check.canHu(selectAction.cards[0], eatHuCards))
		{
			if (seatLocal.selectAction.type != 0)
				continue;

			if (selectAction.type == ACTION_BU)
			{
				SOptionAction& optionAction = seatLocal.optionActions[ACTION_QIANGBUHU];
				optionAction.type = ACTION_QIANGBUHU;
				CARDS_t cards;
				cards.push_back(selectAction.cards[0]);
				optionAction.arrCards.push_back( cards );
			}
			else if (selectAction.type == ACTION_GANG)
			{
				SOptionAction& optionAction = seatLocal.optionActions[ACTION_QIANGGANGHU];
				optionAction.type = ACTION_QIANGGANGHU;
				CARDS_t cards;
				cards.push_back(selectAction.cards[0]);
				optionAction.arrCards.push_back( cards );
			}

			SOptionAction& optionAction = seatLocal.optionActions[ACTION_PASS];
			optionAction.type = ACTION_PASS;

			LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] qiang gang|bu", __FUNCTION__, m_tid, seat.seatid);

			__sendOptionActions(seat.seatid, seatLocal.optionActions);

			bHasHuAction = true;
		}
	}
	
	return bHasHuAction;
}

void CTable::__passCurAction()
{
	if (!m_tableLocal.m_putCard.isValid())
	{
		LOG(Info, "[CTable::%s] tid:[%d] notify putcardseatid:[%d] putcard", __FUNCTION__, m_tid, m_tableLocal.m_putCardSeatId);

		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_NOTIFY_PUTCARD_UC;
		packet_r.end();

		__sendPacketToSeatId(m_tableLocal.m_putCardSeatId, packet_r.tostring());
	}

	else
	{
		m_tableLocal.m_sendCardSeatId = SSeat::getNextSeatid(m_tableLocal.m_putCardSeatId);
		m_tableLocal.m_sendCard = Card();
		m_tableLocal.m_sendCardFlag = 0;

		LOG(Info, "[CTable::%s] tid:[%d] will sendcardseatid:[%d]", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

		m_tableLocal.update();

		__sendCardLogic();
	}
}

void CTable::__haidiBuYaoLogic()
{
	LOG(Info, "[CTable::%s] tid:[%d] sendcardseatid:[%d] select ACTION_HAIDI_BUYAO", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

	__recordAction(m_tableLocal.m_sendCardSeatId, ACTION_HAIDI_BUYAO);

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_HAIDI_ACK_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.val["ack"] = 0;
		packet_r.val["force"] = 0;
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	int nextSeatId = SSeat::getNextSeatid(m_tableLocal.m_sendCardSeatId);
	if (nextSeatId != SSeat::getNextSeatid(m_tableLocal.m_putCardSeatId))
	{
		m_tableLocal.m_sendCardSeatId = nextSeatId;
		m_tableLocal.m_sendCard = Card();
		m_tableLocal.m_sendCardFlag = 0;

		__haidiCardLogic();
		return;
	}

	LOG(Info, "[CTable::%s] tid:[%d] nobody YAO haidi, so call __liujuLogic()", __FUNCTION__, m_tid);

	__liujuLogic();
}

void CTable::__haidiYaoCardLogic(bool bForce)
{
	LOG(Info, "[CTable::%s] tid:[%d] sendcardseatid:[%d] select ACTION_HAIDI_YAO", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

	__recordAction(m_tableLocal.m_sendCardSeatId, ACTION_HAIDI_YAO);

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_HAIDI_ACK_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.val["ack"] = 1;
		packet_r.val["force"] = (bForce ? 1 : 0);
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	Card card;
	m_tableLocal.m_decks.fetchCard(card);
	m_tableLocal.m_sendCard = card;

	// 牌局记录：海底牌
	__recordHaidi(m_tableLocal.m_sendCardSeatId, card);

	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_HAIDI_SEND_CARD_BC;
		packet_r.val["seatid"] = m_tableLocal.m_sendCardSeatId;
		packet_r.val["card"] = card.getVal();
		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());
	}

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(m_tableLocal.m_sendCardSeatId);

	CCheck check(seatLocal.hands);

	EATCARDS_t eatCards;
	if (check.canHu(card, eatCards))
	{
		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] haidi hu card:[%d], send notify", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId, card.getVal());

		{
			SOptionAction& optionAction = seatLocal.optionActions[ACTION_HAIDIHU];
			optionAction.type = ACTION_HAIDIHU;
			CARDS_t cards;
			cards.push_back(card);
			optionAction.arrCards.push_back(cards);
		}

		__sendOptionActions(m_tableLocal.m_sendCardSeatId, seatLocal.optionActions);
		return;
	}

	bool bOptionAction = false;
	int nextSeatId = m_tableLocal.m_sendCardSeatId;

	for (int i = 1; i < MAX_SEAT; ++i)
	{
		nextSeatId = SSeat::getNextSeatid(nextSeatId);
		SSeatLocal& seatLocalN = m_tableLocal.getSeatLocal(nextSeatId);

		CCheck check(seatLocalN.hands);

		EATCARDS_t eatCards;
		if (check.canHu(card, eatCards))
		{
			LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] haidi hu card:[%d], send notify", __FUNCTION__, m_tid, nextSeatId, card.getVal());

			{
				SOptionAction& optionAction = seatLocalN.optionActions[ACTION_HAIDIHU];
				optionAction.type = ACTION_HAIDIHU;
				CARDS_t cards;
				cards.push_back(card);
				optionAction.arrCards.push_back(cards);
			}

			bOptionAction = true;
			__sendOptionActions(nextSeatId, seatLocalN.optionActions);
		}
	}

	if (!bOptionAction)
	{
		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] no player can haidihu, so call __liujuLogic()", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId);

		__liujuLogic();
	}
}

void CTable::__chiCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin();

	int actionSeatId = c_iter->first;
	const SSelectAction& selectAction = c_iter->second;

	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, actionSeatId, selectAction.toString().c_str());

	__recordAction(actionSeatId, selectAction.type);

	m_tempVar.cleanPassHu(actionSeatId);

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(actionSeatId);

	CARDS_t sortCards;
	sortCards.push_back( m_tableLocal.m_putCard );
	std::copy(selectAction.cards.begin(), selectAction.cards.end(), std::back_inserter(sortCards));
	std::sort(sortCards.begin(), sortCards.end(), Card_Lesser);

	m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId).hands.popOutCard();

	for (CARDS_t::const_iterator c_iter = selectAction.cards.begin(); c_iter != selectAction.cards.end(); ++c_iter)
	{
		seatLocal.hands.delHandCard( *c_iter );
	}

	SEatCard eatAdd;
	{
		eatAdd.type = ectEat;
		eatAdd.firstCard = sortCards[0];
		eatAdd.eatCard = m_tableLocal.m_putCard;
		seatLocal.hands.addEatCard(eatAdd);
	}

	{
		const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

		for (int i = 0; i < MAX_SEAT; ++i)
		{
			SSeat& seat = m_seats[i];

			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
			packet_r.val["from_seatid"] = m_tableLocal.m_putCardSeatId;
			packet_r.val["from_card"] = m_tableLocal.m_putCard.getVal();
			packet_r.val["to_seatid"] = actionSeatId;
			packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
			packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			packet_r.val["to_eatcard_del"]["type"] = -1;
			packet_r.val["to_eatcard_del"]["first"] = -1;
			packet_r.val["to_eatcard_del"]["eat"] = -1;

			int zeroCount = 14 - handCards.size();
			for (int i = 0; i < zeroCount; ++i)
				packet_r.val["to_hands"].append(0);

			for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
			{
				if (seat.seatid == actionSeatId)
					packet_r.val["to_hands"].append(c_iter->getVal());
				else
					packet_r.val["to_hands"].append(-1);
			}

			packet_r.end();

			__sendPacketToSeatId(seat.seatid, packet_r.tostring());
		}

		// 牌局记录：玩家操作结果
		Json::Value jsRecord;
		jsRecord["from_seatid"] = m_tableLocal.m_putCardSeatId;
		jsRecord["from_card"] = m_tableLocal.m_putCard.getVal();
		jsRecord["to_seatid"] = actionSeatId;
		jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
		jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
		jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
		jsRecord["to_eatcard_del"]["type"] = -1;
		jsRecord["to_eatcard_del"]["first"] = -1;
		jsRecord["to_eatcard_del"]["eat"] = -1;
		{
			int zeroCount = 14 - handCards.size();
			for (int i = 0; i < zeroCount; ++i)
				jsRecord["to_hands"].append(0);

			for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
			{
				jsRecord["to_hands"].append(c_iter->getVal());
			}
		}
		__recordResult(jsRecord);
	}

	m_tableLocal.m_putCardSeatId = actionSeatId;
	m_tableLocal.m_putCard = Card();
	m_tableLocal.m_checkSelf = 2;

	m_tableLocal.update();

	__putCardLogic();
}

void CTable::__pengCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin();

	int actionSeatId = c_iter->first;
	const SSelectAction& selectAction = c_iter->second;

	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, actionSeatId, selectAction.toString().c_str());

	__recordAction(actionSeatId, selectAction.type);

	m_tempVar.cleanPassHu(actionSeatId);

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(actionSeatId);

	m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId).hands.popOutCard();

	seatLocal.hands.delHandCard( m_tableLocal.m_putCard );
	seatLocal.hands.delHandCard( m_tableLocal.m_putCard );

	SEatCard eatAdd;
	{
		eatAdd.type = ectPeng;
		eatAdd.firstCard = m_tableLocal.m_putCard;
		eatAdd.eatCard = m_tableLocal.m_putCard;
		seatLocal.hands.addEatCard(eatAdd);
	}

	{
		const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

		for (int i = 0; i < MAX_SEAT; ++i)
		{
			SSeat& seat = m_seats[i];

			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
			packet_r.val["from_seatid"] = m_tableLocal.m_putCardSeatId;
			packet_r.val["from_card"] = m_tableLocal.m_putCard.getVal();
			packet_r.val["to_seatid"] = actionSeatId;
			packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
			packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			packet_r.val["to_eatcard_del"]["type"] = -1;
			packet_r.val["to_eatcard_del"]["first"] = -1;
			packet_r.val["to_eatcard_del"]["eat"] = -1;

			int zeroCount = 14 - handCards.size();
			for (int i = 0; i < zeroCount; ++i)
				packet_r.val["to_hands"].append(0);

			for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
			{
				if (seat.seatid == actionSeatId)
					packet_r.val["to_hands"].append(c_iter->getVal());
				else
					packet_r.val["to_hands"].append(-1);
			}

			packet_r.end();

			__sendPacketToSeatId(seat.seatid, packet_r.tostring());
		}

		// 牌局记录：玩家操作结果
		Json::Value jsRecord;
		jsRecord["from_seatid"] = m_tableLocal.m_putCardSeatId;
		jsRecord["from_card"] = m_tableLocal.m_putCard.getVal();
		jsRecord["to_seatid"] = actionSeatId;
		jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
		jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
		jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
		jsRecord["to_eatcard_del"]["type"] = -1;
		jsRecord["to_eatcard_del"]["first"] = -1;
		jsRecord["to_eatcard_del"]["eat"] = -1;
		{
			int zeroCount = 14 - handCards.size();
			for (int i = 0; i < zeroCount; ++i)
				jsRecord["to_hands"].append(0);

			for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
			{
				jsRecord["to_hands"].append(c_iter->getVal());
			}
		}
		__recordResult(jsRecord);
	}

	m_tableLocal.m_putCardSeatId = actionSeatId;
	m_tableLocal.m_putCard = Card();
	m_tableLocal.m_checkSelf = 2;

	m_tableLocal.update();

	__putCardLogic();
}

void CTable::__gangCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin();

	int actionSeatId = c_iter->first;
	const SSelectAction& selectAction = c_iter->second;

	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, actionSeatId, selectAction.toString().c_str());

	__recordAction(actionSeatId, selectAction.type);

	m_tempVar.cleanPassHu(actionSeatId);

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(actionSeatId);

	if (!m_tableLocal.m_putCard.isValid())
	{
		for (CARDS_t::const_iterator c_iter = selectAction.cards.begin(); c_iter != selectAction.cards.end(); ++c_iter)
		{
			seatLocal.hands.delHandCard(*c_iter);
		}

		SEatCard eatDel;
		if (selectAction.cards.size() == 1)
		{
			eatDel.type = ectPeng;
			eatDel.firstCard = selectAction.cards[0];
			eatDel.eatCard = selectAction.cards[0];
			seatLocal.hands.delEatCard(eatDel);
		}

		SEatCard eatAdd;
		{
			eatAdd.type = (selectAction.cards.size() == 1 ? ectGang : ectAnGang);
			eatAdd.firstCard = selectAction.cards[0];
			eatAdd.eatCard = selectAction.cards[0];
			seatLocal.hands.addEatCard(eatAdd);
		}

		{
			const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

			for (int i = 0; i < MAX_SEAT; ++i)
			{
				SSeat& seat = m_seats[i];

				Jpacket packet_r;
				packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
				packet_r.val["from_seatid"] = -1;
				packet_r.val["from_card"] = -1;
				packet_r.val["to_seatid"] = actionSeatId;
				packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
				packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
				packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
				packet_r.val["to_eatcard_add"]["isbu"] = 0;
				packet_r.val["to_eatcard_del"]["type"] = (int)eatDel.type;
				packet_r.val["to_eatcard_del"]["first"] = eatDel.firstCard.getVal();
				packet_r.val["to_eatcard_del"]["eat"] = eatDel.eatCard.getVal();

				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					packet_r.val["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					if (seat.seatid == actionSeatId)
						packet_r.val["to_hands"].append(c_iter->getVal());
					else
						packet_r.val["to_hands"].append(-1);
				}

				packet_r.val["to_hands"].append(0);

				packet_r.end();

				__sendPacketToSeatId(seat.seatid, packet_r.tostring());
			}

			// 牌局记录：玩家操作结果
			Json::Value jsRecord;
			jsRecord["from_seatid"] = -1;
			jsRecord["from_card"] = -1;
			jsRecord["to_seatid"] = actionSeatId;
			jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
			jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			jsRecord["to_eatcard_add"]["isbu"] = 0;
			jsRecord["to_eatcard_del"]["type"] = (int)eatDel.type;
			jsRecord["to_eatcard_del"]["first"] = eatDel.firstCard.getVal();
			jsRecord["to_eatcard_del"]["eat"] = eatDel.eatCard.getVal();
			{
				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					jsRecord["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					jsRecord["to_hands"].append(c_iter->getVal());
				}

				jsRecord["to_hands"].append(0);
			}
			__recordResult(jsRecord);
		}
	}
	else
	{
		m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId).hands.popOutCard();

		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );
		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );
		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );

		SEatCard eatAdd;
		{
			eatAdd.type = ectGang;
			eatAdd.firstCard = m_tableLocal.m_putCard;
			eatAdd.eatCard = m_tableLocal.m_putCard;
			seatLocal.hands.addEatCard(eatAdd);
		}

		{
			const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

			for (int i = 0; i < MAX_SEAT; ++i)
			{
				SSeat& seat = m_seats[i];

				Jpacket packet_r;
				packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
				packet_r.val["from_seatid"] = m_tableLocal.m_putCardSeatId;
				packet_r.val["from_card"] = m_tableLocal.m_putCard.getVal();
				packet_r.val["to_seatid"] = actionSeatId;
				packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
				packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
				packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
				packet_r.val["to_eatcard_add"]["isbu"] = 0;
				packet_r.val["to_eatcard_del"]["type"] = -1;
				packet_r.val["to_eatcard_del"]["first"] = -1;
				packet_r.val["to_eatcard_del"]["eat"] = -1;

				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					packet_r.val["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					if (seat.seatid == actionSeatId)
						packet_r.val["to_hands"].append(c_iter->getVal());
					else
						packet_r.val["to_hands"].append(-1);
				}

				packet_r.val["to_hands"].append(0);

				packet_r.end();

				__sendPacketToSeatId(seat.seatid, packet_r.tostring());
			}

			// 牌局记录：玩家操作结果
			Json::Value jsRecord;
			jsRecord["from_seatid"] = m_tableLocal.m_putCardSeatId;
			jsRecord["from_card"] = m_tableLocal.m_putCard.getVal();
			jsRecord["to_seatid"] = actionSeatId;
			jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
			jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			jsRecord["to_eatcard_add"]["isbu"] = 0;
			jsRecord["to_eatcard_del"]["type"] = -1;
			jsRecord["to_eatcard_del"]["first"] = -1;
			jsRecord["to_eatcard_del"]["eat"] = -1;
			{
				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					jsRecord["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					jsRecord["to_hands"].append(c_iter->getVal());
				}

				jsRecord["to_hands"].append(0);
			}
			__recordResult(jsRecord);
		}
	}

	seatLocal.ting = 1;

	m_tableLocal.m_sendCardSeatId = actionSeatId;
	m_tableLocal.m_sendCard = Card();
	m_tableLocal.m_sendCardFlag = 1;
	
	m_tableLocal.update();

	__gangSendCardLogic();
}

void CTable::__buCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin();

	int actionSeatId = c_iter->first;
	const SSelectAction& selectAction = c_iter->second;

	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, actionSeatId, selectAction.toString().c_str());

	__recordAction(actionSeatId, selectAction.type);

	m_tempVar.cleanPassHu(actionSeatId);

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(actionSeatId);

	if (!m_tableLocal.m_putCard.isValid())
	{
		for (CARDS_t::const_iterator c_iter = selectAction.cards.begin(); c_iter != selectAction.cards.end(); ++c_iter)
		{
			seatLocal.hands.delHandCard(*c_iter);
		}

		SEatCard eatDel;
		if (selectAction.cards.size() == 1)
		{
			eatDel.type = ectPeng;
			eatDel.firstCard = selectAction.cards[0];
			eatDel.eatCard = selectAction.cards[0];
			seatLocal.hands.delEatCard(eatDel);
		}

		SEatCard eatAdd;
		{
			eatAdd.type = (selectAction.cards.size() == 1 ? ectGang : ectAnGang);
			eatAdd.firstCard = selectAction.cards[0];
			eatAdd.eatCard = selectAction.cards[0];
			seatLocal.hands.addEatCard(eatAdd);
		}

		{
			const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

			for (int i = 0; i < MAX_SEAT; ++i)
			{
				SSeat& seat = m_seats[i];

				Jpacket packet_r;
				packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
				packet_r.val["from_seatid"] = -1;
				packet_r.val["from_card"] = -1;
				packet_r.val["to_seatid"] = actionSeatId;
				packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
				packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
				packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
				packet_r.val["to_eatcard_add"]["isbu"] = 1;
				packet_r.val["to_eatcard_del"]["type"] = (int)eatDel.type;
				packet_r.val["to_eatcard_del"]["first"] = eatDel.firstCard.getVal();
				packet_r.val["to_eatcard_del"]["eat"] = eatDel.eatCard.getVal();

				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					packet_r.val["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					if (seat.seatid == actionSeatId)
						packet_r.val["to_hands"].append(c_iter->getVal());
					else
						packet_r.val["to_hands"].append(-1);
				}

				packet_r.val["to_hands"].append(0);

				packet_r.end();

				__sendPacketToSeatId(seat.seatid, packet_r.tostring());
			}

			// 牌局记录：玩家操作结果
			Json::Value jsRecord;
			jsRecord["from_seatid"] = -1;
			jsRecord["from_card"] = -1;
			jsRecord["to_seatid"] = actionSeatId;
			jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
			jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			jsRecord["to_eatcard_add"]["isbu"] = 1;
			jsRecord["to_eatcard_del"]["type"] = (int)eatDel.type;
			jsRecord["to_eatcard_del"]["first"] = eatDel.firstCard.getVal();
			jsRecord["to_eatcard_del"]["eat"] = eatDel.eatCard.getVal();
			{
				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					jsRecord["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					jsRecord["to_hands"].append(c_iter->getVal());
				}

				jsRecord["to_hands"].append(0);
			}
			__recordResult(jsRecord);
		}
	}
	else
	{
		m_tableLocal.getSeatLocal(m_tableLocal.m_putCardSeatId).hands.popOutCard();

		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );
		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );
		seatLocal.hands.delHandCard( m_tableLocal.m_putCard );

		SEatCard eatAdd;
		{
			eatAdd.type = ectGang;
			eatAdd.firstCard = m_tableLocal.m_putCard;
			eatAdd.eatCard = m_tableLocal.m_putCard;
			seatLocal.hands.addEatCard(eatAdd);
		}

		{
			const CARDS_t& handCards = m_tableLocal.getSeatLocal(actionSeatId).hands.getHandCards();

			for (int i = 0; i < MAX_SEAT; ++i)
			{
				SSeat& seat = m_seats[i];

				Jpacket packet_r;
				packet_r.val["cmd"] = SERVER_ACTION_RESULT_BC;
				packet_r.val["from_seatid"] = m_tableLocal.m_putCardSeatId;
				packet_r.val["from_card"] = m_tableLocal.m_putCard.getVal();
				packet_r.val["to_seatid"] = actionSeatId;
				packet_r.val["to_eatcard_add"]["type"] = (int)eatAdd.type;
				packet_r.val["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
				packet_r.val["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
				packet_r.val["to_eatcard_add"]["isbu"] = 1;
				packet_r.val["to_eatcard_del"]["type"] = -1;
				packet_r.val["to_eatcard_del"]["first"] = -1;
				packet_r.val["to_eatcard_del"]["eat"] = -1;

				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					packet_r.val["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					if (seat.seatid == actionSeatId)
						packet_r.val["to_hands"].append(c_iter->getVal());
					else
						packet_r.val["to_hands"].append(-1);
				}

				packet_r.val["to_hands"].append(0);

				packet_r.end();

				__sendPacketToSeatId(seat.seatid, packet_r.tostring());
			}

			// 牌局记录：玩家操作结果
			Json::Value jsRecord;
			jsRecord["from_seatid"] = m_tableLocal.m_putCardSeatId;
			jsRecord["from_card"] = m_tableLocal.m_putCard.getVal();
			jsRecord["to_seatid"] = actionSeatId;
			jsRecord["to_eatcard_add"]["type"] = (int)eatAdd.type;
			jsRecord["to_eatcard_add"]["first"] = eatAdd.firstCard.getVal();
			jsRecord["to_eatcard_add"]["eat"] = eatAdd.eatCard.getVal();
			jsRecord["to_eatcard_add"]["isbu"] = 1;
			jsRecord["to_eatcard_del"]["type"] = -1;
			jsRecord["to_eatcard_del"]["first"] = -1;
			jsRecord["to_eatcard_del"]["eat"] = -1;
			{
				int zeroCount = 13 - handCards.size();
				for (int i = 0; i < zeroCount; ++i)
					jsRecord["to_hands"].append(0);

				for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
				{
					jsRecord["to_hands"].append(c_iter->getVal());
				}

				jsRecord["to_hands"].append(0);
			}
			__recordResult(jsRecord);
		}
	}

	m_tableLocal.m_sendCardSeatId = actionSeatId;
	m_tableLocal.m_sendCard = Card();
	m_tableLocal.m_sendCardFlag = 0;

	m_tableLocal.update();

	__sendCardLogic();
}

// 胡牌逻辑
void CTable::__huCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	{
		int huSeatId = c_iter->first;
		const SSelectAction& selectAction = c_iter->second;

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, huSeatId, selectAction.toString().c_str());

		__recordAction(huSeatId, selectAction.type);

		__makeBalanceInfo(huSeatId, selectAction);
	}

	//std::vector<int> vecHuSeatId;
	//for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	//	vecHuSeatId.push_back(c_iter->first);
	__commonHuCalls(/*vecHuSeatId, */true);
}

void CTable::__ganghuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	{
		int huSeatId = c_iter->first;
		const SSelectAction& selectAction = c_iter->second;

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, huSeatId, selectAction.toString().c_str());

		__recordAction(huSeatId, selectAction.type);

		__makeGangBalanceInfo(huSeatId, selectAction);
	}

	//std::vector<int> vecHuSeatId;
	//for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	//	vecHuSeatId.push_back(c_iter->first);
	__commonHuCalls(/*vecHuSeatId, */true);
}

void CTable::__haidihuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	{
		int huSeatId = c_iter->first;
		const SSelectAction& selectAction = c_iter->second;

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, huSeatId, selectAction.toString().c_str());

		__recordAction(huSeatId, selectAction.type);

		__makeHaidiBalanceInfo(huSeatId, selectAction);
	}

	//std::vector<int> vecHuSeatId;
	//for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	//	vecHuSeatId.push_back(c_iter->first);
	__commonHuCalls(/*vecHuSeatId, */false);
}

void CTable::__qiangganghuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	{
		int huSeatId = c_iter->first;
		const SSelectAction& selectAction = c_iter->second;

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, huSeatId, selectAction.toString().c_str());

		__recordAction(huSeatId, selectAction.type);

		__makeQiangGangBalanceInfo(huSeatId, selectAction);
	}

	//std::vector<int> vecHuSeatId;
	//for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	//	vecHuSeatId.push_back(c_iter->first);
	__commonHuCalls(/*vecHuSeatId, */true);
}

void CTable::__qiangbuhuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction)
{
	for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	{
		int huSeatId = c_iter->first;
		const SSelectAction& selectAction = c_iter->second;

		LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] action:[%s]", __FUNCTION__, m_tid, huSeatId, selectAction.toString().c_str());

		__recordAction(huSeatId, selectAction.type);

		__makeQiangBuBalanceInfo(huSeatId, selectAction);
	}

	//std::vector<int> vecHuSeatId;
	//for (std::map<int, SSelectAction>::const_iterator c_iter = mapSeatSelectAction.begin(); c_iter != mapSeatSelectAction.end(); ++c_iter)
	//	vecHuSeatId.push_back(c_iter->first);
	__commonHuCalls(/*vecHuSeatId, */true);
}

void CTable::__commonHuCalls(/*const std::vector<int>& vecHuSeatId, */bool bZhaniao)
{
	__cleanPlayerReady();

	__broadShowCards(/*vecHuSeatId*/);

	if (bZhaniao)
	{
		__calcZhaniao(false);
	}

	__broadBalance();

	__switchToState(S_END);

	__broadGameState();

	m_tableLocal.update();

	// 局数+1
	m_tableAttr.incPlayRound(1);

	__startGoonRound();
}

void CTable::__liujuLogic()
{
	LOG(Info, "[CTable::%s] tid:[%d] ", __FUNCTION__, m_tid);

	m_tableLocal.m_balanceInfo.isWin = false;

	__cleanPlayerReady();

	__broadShowCards(/*std::vector<int>()*/);

	__broadBalance();

	__switchToState(S_END);

	__broadGameState();

	m_tableLocal.update();

	m_tableAttr.incPlayRound(1);

	__startGoonRound();
}

void CTable::__calcZhaniao(bool gang)
{
	LOG(Info, "[CTable::%s] tid:[%d] gang:[%d]", __FUNCTION__, m_tid, gang);

	int fromSeatid = m_tableLocal.m_bankerSeatId;

	if (m_tableLocal.m_balanceInfo.mapSeatHuType.size() > 1)
	{
		fromSeatid = m_tableLocal.m_balanceInfo.cardSeatId;
	}
	else
	{
		std::map<int, std::set<int> >::iterator iterHuSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin();

		fromSeatid = iterHuSeatId->first;
	}

	// 根据桌子配置，决定扎鸟牌数
	int zhaniaoCount = m_tableAttr.getZhaniaoCount();
	//CARDS_t zhaniaoCards = CDecks::randomCards(zhaniaoCount);

	//LOG(Info, "[CTable::%s] tid:[%d] zhaniaoCount:[%d] zhaniaoCards:[%s]", __FUNCTION__, m_tid, zhaniaoCount, toCardsString(zhaniaoCards).c_str());

	//for (CARDS_t::iterator iterCard = zhaniaoCards.begin(); iterCard != zhaniaoCards.end(); ++iterCard)
	//{
	//	int modNumber = (iterCard->getPoint() - 1) % MAX_SEAT;

	//	int zhaniaoSeatId = fromSeatid;//m_tableLocal.m_bankerSeatId;

	//	for (int i = 0; i < modNumber; ++i)
	//	{
	//		zhaniaoSeatId = SSeat::getNextSeatid(zhaniaoSeatId);
	//	}

	//	// 记录扎鸟
	//	m_tempVar.vecZhaniao.push_back( std::make_pair(iterCard->getVal(), zhaniaoSeatId) );

	//	LOG(Info, "[CTable::%s] tid:[%d] card:[%d] bankerseatid:[%d] fromseatid:[%d] zhaliaoseatid:[%d]", __FUNCTION__, 
	//		m_tid, iterCard->getVal(), m_tableLocal.m_bankerSeatId, fromSeatid, zhaniaoSeatId);
	//}

	m_tempVar.gangCards.clear();

	CARDS_t cards;
	m_tableLocal.m_decks.fetchCards(cards, zhaniaoCount);

	//if (cards.empty())
	//{
	//	if (!gang)
	//	{
	//		cards.push_back(m_tableLocal.m_sendCard);
	//	}
	//	else
	//	{
	//		// 取暂存杠牌
	//		cards = m_tempVar.gangCards;

	//		// 杠牌用完之后清空
	//		m_tempVar.gangCards.clear();
	//	}
	//}
	//
	//if (cards.size() == 1)
	//{
	//	cards.push_back(cards[0]);
	//}

	for (CARDS_t::iterator iter = cards.begin(); iter != cards.end(); ++iter)
	{
		int modNumber = (iter->getPoint() - 1) % MAX_SEAT;

		int zhaniaoSeatId = fromSeatid; //m_tableLocal.m_bankerSeatId;

		for (int i = 0; i < modNumber; ++i)
		{
			zhaniaoSeatId = SSeat::getNextSeatid(zhaniaoSeatId);
		}

		m_tempVar.vecZhaniao.push_back( std::make_pair(iter->getVal(), zhaniaoSeatId) );

		LOG(Info, "[CTable::%s] tid:[%d] card:[%d] bankerseatid:[%d] zhaliaoseatid:[%d]", __FUNCTION__, 
			m_tid, iter->getVal(), m_tableLocal.m_bankerSeatId, zhaniaoSeatId);
	}
}

void CTable::__makeBalanceInfo(int huSeatId, const SSelectAction& selectAction)
{
	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(huSeatId);

	CCheck check(seatLocal.hands);

	if (!m_tableLocal.m_putCard.isValid())
	{
		EATCARDS_t huCards;
		if (!check.canHu(huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] zimo", __FUNCTION__, m_tid, huSeatId);

		m_tableLocal.m_balanceInfo.huCard = m_tableLocal.m_sendCard;
		m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_sendCardSeatId;

		//m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_NORMAL);
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_ZIMO);
	}
	else
	{
		EATCARDS_t huCards;
		if (!check.canHu(m_tableLocal.m_putCard, huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] putcard:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, m_tableLocal.m_putCard.getVal(), selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid from:[%d] => [%d]", __FUNCTION__, m_tid, m_tableLocal.m_putCardSeatId, huSeatId);

		if (m_tableLocal.m_balanceInfo.cardSeatId == -1)
		{
			m_tableLocal.m_balanceInfo.huCard = m_tableLocal.m_putCard;
			m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_putCardSeatId;
		}

		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_NORMAL);
	}

	int huFlag = check.getHuFlag();

	__makeSeatHuTypes(huSeatId, huFlag);
}

void CTable::__makeGangBalanceInfo(int huSeatId, const SSelectAction& selectAction)
{
	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(huSeatId);

	CCheck check(seatLocal.hands);

	if (huSeatId == m_tableLocal.m_sendCardSeatId)
	{
		EATCARDS_t huCards;
		if (!check.canHu(selectAction.cards[0], huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] HU_TYPE_GANGSHANGKAIHUA", __FUNCTION__, m_tid, huSeatId);

		m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
		m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_sendCardSeatId;

		//m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_ZIMO);
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_GANGSHANGKAIHUA);
	}
	else
	{
		EATCARDS_t huCards;
		if (!check.canHu(selectAction.cards[0], huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid from:[%d] => [%d] HU_TYPE_GANGSHANGPAO", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId, huSeatId);

		if (m_tableLocal.m_balanceInfo.cardSeatId == -1)
		{
			m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
			m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_sendCardSeatId;
		}

		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_GANGSHANGPAO);
	}

	int huFlag = check.getHuFlag();

	__makeSeatHuTypes(huSeatId, huFlag);
}

void CTable::__makeHaidiBalanceInfo(int huSeatId, const SSelectAction& selectAction)
{
	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(huSeatId);

	CCheck check(seatLocal.hands);

	if (huSeatId == m_tableLocal.m_sendCardSeatId)
	{
		EATCARDS_t huCards;
		if (!check.canHu(selectAction.cards[0], huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] HU_TYPE_HAIDILAOYUE", __FUNCTION__, m_tid, huSeatId);

		m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
		m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_sendCardSeatId;

		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_ZIMO);
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_HAIDILAOYUE);
	}
	else
	{
		EATCARDS_t huCards;
		if (!check.canHu(selectAction.cards[0], huCards))
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
				m_tid, huSeatId, selectAction.toString().c_str());
			assert(0);
			return;
		}

		LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid from:[%d] => [%d] HU_TYPE_HAIDILAOYUE", __FUNCTION__, m_tid, m_tableLocal.m_sendCardSeatId, huSeatId);

		if (m_tableLocal.m_balanceInfo.cardSeatId == -1)
		{
			m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
			m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_sendCardSeatId;
		}

		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_HAIDILAOYUE);
	}

	int huFlag = check.getHuFlag();

	__makeSeatHuTypes(huSeatId, huFlag);
}

void CTable::__makeQiangGangBalanceInfo(int huSeatId, const SSelectAction& selectAction)
{
	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(huSeatId);

	CCheck check(seatLocal.hands);

	EATCARDS_t huCards;
	if (!check.canHu(selectAction.cards[0], huCards))
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
			m_tid, huSeatId, selectAction.toString().c_str());
		assert(0);
		return;
	}

	LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid from:[%d] => [%d] HU_TYPE_QIANGGANG", __FUNCTION__, m_tid, m_tableLocal.m_putCardSeatId, huSeatId);

	if (m_tableLocal.m_balanceInfo.cardSeatId == -1)
	{
		m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
		m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_putCardSeatId;
	}

	m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_QIANGGANG);

	int huFlag = check.getHuFlag();

	__makeSeatHuTypes(huSeatId, huFlag);
}

void CTable::__makeQiangBuBalanceInfo(int huSeatId, const SSelectAction& selectAction)
{
	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(huSeatId);

	CCheck check(seatLocal.hands);

	EATCARDS_t huCards;
	if (!check.canHu(selectAction.cards[0], huCards))
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] seatid:[%d] select action:[%s], bug!!!", __FUNCTION__, 
			m_tid, huSeatId, selectAction.toString().c_str());
		assert(0);
		return;
	}

	LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid from:[%d] => [%d] HU_TYPE_NORMAL", __FUNCTION__, m_tid, m_tableLocal.m_putCardSeatId, huSeatId);

	if (m_tableLocal.m_balanceInfo.cardSeatId == -1)
	{
		m_tableLocal.m_balanceInfo.huCard = selectAction.cards[0];
		m_tableLocal.m_balanceInfo.cardSeatId = m_tableLocal.m_putCardSeatId;
	}

	m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_NORMAL);

	int huFlag = check.getHuFlag();

	__makeSeatHuTypes(huSeatId, huFlag);
}

void CTable::__makeSeatHuTypes(int huSeatId, int huFlag)
{
	//// 海底胡
	//if (m_tableLocal.m_decks.getCards().empty())
	//{
	//	m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_HAIDILAOYUE);
	//}

	bool bHasDaHu = false;

	// 大胡
	if (huFlag & HU_PENGPENGHU)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_PENGPENG);

		bHasDaHu = true;
	}
	if (huFlag & HU_JIANGJIANGHU)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_JIANGJIANG);

		bHasDaHu = true;
	}
	if (huFlag & HU_QINGYISE)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_QINGYISE);

		bHasDaHu = true;
	}
	if (huFlag & HU_QUANQIUREN)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_QUANQIUREN);

		bHasDaHu = true;
	}
	if (huFlag & HU_7DUIZI)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_7DUIZI);

		bHasDaHu = true;
	}
	if (huFlag & HU_HAOHUA7DUI)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_HAOHUA7DUI);

		bHasDaHu = true;
	}
	if (huFlag & HU_CHAOHAOHUA7DUI)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].insert(HU_TYPE_CHAOHAOHUA7DUI);

		bHasDaHu = true;
	}

	if (bHasDaHu)
	{
		m_tableLocal.m_balanceInfo.mapSeatHuType[huSeatId].erase( HU_TYPE_NORMAL );
	}
}

void CTable::__goonRoundLogic()
{
	if (m_tableLocal.m_balanceInfo.mapSeatHuType.empty())
	{
		m_tableLocal.m_bankerSeatId = m_tableLocal.m_putCardSeatId;
	}
	else if (m_tableLocal.m_balanceInfo.mapSeatHuType.size() > 1)
	{
		m_tableLocal.m_bankerSeatId = m_tableLocal.m_balanceInfo.cardSeatId;
	}
	else
	{
		std::map<int, std::set<int> >::iterator iterHuSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin();

		m_tableLocal.m_bankerSeatId = iterHuSeatId->first;
	}

	m_tempVar.vecZhaniao.clear();

	m_tempVar.cleanPassHu();

	m_tableLocal.cleanRound();

	if (m_tableAttr.getPlayRound() >= m_tableAttr.getTotalRound())
	{
		{
			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_ROOM_EXPIRE_BC;
			packet_r.end();

			__broadPacket(NULL, packet_r.tostring());
		}

		__broadTotalBalance();

		__switchToState(S_OVER);
	}
	else
	{
		__switchToState(S_GOON);
	}

	m_tableLocal.update();
}

void CTable::__tryCoolRoom()
{
	if (!__isTableEmpty())
	{
		m_tempVar.uTotalFreeTimes = 0;
	}
	else
	{
		m_tempVar.uTotalFreeTimes += DEAMON_INTERVAL;

		if (m_tempVar.uTotalFreeTimes >= COOL_TABLE_SECONDS)
		{
			LOG(Info, "[CTable::%s] tid:[%d] too many times no player, so cool", __FUNCTION__, m_tid);

			m_tableLocal.update();

			((CGame*)g_entry.pGame)->coolTable(m_tid);
		}
	}
}

void CTable::__sendOptionActions(int seatid, const SOptionActionS& optionActions)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_NOTIFY_ACTION_UC;

	std::stringstream ssDebug;

	for (SOptionActionS::const_iterator c_iter = optionActions.begin(); c_iter != optionActions.end(); ++c_iter)
	{
		const SOptionAction& optionAction = c_iter->second;

		Json::Value jsAction;
		jsAction["type"] = optionAction.type;

		ssDebug << " type:" << optionAction.type << " ";

		for (std::vector<CARDS_t>::const_iterator c_iterCards = optionAction.arrCards.begin(); c_iterCards != optionAction.arrCards.end(); ++c_iterCards)
		{
			const CARDS_t& cards = (*c_iterCards);

			Json::Value jsCards;

			ssDebug << "(";

			for (CARDS_t::const_iterator c_iterCard = cards.begin(); c_iterCard != cards.end(); ++c_iterCard)
			{
				jsCards["cards"].append( c_iterCard->getVal() );

				ssDebug << c_iterCard->getVal() << ",";
			}

			ssDebug << ") ";

			jsAction["options"].append(jsCards);
		}

		packet_r.val["actions"].append(jsAction);
	}

	packet_r.end();

	__sendPacketToSeatId(seatid, packet_r.tostring());

	LOG(Info, "[CTable::%s] tid:[%d] seatid:[%d] optionactions:[%s]", __FUNCTION__, m_tid, seatid, ssDebug.str().c_str());
}

int CTable::__decideOptionAction(std::vector<int>& vecDecideSeatId)
{
	std::map<int, int> mapSeatIdSelect;
	std::map<int, int> mapSeatIdNotSelect;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		if (seatLocal.selectAction.type == 0)
		{
			if (!seatLocal.optionActions.empty())
			{
				SOptionActionS::reverse_iterator r_iter = seatLocal.optionActions.rbegin();
				mapSeatIdNotSelect[seat.seatid] = r_iter->first;
			}
		}
		else
		{
			mapSeatIdSelect[seat.seatid] = seatLocal.selectAction.type;
		}
	}

	std::map<int, std::vector<int> > mapSelectTypeVecSeats;
	std::map<int, std::vector<int> > mapNotSelectTypeVecSeats;

	for (std::map<int, int>::iterator iter = mapSeatIdSelect.begin(); iter != mapSeatIdSelect.end(); ++iter)
	{
		mapSelectTypeVecSeats[iter->second].push_back(iter->first);
	}
	for (std::map<int, int>::iterator iter = mapSeatIdNotSelect.begin(); iter != mapSeatIdNotSelect.end(); ++iter)
	{
		mapNotSelectTypeVecSeats[iter->second].push_back(iter->first);
	}

	if (mapSelectTypeVecSeats.empty())
		return -1;

	std::map<int, std::vector<int> >::reverse_iterator r_iterSelect = mapSelectTypeVecSeats.rbegin();

	for (std::map<int, std::vector<int> >::iterator iterNotSelect = mapNotSelectTypeVecSeats.begin(); iterNotSelect != mapNotSelectTypeVecSeats.end(); ++iterNotSelect)
	{
		if (iterNotSelect->first >= r_iterSelect->first)
			return -1;
	}

	vecDecideSeatId = r_iterSelect->second;
	return r_iterSelect->first;
}

bool CTable::__checkCardFromHands(const SSeatLocal& seatLocal, const Card& card)
{
	const CARDS_t& handCards = seatLocal.hands.getHandCards();

	return (std::find(handCards.begin(), handCards.end(), card) != handCards.end());
}

bool CTable::__isTableEmpty()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (seat.occupied)
			return false;
	}

	return true;
}

bool CTable::__isAllReady()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (!seat.occupied || !seat.ready)
			return false;
	}

	return true;
}

void CTable::__cleanPlayerReady()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		seat.ready = false;
	}
}

void CTable::__tryBindOwnner()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		if (seatLocal.binduid == m_tableAttr.getOWUid())
		{
			LOG(Info, "[CTable::%s] tid:[%d] has bind ownner:[%d] at seatid:[%d]", __FUNCTION__, m_tid, m_tableAttr.getOWUid(), seat.seatid);
			return;
		}
	}

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		if (seatLocal.binduid == -1)
		{
			seatLocal.binduid = m_tableAttr.getOWUid();

			LOG(Info, "[CTable::%s] tid:[%d] bind ownner:[%d] at seatid:[%d]", __FUNCTION__, m_tid, m_tableAttr.getOWUid(), seat.seatid);

			CPlayer* pPlayer = CPlayer::getPlayer( m_tableAttr.getOWUid() );
			if (pPlayer)
			{
				LOG(Info, "[CTable::%s] #bind# tid:[%d] set uid:[%d] vid(%d) zid(%d)", __FUNCTION__, 
					m_tid, pPlayer->getUid(), g_entry.conf["tables"]["vid"].asInt(), m_tid);

				pPlayer->setVid( g_entry.conf["tables"]["vid"].asInt() );
				pPlayer->setZid(m_tid);
				CPlayer::freePlayer(pPlayer);
			}

			return;
		}
	}
}

int CTable::__getOwnnerSeatId()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		if (seatLocal.binduid == m_tableAttr.getOWUid())
			return seat.seatid;
	}

	LOG(Error, "[CTable::%s] #error# tid:[%d] get ownner seatid is -1!", __FUNCTION__, m_tid); 
	return -1;
}

void CTable::__transBindOwnner(int fromseatid, int fromuid, int toseatid, int touid)
{
	m_tableLocal.getSeatLocal(fromseatid).binduid = -1;
	m_tableLocal.getSeatLocal(toseatid).binduid = touid;

	CPlayer* pPlayer = CPlayer::getPlayer(fromuid);
	if (pPlayer)
	{
		LOG(Info, "[CTable::%s] #bind# tid:[%d] set uid:[%d] vid(%d) zid(%d)", __FUNCTION__, 
			m_tid, pPlayer->getUid(), 0, 0);

		pPlayer->setVid(0);
		pPlayer->setZid(0);
		CPlayer::freePlayer(pPlayer);
	}

	pPlayer = CPlayer::getPlayer(touid);
	if (pPlayer)
	{
		LOG(Info, "[CTable::%s] #bind# tid:[%d] set uid:[%d] vid(%d) zid(%d)", __FUNCTION__, 
			m_tid, pPlayer->getUid(), g_entry.conf["tables"]["vid"].asInt(), m_tid);

		pPlayer->setVid( g_entry.conf["tables"]["vid"].asInt() );
		pPlayer->setZid(m_tid);
		CPlayer::freePlayer(pPlayer);
	}
}

void CTable::__bindPlayers()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (seat.occupied && seat.uid != -1)
		{
			m_tableLocal.getSeatLocal(seat.seatid).binduid = seat.uid;

			CPlayer* pPlayer = CPlayer::getPlayer(seat.uid);
			if (pPlayer)
			{
				LOG(Info, "[CTable::%s] #bind# tid:[%d] set uid:[%d] vid(%d) zid(%d)", __FUNCTION__, 
					m_tid, pPlayer->getUid(), g_entry.conf["tables"]["vid"].asInt(), m_tid);

				pPlayer->setVid( g_entry.conf["tables"]["vid"].asInt() );
				pPlayer->setZid(m_tid);
				CPlayer::freePlayer(pPlayer);
			}
		}
	}


	m_tableLocal.update();
}

void CTable::__tickAllPlayer()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (!seat.occupied)
			continue;

		CPlayer* pPlayer = (CPlayer*)g_entry.pGame->findPlayerByUid(seat.uid);

		LOG(Info, "[CTable::%s] tid:[%d] tick uid:[%d]", __FUNCTION__, m_tid, pPlayer->getUid());

		g_entry.pGame->tickPlayer(pPlayer);
	}
}

int CTable::__chooseSeatId(int uid)
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (m_tableLocal.getSeatLocal(seat.seatid).binduid == uid)
		{
			return seat.seatid;
		}
	}

	int factor = m_random(0, MAX_SEAT-1);

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		int pos = (factor+i) % MAX_SEAT;

		SSeat& seat = m_seats[pos];

		if (m_tableLocal.getSeatLocal(seat.seatid).binduid != -1)
			continue;

		if (!seat.occupied)
			return seat.seatid;
	}

	return -1;
}

std::vector<int> CTable::__getOtherSeatid(int seatid)
{
	std::vector<int> vecRet;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (seat.seatid != seatid)
			vecRet.push_back(seat.seatid);
	}

	return vecRet;
}

SSeat& CTable::__getSeatById(int seatid)
{
	static SSeat nullSeat;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		if (seat.seatid == seatid)
			return seat;
	}

	return nullSeat;
}

bool CTable::__isGameing()
{
	return ( (m_tableLocal.m_gameState != S_INIT) && (m_tableLocal.m_gameState != S_END) && (m_tableLocal.m_gameState != S_OVER) && (m_tableLocal.m_gameState != S_GOON) );
}

void CTable::__switchToState(const EGameState& state)
{
	LOG(Info, "[CTable::%s] tid:[%d] state:[%d] -> [%d]", __FUNCTION__, m_tid, m_tableLocal.m_gameState, state);

	m_tableLocal.m_gameState = (int)state;

	if (m_tableLocal.m_gameState == S_OVER)
	{
		ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDeamon);
		m_evTimerDeamon.data = this;
		ev_timer_init(&m_evTimerDeamon, CTable::__cbTimerDeamon, 0.5, 0.5);
		ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDeamon);
	}
}

int CTable::__convertClientGameState(int gamestate)
{
	if (gamestate == S_FIRSTCARDS || gamestate == S_QISHOUHU || gamestate == S_PLAY)
	{
		return 1;
	}
	else if (gamestate == S_END || gamestate == S_GOON)
	{
		return 2;
	}
	else
	{
		return 0;
	}
}

// 写入牌局记录
void CTable::__appendRecord(int type, const std::string& strRecord, bool bJsonFormat)
{
	if (bJsonFormat)
		m_tableLocal.m_records.vecTypeRecord.push_back( std::make_pair(type, base64encode(strRecord)) );
	else
		m_tableLocal.m_records.vecTypeRecord.push_back( std::make_pair(type, strRecord) );
}

// 牌局记录：初使化
void CTable::__recordInit()
{
	Json::Value jsRecord;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Json::Value jsPlayer;
		jsPlayer["seatid"] = seat.seatid;
		jsPlayer["uid"] = m_tableLocal.getSeatLocal(seat.seatid).binduid;
		jsPlayer["score"] = m_tableLocal.getSeatLocal(seat.seatid).score;

		CPlayer* pPlayer = CPlayer::getPlayer( m_tableLocal.getSeatLocal(seat.seatid).binduid );
		if (pPlayer)
		{
			jsPlayer["name"] = pPlayer->m_name;
			jsPlayer["avatar"] = pPlayer->m_avatar;
			jsPlayer["sex"] = pPlayer->m_sex;

			jsRecord["players"].append(jsPlayer);

			CPlayer::freePlayer(pPlayer);
		}
	}

	__appendRecord(RECORD_TYPE_INIT, jsRecord.toStyledString(), true);
}

// 牌局记录：开始发牌
void CTable::__recordFirstcard(int bankerType)
{
	Json::Value jsRecord;

	jsRecord["banker_seatid"] = m_tableLocal.m_bankerSeatId;
	jsRecord["banker_type"] = bankerType;
	jsRecord["decks_count"] = m_tableLocal.m_decks.cardCount();

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Json::Value jsHands;
		jsHands["seatid"] = seat.seatid;

		const CARDS_t& handCards = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards();

		for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
		{
			jsHands["hands"].append( c_iter->getVal() );
		}

		if (jsHands["hands"].size() < 14)
			jsHands["hands"].append(0);

		jsRecord["player_cards"].append(jsHands);
	}

	__appendRecord(RECORD_TYPE_FIRSTCARD, jsRecord.toStyledString(), true);
}

// 牌局记录：起牌胡
void CTable::__recordQibaihu(const Json::Value& jsRecord)
{
	//Json::Value jsRecord;

	__appendRecord(RECORD_TYPE_QIBAIHU, jsRecord.toStyledString(), true);
}

// 牌局记录：玩家出牌
void CTable::__recordPutcard(int seatid, const Card& card, const CARDS_t& handCards)
{
	Json::Value jsRecord;

	jsRecord["seatid"] = seatid;
	jsRecord["card"] = card.getVal();

	int zeroCount = 13 - handCards.size();
	for (int i = 0; i < zeroCount; ++i)
		jsRecord["hands"].append(0);

	for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
	{
		jsRecord["hands"].append( c_iter->getVal() );
	}

	jsRecord["hands"].append(0);

	__appendRecord(RECORD_TYPE_PUTCARD, jsRecord.toStyledString(), true);
}

// 牌局记录：玩家发牌
void CTable::__recordSendcard(int seatid, const Card& card, int decks_count)
{
	Json::Value jsRecord;

	jsRecord["seatid"] = seatid;
	jsRecord["card"] = card.getVal();
	jsRecord["decks_count"] = decks_count;

	__appendRecord(RECORD_TYPE_SENDCARD, jsRecord.toStyledString(), true);
}

// 牌局记录：玩家选择操作
void CTable::__recordAction(int seatid, int type)
{
	Json::Value jsRecord;

	jsRecord["seatid"] = seatid;
	jsRecord["type"] = type;

	__appendRecord(RECORD_TYPE_ACTION, jsRecord.toStyledString(), true);
}

// 牌局记录：玩家操作结果
void CTable::__recordResult(const Json::Value& jsRecord)
{
	//Json::Value jsRecord;

	__appendRecord(RECORD_TYPE_RESULT, jsRecord.toStyledString(), true);
}

// 牌局记录：杠上发牌
void CTable::__recordGangcard(int seatid, const Card& card1, const Card& card2, int decks_count)
{
	Json::Value jsRecord;
	jsRecord["seatid"] = seatid;
	jsRecord["cards"].append(card1.getVal());
	jsRecord["cards"].append(card2.getVal());
	jsRecord["decks_count"] = decks_count;

	__appendRecord(RECORD_TYPE_GANGCARD, jsRecord.toStyledString(), true);
}

// 牌局记录：海底牌
void CTable::__recordHaidi(int seatid, const Card& card)
{
	Json::Value jsRecord;
	jsRecord["seatid"] = seatid;
	jsRecord["card"] = card.getVal();

	__appendRecord(RECORD_TYPE_HAIDI, jsRecord.toStyledString(), true);
}

// 牌局记录：结算
void CTable::__recordBalance(const Json::Value& jsRecord)
{
	__appendRecord(RECORD_TYPE_BALANCE, jsRecord.toStyledString(), true);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
// 发包辅助

void CTable::__broadPlayerEnter(CPlayer* pPlayer)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_PLAYER_ENTER_BC;

	Json::Value jsPlayer;
	jsPlayer["seatid"] = pPlayer->m_seatid;
	jsPlayer["uid"] = pPlayer->getUid();
	jsPlayer["name"] = pPlayer->m_name;
	jsPlayer["sex"] = pPlayer->m_sex;
	jsPlayer["avatar"] = pPlayer->m_avatar;
	jsPlayer["avatar_auth"] = pPlayer->m_avatar_auth;
	jsPlayer["ps"] = pPlayer->m_ps;
	jsPlayer["money"] = pPlayer->m_money;
	jsPlayer["vlevel"] = pPlayer->m_vlevel;
	jsPlayer["exp"] = pPlayer->m_exp;
	jsPlayer["rmb"] = pPlayer->m_rmb; 
	jsPlayer["vlevel"] = pPlayer->m_gid;

	std::string strPeerIp;
	uint16_t uPeerPort = 0;
	pPlayer->getConn()->getPeerInfo(strPeerIp, uPeerPort);
	jsPlayer["clientip"] = strPeerIp;

	jsPlayer["score"] = m_tableLocal.getSeatLocal(pPlayer->m_seatid).score;

	packet_r.val["player"].append(jsPlayer);

	packet_r.end();

	__broadPacket(pPlayer, packet_r.tostring());
}

void CTable::__broadPlayerExit(CPlayer* pPlayer, int seatid, bool bbinded)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_PLAYER_EXIT_BC;
	packet_r.val["seatid"] = seatid;
	packet_r.val["uid"] = pPlayer->getUid();
	packet_r.val["binded"] = (bbinded ? 1 : 0);
	packet_r.end();

	__broadPacket(pPlayer, packet_r.tostring());
}

void CTable::__sendTableInfo(CPlayer* pPlayer, bool bRecover)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_TABLE_INFO_UC;
	packet_r.val["reconnect"] = (bRecover ? 1 : 0);
	packet_r.val["tid"] = m_tid;
	packet_r.val["myseatid"] = pPlayer->m_seatid;
	packet_r.val["gamestate"] = __convertClientGameState(m_tableLocal.m_gameState);
	packet_r.val["owner_seatid"] = __getOwnnerSeatId();
	packet_r.val["total_round"] = m_tableAttr.getTotalRound();
	packet_r.val["play_round"] = m_tableAttr.getPlayRound() + (__convertClientGameState(m_tableLocal.m_gameState) == 1 ? 1 : 0);
	packet_r.val["zhaniao_count"] = m_tableAttr.getZhaniaoCount();
	packet_r.val["bankerwin_score"] = m_tableAttr.isBankerWinScore() ? 1 : 0;
	packet_r.val["gametype"] = GAME_TYPE_CHANGSHA;
	packet_r.val["decks_count"] = m_tableLocal.m_decks.cardCount();
	packet_r.val["banker_seatid"] = m_tableLocal.m_bankerSeatId;
	packet_r.val["putcard_seatid"] = m_tableLocal.m_putCardSeatId;
	packet_r.val["putcard_card"] = m_tableLocal.m_putCard.getVal();
	packet_r.val["sendcard_seatid"] = m_tableLocal.m_sendCardSeatId;
	packet_r.val["sendcard_card"] = m_tableLocal.m_sendCard.getVal();

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Json::Value jsPlayer;

		CPlayer* pSeatPlayer = NULL;

		if (seat.occupied)
		{
			pSeatPlayer = (CPlayer*)g_entry.pGame->findPlayerByUid(seat.uid);

			jsPlayer["offline"] = 0;

			std::string strPeerIp;
			uint16_t uPeerPort = 0;
			pSeatPlayer->getConn()->getPeerInfo(strPeerIp, uPeerPort);
			jsPlayer["clientip"] = strPeerIp;
		}
		else
		{
			int binduid = m_tableLocal.getSeatLocal(seat.seatid).binduid;
			if (binduid != -1)
			{
				pSeatPlayer = CPlayer::getPlayer(binduid);
				if (pSeatPlayer == NULL)
				{
					LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] get player:[%d] no data!", __FUNCTION__, m_tid, pPlayer->getUid(), binduid);
				}

				jsPlayer["offline"] = 1;
			}
		}

		if (pSeatPlayer == NULL)
			continue;

		if (pSeatPlayer)
		{
			jsPlayer["seatid"] = seat.seatid;
			jsPlayer["ready"] = seat.ready;

			jsPlayer["uid"] = pSeatPlayer->getUid();
			jsPlayer["name"] = pSeatPlayer->m_name;
			jsPlayer["sex"] = pSeatPlayer->m_sex;
			jsPlayer["avatar"] = pSeatPlayer->m_avatar;
			jsPlayer["avatar_auth"] = pSeatPlayer->m_avatar_auth;
			jsPlayer["ps"] = pSeatPlayer->m_ps;
			jsPlayer["money"] = pSeatPlayer->m_money;
			jsPlayer["vlevel"] = pSeatPlayer->m_vlevel;
			jsPlayer["exp"] = pSeatPlayer->m_exp;
			jsPlayer["rmb"] = pSeatPlayer->m_rmb; 
			jsPlayer["cid"] = pSeatPlayer->m_gid;
		}

		if (!seat.occupied)
		{
			CPlayer::freePlayer(pSeatPlayer);
		}


		SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(seat.seatid);

		const CARDS_t& handCards = seatLocal.hands.getHandCards();
		const EATCARDS_t& eatCards = seatLocal.hands.getEatCards();
		const CARDS_t& outCards = seatLocal.hands.getOutCards();

		bool bPutCard = (m_tableLocal.m_putCardSeatId == seat.seatid && !m_tableLocal.m_putCard.isValid());
		if (!bPutCard)
		{
			bPutCard = (handCards.size() > 13);
		}

		int zeroCount = 0;
		if (bPutCard)
			zeroCount = 14 - handCards.size();
		else
			zeroCount = 13 - handCards.size();
		for (int i = 0; i < zeroCount; ++i)
			jsPlayer["hands"].append(0);

		for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
		{
			if (pPlayer->m_seatid == seat.seatid)
				jsPlayer["hands"].append(c_iter->getVal());
			else
				jsPlayer["hands"].append(-1);
		}

		if (jsPlayer["hands"].size() < 14)
			jsPlayer["hands"].append(0);

		for (EATCARDS_t::const_iterator c_iter = eatCards.begin(); c_iter != eatCards.end(); ++c_iter)
		{
			Json::Value jsEat;
			jsEat["type"] = (int)c_iter->type;
			jsEat["first"] = c_iter->firstCard.getVal();
			jsEat["eat"] = c_iter->eatCard.getVal();

			jsPlayer["eats"].append(jsEat);
		}

		for (CARDS_t::const_iterator c_iter = outCards.begin(); c_iter != outCards.end(); ++c_iter)
		{
			jsPlayer["outcards"].append(c_iter->getVal());
		}

		jsPlayer["ting"] = seatLocal.ting;

		jsPlayer["score"] = seatLocal.score;

		packet_r.val["players"].append(jsPlayer);
	}

	packet_r.end();

	g_entry.pGame->sendPacket(pPlayer, packet_r.tostring());

	if (m_tableLocal.m_gameState == S_END)
	{
		g_entry.pGame->sendPacket(pPlayer, base64decode( m_tableLocal.m_balanceInfo.strZhaniao ) );
		g_entry.pGame->sendPacket(pPlayer, base64decode( m_tableLocal.m_balanceInfo.strScore ) );
	}
}

void CTable::__sendReentryInfo(CPlayer* pPlayer)
{
	if (m_tableLocal.m_gameState != S_PLAY)
		return;

	SSeatLocal& seatLocal = m_tableLocal.getSeatLocal(pPlayer->m_seatid);
	if (!seatLocal.optionActions.empty() && seatLocal.selectAction.type == 0)
	{
		if (seatLocal.optionActions.find(ACTION_HAIDI_YAO) != seatLocal.optionActions.end())
		{
			Jpacket packet_r;
			packet_r.val["cmd"] = SERVER_HAIDI_ASK_BC;
			packet_r.val["seatid"] = pPlayer->m_seatid;
			packet_r.end();

			g_entry.pGame->sendPacket(pPlayer, packet_r.tostring());
		}

		__sendOptionActions(pPlayer->m_seatid, seatLocal.optionActions);
		return;
	}

	if (pPlayer->m_seatid == m_tableLocal.m_sendCardSeatId && !m_tableLocal.m_sendCard.isValid())
	{
		if (m_tableLocal.m_sendCardFlag == 0)
		{
			__sendCardLogic();
		}
		else
		{
			__gangSendCardLogic();
		}
		return;
	}

	if (pPlayer->m_seatid == m_tableLocal.m_putCardSeatId && !m_tableLocal.m_putCard.isValid())
	{
		LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] send put card notify", __FUNCTION__, 
			m_tid, pPlayer->getUid(), pPlayer->m_seatid);

		__putCardLogic();
		return;
	}
}

void CTable::__sendDissolveSession(CPlayer* pPlayer)
{
	if (m_pDissolveSession)
	{
		m_pDissolveSession->askDissolve(pPlayer->m_seatid);
	}
}

void CTable::__broadGameState()
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_CUR_BANKER_BC;
	packet_r.val["gamestate"] = __convertClientGameState(m_tableLocal.m_gameState);
	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::__broadCurrentBanker(int select_type)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_CUR_BANKER_BC;
	packet_r.val["seatid"] = m_tableLocal.m_bankerSeatId;
	packet_r.val["select_type"] = select_type;
	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::__broadShowCards(/*const std::vector<int>& vecHuSeatId*/)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_SHOWCARD_BC;

	//for (std::vector<int>::const_iterator c_iter = vecHuSeatId.begin(); c_iter != vecHuSeatId.end(); ++c_iter)
	//{
	//	packet_r.val["huseatid"].append(*c_iter);
	//}

	bool bZiMo = false;
	for (std::map<int, std::set<int> >::iterator iterSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin(); 
		iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end(); ++iterSeatId)
	{
		packet_r.val["huseatid"].append(iterSeatId->first);

		std::set<int>& setHuType = iterSeatId->second;

		bZiMo = ( (setHuType.find(HU_TYPE_ZIMO) != setHuType.end()) || (setHuType.find(HU_TYPE_GANGSHANGKAIHUA) != setHuType.end()) );
	}
	packet_r.val["iszimo"] = (bZiMo ? 1 : 0);

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		const CARDS_t& handCards = m_tableLocal.getSeatLocal(seat.seatid).hands.getHandCards();
		const EATCARDS_t& eatCards = m_tableLocal.getSeatLocal(seat.seatid).hands.getEatCards();

		Json::Value jsCards;
		jsCards["seatid"] = seat.seatid;

		bool bPutCard = (m_tableLocal.m_putCardSeatId == seat.seatid && !m_tableLocal.m_putCard.isValid());

		int zeroCount = 0;
		if (bPutCard)
			zeroCount = 14 - handCards.size();
		else
			zeroCount = 13 - handCards.size();
		for (int i = 0; i < zeroCount; ++i)
			jsCards["hands"].append(0);

		for (CARDS_t::const_iterator c_iter = handCards.begin(); c_iter != handCards.end(); ++c_iter)
		{
			jsCards["hands"].append(c_iter->getVal());
		}

		if (jsCards["hands"].size() < 14)
			jsCards["hands"].append(0);

		for (EATCARDS_t::const_iterator c_iter = eatCards.begin(); c_iter != eatCards.end(); ++c_iter)
		{
			Json::Value jsEat;
			jsEat["type"] = (int)c_iter->type;
			jsEat["first"] = c_iter->firstCard.getVal();
			jsEat["eat"] = c_iter->eatCard.getVal();

			jsCards["eats"].append(jsEat);
		}

		std::map<int, std::set<int> >::iterator iterSeatId =  m_tableLocal.m_balanceInfo.mapSeatHuType.find(seat.seatid);
		if (iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end())
		{
			std::set<int>& setHuType = iterSeatId->second;
			for (std::set<int>::iterator iterType = setHuType.begin(); iterType != setHuType.end(); ++iterType)
			{
				jsCards["types"].append(*iterType);
			}
		}

		packet_r.val["cards"].append(jsCards);
	}

	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::__broadBalance()
{
	std::map<int, int> mapSeatScore;
	std::set<int> setAboutZhaniaoSeat;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		int qibaihuCount = 0;

		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI) == QIPAIHU_SIXI)
		{
			qibaihuCount++;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_BANBANHU) == QIPAIHU_BANBANHU)
		{
			qibaihuCount++;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE) == QIPAIHU_QUEYISE)
		{
			qibaihuCount++;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN) == QIPAIHU_66SHUN)
		{
			qibaihuCount++;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x2) == QIPAIHU_SIXI_x2)
		{
			qibaihuCount += 2;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x3) == QIPAIHU_SIXI_x3)
		{
			qibaihuCount += 3;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE_x2) == QIPAIHU_QUEYISE_x2)
		{
			qibaihuCount += 2;
		}
		if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN_x2) == QIPAIHU_66SHUN_x2)
		{
			qibaihuCount += 2;
		}

		if (qibaihuCount > 0)
		{
			std::vector<int> vecOtherSeatId = __getOtherSeatid(seat.seatid);

			for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
			{
				LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] qibaihu +2*%d(qibaihuCount) otherseateid:[%d]", __FUNCTION__, 
					m_tid, seat.seatid, qibaihuCount, *iterOtherSeatid);

				mapSeatScore[seat.seatid] += (2*qibaihuCount);
				mapSeatScore[*iterOtherSeatid] += (-2*qibaihuCount);
			}
		}
	}

	for (std::map<int, std::set<int> >::iterator iterSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin(); 
		iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end(); ++iterSeatId)
	{
		int seatid = iterSeatId->first;
		std::set<int>& setHuType = iterSeatId->second;

		bool bZiMo = ( (setHuType.find(HU_TYPE_ZIMO) != setHuType.end()) || (setHuType.find(HU_TYPE_GANGSHANGKAIHUA) != setHuType.end()) );

		bool bHasDaHu = false;

		for (std::set<int>::iterator iterHuType = setHuType.begin(); iterHuType != setHuType.end(); ++iterHuType)
		{
			if (*iterHuType == HU_TYPE_PENGPENG || *iterHuType == HU_TYPE_JIANGJIANG || *iterHuType == HU_TYPE_QINGYISE || *iterHuType == HU_TYPE_QUANQIUREN ||
				*iterHuType == HU_TYPE_7DUIZI || *iterHuType == HU_TYPE_HAOHUA7DUI || *iterHuType == HU_TYPE_CHAOHAOHUA7DUI || *iterHuType == HU_TYPE_HAIDILAOYUE || 
				*iterHuType == HU_TYPE_GANGSHANGKAIHUA || *iterHuType == HU_TYPE_QIANGGANG || *iterHuType == HU_TYPE_GANGSHANGPAO)
			{
				int baseScore = 6;
				if (*iterHuType == HU_TYPE_HAOHUA7DUI)
					baseScore = 12;
				if (*iterHuType == HU_TYPE_CHAOHAOHUA7DUI)
					baseScore = 18;

				if (bZiMo)
				{
					std::vector<int> vecOtherSeatId = __getOtherSeatid(seatid);
					for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
					{
						LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] zimo dahu +%d otherseateid:[%d]", __FUNCTION__, 
							m_tid, seatid, baseScore, *iterOtherSeatid);

						mapSeatScore[seatid] += (baseScore);
						mapSeatScore[*iterOtherSeatid] += (-baseScore);
					}
				}
				else
				{
					LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] dahu +%d otherseateid:[%d]", __FUNCTION__, 
						m_tid, seatid, baseScore, m_tableLocal.m_balanceInfo.cardSeatId);

					mapSeatScore[seatid] += (baseScore);
					mapSeatScore[m_tableLocal.m_balanceInfo.cardSeatId] += (-baseScore);
				}

				bHasDaHu = true;
			}
		}

		if (!bHasDaHu)
		{
			if (bZiMo)
			{
				std::vector<int> vecOtherSeatId = __getOtherSeatid(seatid);
				for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
				{
					LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] zimo xiaohu +2 otherseateid:[%d]", __FUNCTION__, 
						m_tid, seatid, *iterOtherSeatid);

					mapSeatScore[seatid] += (2);
					mapSeatScore[*iterOtherSeatid] += (-2);
				}
			}
			else
			{
				LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] xiaohu +1 otherseateid:[%d]", __FUNCTION__, 
					m_tid, seatid, m_tableLocal.m_balanceInfo.cardSeatId);

				mapSeatScore[seatid] += (1);
				mapSeatScore[m_tableLocal.m_balanceInfo.cardSeatId] += (-1);
			}
		}

		if (bZiMo)
		{
			std::vector<int> vecOtherSeatId = __getOtherSeatid(seatid);
			for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
			{
				int huCount = m_tempVar.zhaniaoCount(seatid);
				int otherCount = m_tempVar.zhaniaoCount(*iterOtherSeatid);

				setAboutZhaniaoSeat.insert(seatid);
				setAboutZhaniaoSeat.insert(*iterOtherSeatid);

				LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] zimo zhaniao +%d(huCount)+%d(otherCount) otherseateid:[%d]", __FUNCTION__, 
					m_tid, seatid, huCount, otherCount, *iterOtherSeatid);

				mapSeatScore[seatid] += (+huCount+otherCount);
				mapSeatScore[*iterOtherSeatid] += (-huCount-otherCount);
			}
		}
		else
		{
			int huCount = m_tempVar.zhaniaoCount(seatid);
			int otherCount = m_tempVar.zhaniaoCount(m_tableLocal.m_balanceInfo.cardSeatId);

			setAboutZhaniaoSeat.insert(seatid);
			setAboutZhaniaoSeat.insert(m_tableLocal.m_balanceInfo.cardSeatId);

			LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] xiaohu zhaniao +%d(huCount)+%d(otherCount) otherseateid:[%d]", __FUNCTION__, 
				m_tid, seatid, huCount, otherCount, m_tableLocal.m_balanceInfo.cardSeatId);

			mapSeatScore[seatid] += (+huCount+otherCount);
			mapSeatScore[m_tableLocal.m_balanceInfo.cardSeatId] += (-huCount-otherCount);
		}

		if (bZiMo)
		{
			if (bHasDaHu)
			{
				m_tableLocal.m_balanceInfo.incPaoCount(seatid, 1, 1);
			}
			else
			{
				m_tableLocal.m_balanceInfo.incPaoCount(seatid, 2, 1);
			}
		}
		else
		{
			if (bHasDaHu)
			{
				m_tableLocal.m_balanceInfo.incPaoCount(seatid, 3, 1);
				m_tableLocal.m_balanceInfo.incPaoCount(m_tableLocal.m_balanceInfo.cardSeatId, 5, 1);
			}
			else
			{
				m_tableLocal.m_balanceInfo.incPaoCount(seatid, 4, 1);
				m_tableLocal.m_balanceInfo.incPaoCount(m_tableLocal.m_balanceInfo.cardSeatId, 6, 1);
			}
		}
	}

	LOG(Info, "[CTable::%s] #hu# tid:[%d] bankerwinscore:[%d]", __FUNCTION__, m_tid, m_tableAttr.isBankerWinScore());

	if (m_tableAttr.isBankerWinScore())
	{
		int zimoSeatid = -1;

		for (std::map<int, std::set<int> >::iterator iterSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin(); 
			iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end(); ++iterSeatId)
		{
			int seatid = iterSeatId->first;
			std::set<int>& setHuType = iterSeatId->second;

			bool bZiMo = ( (setHuType.find(HU_TYPE_ZIMO) != setHuType.end()) || (setHuType.find(HU_TYPE_GANGSHANGKAIHUA) != setHuType.end()) );
			if (bZiMo)
			{
				zimoSeatid = seatid;
				break;
			}
		}

		std::map<int, std::set<int> >::iterator iterSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.find(m_tableLocal.m_bankerSeatId);
		if (iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end())
		{
			int seatid = iterSeatId->first;

			if (zimoSeatid == seatid)
			{
				std::vector<int> vecOtherSeatId = __getOtherSeatid(seatid);
				for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
				{
					LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] banker zimo +1 otherseateid:[%d]", __FUNCTION__, 
						m_tid, seatid, *iterOtherSeatid);

					mapSeatScore[seatid] += (1);
					mapSeatScore[*iterOtherSeatid] += (-1);
				}
			}
			else
			{
				LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] banker hu +1otherseateid:[%d]", __FUNCTION__, 
					m_tid, seatid,  m_tableLocal.m_balanceInfo.cardSeatId);

				mapSeatScore[seatid] += (1);
				mapSeatScore[m_tableLocal.m_balanceInfo.cardSeatId] += (-1);
			}
		}
		else if (m_tableLocal.m_bankerSeatId == m_tableLocal.m_balanceInfo.cardSeatId)
		{
			for (std::map<int, std::set<int> >::iterator iterSeatId = m_tableLocal.m_balanceInfo.mapSeatHuType.begin(); 
				iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end(); ++iterSeatId)
			{
				LOG(Info, "[CTable::%s] #hu# tid:[%d] bankerSeatid:[%d] fangpao -1 huSeatid:[%d]", __FUNCTION__, 
					m_tid, m_tableLocal.m_bankerSeatId, iterSeatId->first);

				mapSeatScore[m_tableLocal.m_bankerSeatId] += (-1);
				mapSeatScore[iterSeatId->first] += (1);
			}
		}
		else if (zimoSeatid != -1)
		{
			LOG(Info, "[CTable::%s] #hu# tid:[%d] bankerSeatid:[%d] other zimo -1 huSeatid:[%d]", __FUNCTION__, 
				m_tid, m_tableLocal.m_bankerSeatId, zimoSeatid);

			mapSeatScore[m_tableLocal.m_bankerSeatId] += (-1);
			mapSeatScore[zimoSeatid] += (1);
		}

		//// 查看起手胡，计算庄闲
		//for (int i = 0; i < MAX_SEAT; ++i)
		//{
		//	SSeat& seat = m_seats[i];

		//	int qibaihuCount = 0;

		//	if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI) == QIPAIHU_SIXI)
		//	{
		//		qibaihuCount++;
		//	}
		//	if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_BANBANHU) == QIPAIHU_BANBANHU)
		//	{
		//		qibaihuCount++;
		//	}
		//	if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE) == QIPAIHU_QUEYISE)
		//	{
		//		qibaihuCount++;
		//	}
		//	if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN) == QIPAIHU_66SHUN)
		//	{
		//		qibaihuCount++;
		//	}

		//	if (qibaihuCount > 0)
		//	{
		//		// 庄家起手胡
		//		if (seat.seatid == m_tableLocal.m_bankerSeatId)
		//		{
		//			std::vector<int> vecOtherSeatId = __getOtherSeatid(seat.seatid);

		//			for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
		//			{
		//				LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] banker qibaihu +1*%d(qibaihuCount) otherseateid:[%d]", __FUNCTION__, 
		//					m_tid, seat.seatid, qibaihuCount, *iterOtherSeatid);

		//				mapSeatScore[seat.seatid] += (1*qibaihuCount);
		//				mapSeatScore[*iterOtherSeatid] += (-1*qibaihuCount);
		//			}
		//		}
		//		// 闲家起手胡
		//		else
		//		{
		//			std::vector<int> vecOtherSeatId = __getOtherSeatid(seat.seatid);

		//			for (std::vector<int>::iterator iterOtherSeatid = vecOtherSeatId.begin(); iterOtherSeatid != vecOtherSeatId.end(); ++iterOtherSeatid)
		//			{
		//				if (*iterOtherSeatid == m_tableLocal.m_bankerSeatId)
		//				{
		//					LOG(Info, "[CTable::%s] #hu# tid:[%d] seatid:[%d] banker pei qibaihu -1*%d(qibaihuCount) otherseateid:[%d]", __FUNCTION__, 
		//						m_tid, seat.seatid, qibaihuCount, *iterOtherSeatid);

		//					mapSeatScore[seat.seatid] += (1*qibaihuCount);
		//					mapSeatScore[*iterOtherSeatid] += (-1*qibaihuCount);
		//				}
		//			}
		//		}
		//	}
		//}
	}

	// 广播扎鸟
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_ZHANIAO_BC;

		for (std::vector<std::pair<int, int> >::iterator iter = m_tempVar.vecZhaniao.begin(); iter != m_tempVar.vecZhaniao.end(); ++iter)
		{
			Json::Value jsZhaniao;

			jsZhaniao["card"] = iter->first;
			jsZhaniao["seatid"] = iter->second;

			std::set<int>::iterator iterSeat = setAboutZhaniaoSeat.find(iter->second);
			if (iterSeat != setAboutZhaniaoSeat.end())
			{
				jsZhaniao["iszhong"] = 1;
			}
			else
			{
				jsZhaniao["iszhong"] = 0;
			}

			packet_r.val["zhaniao"].append(jsZhaniao);
		}

		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());

		// 保存扎鸟结算信息串
		m_tableLocal.m_balanceInfo.strZhaniao = base64encode( packet_r.tostring() );
	}


	// 广播积分
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SERVER_BALANCE_BC;

		packet_r.val["banker_seatid"] = m_tableLocal.m_bankerSeatId;
		packet_r.val["iswin"] = m_tableLocal.m_balanceInfo.isWin ? 1 : 0;
		packet_r.val["fromseatid"] = m_tableLocal.m_balanceInfo.cardSeatId;
		packet_r.val["fromcard"] = m_tableLocal.m_balanceInfo.huCard.getVal();

		for (int i = 0; i < MAX_SEAT; ++i)
		{
			SSeat& seat = m_seats[i];

			std::map<int, int>::iterator iter = mapSeatScore.find(seat.seatid);

			if (iter != mapSeatScore.end())
				m_tableLocal.getSeatLocal(seat.seatid).score += iter->second;

			Json::Value jsScore;
			jsScore["seatid"] = seat.seatid;
			jsScore["score"] = m_tableLocal.getSeatLocal(seat.seatid).score;
			jsScore["incsore"] = (iter != mapSeatScore.end() ? iter->second : 0);

			std::map<int, std::set<int> >::iterator iterSeatId =  m_tableLocal.m_balanceInfo.mapSeatHuType.find(seat.seatid);
			if (iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end())
			{
				std::set<int>& setHuType = iterSeatId->second;
				for (std::set<int>::iterator iterType = setHuType.begin(); iterType != setHuType.end(); ++iterType)
				{
					jsScore["types"].append(*iterType);
				}
			}

			if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI) == QIPAIHU_SIXI )
			{
				jsScore["types"].append(HU_TYPE_SIXI);
			}
			if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_BANBANHU) == QIPAIHU_BANBANHU )
			{
				jsScore["types"].append(HU_TYPE_BANBAN);
			}
			if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE) == QIPAIHU_QUEYISE )
			{
				jsScore["types"].append(HU_TYPE_QUEYISE);
			}
			if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN) == QIPAIHU_66SHUN )
			{
				jsScore["types"].append(HU_TYPE_66SHUN);
			}

			if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x2) == QIPAIHU_SIXI_x2)
			{
				for (int i = 0; i < 2; ++i)
					jsScore["types"].append(HU_TYPE_SIXI);
			}
			if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x3) == QIPAIHU_SIXI_x3)
			{
				for (int i = 0; i < 3; ++i)
					jsScore["types"].append(HU_TYPE_SIXI);
			}
			if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE_x2) == QIPAIHU_QUEYISE_x2)
			{
				for (int i = 0; i < 2; ++i)
					jsScore["types"].append(HU_TYPE_QUEYISE);
			}
			if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN_x2) == QIPAIHU_66SHUN_x2)
			{
				for (int i = 0; i < 2; ++i)
					jsScore["types"].append(HU_TYPE_66SHUN);
			}

			packet_r.val["scores"].append(jsScore);
		}

		packet_r.end();

		__broadPacket(NULL, packet_r.tostring());

		// 保存积分结算信息串
		m_tableLocal.m_balanceInfo.strScore = base64encode( packet_r.tostring() );
	}

	// 牌局记录：结算
	Json::Value jsRecord;

	// 通知WebService上传每局战绩
	// TODO
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SYS_SCORE2_UPLOAD;
		packet_r.val["tid"] = m_tid;
		packet_r.val["tidalias"] = m_tableAttr.getTidAlias();
		packet_r.val["round"] = m_tableAttr.getPlayRound() + 1;
		packet_r.val["bankerseatid"] = m_tableLocal.m_bankerSeatId;
		packet_r.val["iswin"] = m_tableLocal.m_balanceInfo.isWin ? 1 : 0;
		packet_r.val["fromseatid"] = m_tableLocal.m_balanceInfo.cardSeatId;
		packet_r.val["fromcard"] = m_tableLocal.m_balanceInfo.huCard.getVal();

		// 牌局记录：结算
		jsRecord["bankerseatid"] = m_tableLocal.m_bankerSeatId;
		jsRecord["iswin"] = m_tableLocal.m_balanceInfo.isWin ? 1 : 0;
		jsRecord["fromseatid"] = m_tableLocal.m_balanceInfo.cardSeatId;
		jsRecord["fromcard"] = m_tableLocal.m_balanceInfo.huCard.getVal();

		// 扎鸟
		for (std::vector<std::pair<int, int> >::iterator iter = m_tempVar.vecZhaniao.begin(); iter != m_tempVar.vecZhaniao.end(); ++iter)
		{
			Json::Value jsZhaniao;

			jsZhaniao["card"] = iter->first;
			jsZhaniao["seatid"] = iter->second;

			std::set<int>::iterator iterSeat = setAboutZhaniaoSeat.find(iter->second);
			if (iterSeat != setAboutZhaniaoSeat.end())
			{
				jsZhaniao["iszhong"] = 1;
			}
			else
			{
				jsZhaniao["iszhong"] = 0;
			}

			packet_r.val["zhaniao"].append(jsZhaniao);

			// 牌局记录：结算
			jsRecord["zhaniao"].append(jsZhaniao);
		}
		
		for (int i = 0; i < MAX_SEAT; ++i)
		{
			SSeat& seat = m_seats[i];

			CPlayer* pPlayer = CPlayer::getPlayer( m_tableLocal.getSeatLocal(seat.seatid).binduid );
			if (pPlayer)
			{
				Json::Value jsScore;

				jsScore["uid"] = pPlayer->getUid();
				jsScore["name"] = pPlayer->m_name;
				jsScore["seatid"] = seat.seatid;

				std::map<int, int>::iterator iter = mapSeatScore.find(seat.seatid);
				jsScore["incscore"] = (iter != mapSeatScore.end() ? iter->second : 0);

				std::map<int, std::set<int> >::iterator iterSeatId =  m_tableLocal.m_balanceInfo.mapSeatHuType.find(seat.seatid);
				if (iterSeatId != m_tableLocal.m_balanceInfo.mapSeatHuType.end())
				{
					std::set<int>& setHuType = iterSeatId->second;
					for (std::set<int>::iterator iterType = setHuType.begin(); iterType != setHuType.end(); ++iterType)
					{
						jsScore["types"].append(*iterType);
					}
				}

				if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI) == QIPAIHU_SIXI )
				{
					jsScore["types"].append(HU_TYPE_SIXI);
				}
				if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_BANBANHU) == QIPAIHU_BANBANHU )
				{
					jsScore["types"].append(HU_TYPE_BANBAN);
				}
				if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE) == QIPAIHU_QUEYISE )
				{
					jsScore["types"].append(HU_TYPE_QUEYISE);
				}
				if ( (m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN) == QIPAIHU_66SHUN )
				{
					jsScore["types"].append(HU_TYPE_66SHUN);
				}

				if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x2) == QIPAIHU_SIXI_x2)
				{
					for (int i = 0; i < 2; ++i)
						jsScore["types"].append(HU_TYPE_SIXI);
				}
				if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_SIXI_x3) == QIPAIHU_SIXI_x3)
				{
					for (int i = 0; i < 3; ++i)
						jsScore["types"].append(HU_TYPE_SIXI);
				}
				if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_QUEYISE_x2) == QIPAIHU_QUEYISE_x2)
				{
					for (int i = 0; i < 2; ++i)
						jsScore["types"].append(HU_TYPE_QUEYISE);
				}
				if ((m_tableLocal.getSeatLocal(seat.seatid).qibaihu & QIPAIHU_66SHUN_x2) == QIPAIHU_66SHUN_x2)
				{
					for (int i = 0; i < 2; ++i)
						jsScore["types"].append(HU_TYPE_66SHUN);
				}

				packet_r.val["scores"].append(jsScore);

				CPlayer::freePlayer(pPlayer);

				// 牌局记录：结算
				jsRecord["scores"].append(jsScore);
			}
		}

		packet_r.end();

		if ( !CWebService::notifyWebService( packet_r.val.toStyledString() ) )
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] notify webservice SYS_SCORE2_UPLOAD failed", __FUNCTION__, m_tid);
		}
	}

	// 牌局记录：结算
	__recordBalance( jsRecord );

	// 通知WebService写入记录
	// TODO
	{
		Jpacket packet_r;
		packet_r.val["cmd"] = SYS_RECORDS;
		packet_r.val["tid"] = m_tid;
		packet_r.val["round"] = m_tableAttr.getPlayRound() + 1;

		for (std::vector< std::pair<int, std::string> >::iterator iter = m_tableLocal.m_records.vecTypeRecord.begin(); 
			iter != m_tableLocal.m_records.vecTypeRecord.end(); ++iter)
		{
			Json::Value jsRecord;
			jsRecord["type"] = iter->first;
			jsRecord["record"] = iter->second;
			packet_r.val["records"].append(jsRecord);
		}

		packet_r.end();

		if ( !CWebService::notifyWebService( packet_r.val.toStyledString() ) )
		{
			LOG(Error, "[CTable::%s] #error# tid:[%d] notify webservice SYS_SCORE1_UPLOAD failed", __FUNCTION__, m_tid);
		}
	}
}

// 广播总结算
void CTable::__broadTotalBalance()
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_TOTAL_BALANCE_BC;

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];

		Json::Value jsScore;

		jsScore["seatid"] = seat.seatid;
		jsScore["owner_seatid"] = __getOwnnerSeatId();
		jsScore["score"] = m_tableLocal.getSeatLocal(seat.seatid).score - INIT_SCORE;

		std::map<int, std::map<int,int> >::iterator iterSeat = m_tableLocal.m_balanceInfo.mapSeatPaoCount.find(seat.seatid);
		if (iterSeat != m_tableLocal.m_balanceInfo.mapSeatPaoCount.end())
		{
			for (std::map<int,int>::iterator iterPao = iterSeat->second.begin(); iterPao != iterSeat->second.end(); ++iterPao)
			{
				Json::Value jsPao;
				jsPao["paotype"] = iterPao->first;
				jsPao["count"] = iterPao->second;

				jsScore["paocount"].append(jsPao);
			}
		}

		packet_r.val["scores"].append(jsScore);
	}

	packet_r.end();

	__broadPacket(NULL, packet_r.tostring());
}

void CTable::__sendPacketToSeatId(int seatid, const std::string& packet)
{
	SSeat& seat = __getSeatById(seatid);
	if (seat.seatid == -1)
		return;

	if (seat.occupied && seat.uid != -1)
		g_entry.pGame->sendPacket(seat.uid, packet);
}

void CTable::__broadPacket(CPlayer* pPlayer, const std::string& packet)
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];
		if (!seat.occupied)
			continue;

		if (pPlayer && pPlayer->getUid() == seat.uid)
			continue;

		g_entry.pGame->sendPacket(seat.uid, packet);
	}
}

void CTable::__initTimerLocal()
{
	m_bStartQiShouHu = false;
	m_bStartPlayCards = false;
	m_bStartGoonRound = false;
	m_bStartGangHuAsk = false;
}

void CTable::__startQiShouHu()
{
	__stopQiShouHu();

	{
		m_evTimerQiShouHu.data = this;
		ev_timer_init(&m_evTimerQiShouHu, CTable::__cbTimerQiShouHu, QISHOUHU_SECONDS, 0);
		ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerQiShouHu);

		m_bStartQiShouHu = true;
	}
}

void CTable::__stopQiShouHu()
{
	if (!m_bStartQiShouHu)
		return;

	{
		ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerQiShouHu);

		m_bStartQiShouHu = false;
	}
}

void CTable::__startPlayCards(bool bHasQiShouHu)
{
	__stopPlayCards();

	{
		m_evTimerPlayCards.data = this;
		ev_timer_init(&m_evTimerPlayCards, CTable::__cbTimerPlayCards, (bHasQiShouHu ? PLAY_CHARDS_QISHOU_SECONDS : PLAY_CHARDS_SECONDS), 0);
		ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerPlayCards);

		m_bStartPlayCards = true;
	}
}

void CTable::__stopPlayCards()
{
	if (!m_bStartPlayCards)
		return;

	{
		ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerPlayCards);

		m_bStartPlayCards = false;
	}
}

void CTable::__startGoonRound()
{
	__stopGoonRound();

	{
		m_evTimerGoonRound.data = this;
		ev_timer_init(&m_evTimerGoonRound, CTable::__cbTimerGoonRound, GOON_ROUND_SECONDS, 0);
		ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerGoonRound);

		m_bStartGoonRound = true;
	}
}

void CTable::__stopGoonRound()
{
	if (!m_bStartGoonRound)
		return;

	{
		ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerGoonRound);

		m_bStartGoonRound = false;
	}
}

void CTable::__startGangHuAsk()
{
	__stopGangHuAsk();

	{
		m_evGangHuAsk.data = this;
		ev_timer_init(&m_evGangHuAsk, CTable::__cbTimerGangHuAsk, GANGHU_ASK_SECONDS, 0);
		ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evGangHuAsk);

		m_bStartGangHuAsk = true;
	}
}

void CTable::__stopGangHuAsk()
{
	if (!m_bStartGangHuAsk)
		return;

	{
		ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evGangHuAsk);

		m_bStartGangHuAsk = false;
	}
}

void CTable::__doTimerDeamon()
{
	if (m_tableLocal.m_gameState == S_INIT)
	{
		__initTableEntry();
	}
	else if (m_tableLocal.m_gameState == S_GOON)
	{
		__goonTableEntry();
	}
	else if (m_tableLocal.m_gameState == S_OVER)
	{
		__overTableEntry();
		return;
	}

	__tryCoolRoom();
}

void CTable::__doTimerQiShouHu()
{
	__stopQiShouHu();

	__qishouHu();
}

void CTable::__doTimerPlayCards()
{
	__stopPlayCards();

	__playCards();
}

void CTable::__doTimerGoonRound()
{
	__stopGoonRound();

	__goonRoundLogic();
}

void CTable::__doTimerGangHuAsk()
{
	__stopGangHuAsk();

	__gangHuAskLogic();
}

void CTable::__cbTimerDeamon(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CTable* pThis = (CTable*)w->data;

	pThis->__doTimerDeamon();
}

void CTable::__cbTimerQiShouHu(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CTable* pThis = (CTable*)w->data;

	pThis->__doTimerQiShouHu();
}

void CTable::__cbTimerPlayCards(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CTable* pThis = (CTable*)w->data;

	pThis->__doTimerPlayCards();
}

void CTable::__cbTimerGoonRound(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CTable* pThis = (CTable*)w->data;

	pThis->__doTimerGoonRound();
}

void CTable::__cbTimerGangHuAsk(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CTable* pThis = (CTable*)w->data;

	pThis->__doTimerGangHuAsk();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDissolveSession::CDissolveSession(CTable* pTable, int sponsorSeatid, int expireSession)
: m_pTable(pTable)
, m_sponsorSeatid(sponsorSeatid)
, m_expireSession(expireSession)
{
	__startSessionTimer();
}

CDissolveSession::~CDissolveSession()
{
	__stopSessionTimer();
}

void CDissolveSession::askDissolve()
{
	LOG(Info, "[CDissolveSession::%s] tid:[%d] sponsor_seatid:[%d] expire:[%d]", __FUNCTION__, m_pTable->m_tid, m_sponsorSeatid, m_expireSession);

	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_ASK_DISSOLVE_ROOM_BC;
	packet_r.val["sponsor_seatid"] = m_sponsorSeatid;
	packet_r.val["expire"] = m_expireSession;
	packet_r.end();

	m_pTable->__broadPacket(NULL, packet_r.tostring());
}

void CDissolveSession::askDissolve(int seatid)
{
	ev_tstamp tsRemain = ev_timer_remaining(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDissolveSession);

	LOG(Info, "[CDissolveSession::%s] tid:[%d] sponsor_seatid:[%d] expire:[%d]", __FUNCTION__, m_pTable->m_tid, m_sponsorSeatid, (int)tsRemain);

	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_ASK_DISSOLVE_ROOM_BC;
	packet_r.val["sponsor_seatid"] = m_sponsorSeatid;
	packet_r.val["expire"] = (int)tsRemain;

	for (std::map<int, int>::iterator iter = m_mapSeatAck.begin(); iter != m_mapSeatAck.end(); ++iter)
	{
		Json::Value jsAck;
		jsAck["seatid"] = iter->first;
		jsAck["ack"] = iter->second;

		packet_r.val["acks"].append(jsAck);
	}

	packet_r.end();

	m_pTable->__sendPacketToSeatId(seatid, packet_r.tostring());
}

void CDissolveSession::setSeatAck(int seatid, int ack)
{
	LOG(Info, "[CDissolveSession::%s] tid:[%d] seatid:[%d] ack:[%d]", __FUNCTION__, m_pTable->m_tid, seatid, ack);

	if (ack == 0)
	{
		m_pTable->cbDissolveRoomResult(0);
		return;
	}

	m_mapSeatAck[seatid] = ack;

	if (m_mapSeatAck.size() >= MAX_SEAT-1)
	{
		m_pTable->cbDissolveRoomResult(1);
	}
}

void CDissolveSession::__sessionTimeout()
{
	LOG(Info, "[CDissolveSession::%s] tid:[%d] ", __FUNCTION__, m_pTable->m_tid);

	m_pTable->cbDissolveRoomResult(1);
}

void CDissolveSession::__startSessionTimer()
{
	m_evTimerDissolveSession.data = this;
	ev_timer_init(&m_evTimerDissolveSession, CDissolveSession::__cbTimerDissolveSession, m_expireSession, 0);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDissolveSession);
}

void CDissolveSession::__stopSessionTimer()
{
	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDissolveSession);
}

void CDissolveSession::__cbTimerDissolveSession(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CDissolveSession* pThis = (CDissolveSession*)w->data;

	pThis->__sessionTimeout();
}
