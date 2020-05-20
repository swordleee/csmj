#include "Table.h"

#include "base/Entry.h"

#include "Game.h"
#include "Protocol.h"
#include "Helper.h"

using namespace evwork;

CTable::CTable(int tid)
: m_tid(tid)
{
}
CTable::~CTable()
{
}

int CTable::getTid()
{
	return m_tid;
}

// 初使化桌子
bool CTable::init()
{
	// 初使化位子
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		SSeat& seat = m_seats[i];
		seat.seatid = SSeat::getSeatId(i);
		seat.uid = -1;
		seat.occupied = false;
	}

	// 继续做其他初使化的逻辑
	// TODO

	LOG(Info, "[CTable::%s] table:[%d] init ok", __FUNCTION__, m_tid);
	return true;
}

// 是否满员
bool CTable::isFulled()
{
	for (int i = 0; i < MAX_SEAT; ++i)
	{
		if (!m_seats[i].occupied)
			return false;
	}

	return true;
}


// 玩家进入
bool CTable::enterPlayer(CPlayer* pPlayer)
{
	// 检查玩家
	if (pPlayer->m_tid != -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] has seat in tid:[%d] seatid:[%d]", 
			__FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_tid, pPlayer->m_seatid);
		return false;
	}

	// TODO
	// 视业务需求决定玩家是否落座

	// 如果需要落座，下面演示了如何找座位

	// 找座位
	int seatid = __chooseSeatId( pPlayer->getUid() );
	if (seatid == -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] no free seat", __FUNCTION__, m_tid, pPlayer->getUid());

		g_entry.pGame->sendCode(pPlayer, CLIENT_ENTER_ROOM, CODE_SEAT_NOFREE);
		return false;
	}

	SSeat& seat = __getSeatById(seatid);

	// 广播玩家进入
	__broadPlayerEnter(pPlayer);

	// 下发桌游信息
	__sendTableInfo(pPlayer, false);

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] ok", __FUNCTION__, m_tid, pPlayer->getUid(), seat.seatid);
	return true;
}

// 玩家退出
bool CTable::exitPlayer(CPlayer* pPlayer)
{
	// 检查玩家
	if (pPlayer->m_tid != m_tid || pPlayer->m_seatid == -1)
	{
		LOG(Error, "[CTable::%s] #error# tid:[%d] uid:[%d] real seat in tid:[%d] seatid:[%d]", 
			__FUNCTION__, m_tid, pPlayer->getUid(), pPlayer->m_tid, pPlayer->m_seatid);
		return false;
	}

	int curSeatId = pPlayer->m_seatid;

	// 广播玩家退出
	__broadPlayerExit(pPlayer, curSeatId);

	LOG(Info, "[CTable::%s] tid:[%d] uid:[%d] seatid:[%d] exit ok", __FUNCTION__, m_tid, pPlayer->getUid(), curSeatId);
	return true;
}

// 恢复客户端
void CTable::recoverPlayer(CPlayer* pPlayer)
{
	// 下发桌游信息
	__sendTableInfo(pPlayer, true);
}

// 选择座位
int CTable::__chooseSeatId(int uid)
{
	int factor = m_random(0, MAX_SEAT-1);

	for (int i = 0; i < MAX_SEAT; ++i)
	{
		int pos = (factor+i) % MAX_SEAT;

		SSeat& seat = m_seats[pos];

		if (!seat.occupied)
			return seat.seatid;
	}

	return -1;
}

// 根据座位ID找座位
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


// 广播玩家进入
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

	packet_r.val["player"].append(jsPlayer);

	packet_r.end();

	__broadPacket(pPlayer, packet_r.tostring());
}

// 广播玩家退出
void CTable::__broadPlayerExit(CPlayer* pPlayer, int seatid)
{
	Jpacket packet_r;
	packet_r.val["cmd"] = SERVER_PLAYER_EXIT_BC;
	packet_r.val["seatid"] = seatid;
	packet_r.val["uid"] = pPlayer->getUid();
	packet_r.end();

	__broadPacket(pPlayer, packet_r.tostring());
}

// 下发桌游信息
void CTable::__sendTableInfo(CPlayer* pPlayer, bool bRecover)
{
}

// 广播报文
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
