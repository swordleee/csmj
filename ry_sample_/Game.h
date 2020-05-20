#pragma once 

#include "base/IGame.h"

#include "Table.h"

class CGame
	: public IGame
{
public:
	static IGame* createGame();

	CGame();
	virtual ~CGame();

	// 游戏初使化
	bool init();

	// 选桌子，tid为-1，表示随机选
	CTable* getTable(int tid);

	// 换桌子，tid为-1，表示随机选
	CTable* getOtherTable(int tid);

	// 改变桌子人数
	void changeTableNumber(int tid, int number);

protected:

	// 登录认证
	virtual bool __loginCheck(evwork::Jpacket& packet, evwork::IConn* pConn);

	// 创建新玩家
	virtual IPlayer* __createPlayer(int uid, evwork::IConn* pConn);

	// 销毁玩家
	virtual void __destroyPlayer(IPlayer* pPlayer);

protected:
	typedef std::tr1::unordered_map<int, CTable*> MAP_TID_TABLE_t;
	typedef std::map<int, int> MAP_TID_NUMBER_t;
	typedef std::map<int, std::set<int> > MAP_NUMBER_SETTID_t;

	MAP_TID_TABLE_t m_mapTidTable;
	MAP_TID_NUMBER_t m_mapTidNumber;
	MAP_NUMBER_SETTID_t m_mapNumberTids;
};
