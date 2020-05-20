#pragma once 

#include "Player.h"

#include <map>
#include <set>

#define MAX_SEAT	4

struct SSeat
{
	int seatid;
	bool occupied;	// 是否落座

	int uid; // 玩家ID

	SSeat()
	{
		seatid = -1;
		occupied = false;
		uid = -1;
	}

	static int getSeatId(int pos)
	{
		static int seatid_array[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
		if ( pos >= (int) (sizeof(seatid_array)/sizeof(int)) )
			return -1;

		return seatid_array[pos];
	}
};

class CTable
{
public:
	CTable(int tid);
	virtual ~CTable();

	int getTid();

	// 初使化桌子
	bool init();

	// 是否满员
	bool isFulled();

public:
	// 框架模板

	// 玩家进入
	bool enterPlayer(CPlayer* pPlayer);

	// 玩家退出
	bool exitPlayer(CPlayer* pPlayer);

	// 恢复玩家
	void recoverPlayer(CPlayer* pPlayer);

public:
	// 业务逻辑

	// 选择座位
	int __chooseSeatId(int uid);

	// 根据座位ID找座位
	SSeat& __getSeatById(int seatid);

private:
	// 发包辅助

	// 广播玩家进入
	void __broadPlayerEnter(CPlayer* pPlayer);

	// 广播玩家退出
	void __broadPlayerExit(CPlayer* pPlayer, int seatid);

	// 下发桌游信息
	void __sendTableInfo(CPlayer* pPlayer, bool bRecover);

	// 广播报文
	void __broadPacket(CPlayer* pPlayer, const std::string& packet);

private:
	int m_tid;
	SSeat m_seats[MAX_SEAT];
};
