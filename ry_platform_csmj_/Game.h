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

	bool init();

	CTable* getTable(int tid);

	void coolTable(int tid);

protected:

	virtual bool __loginCheck(evwork::Jpacket& packet, evwork::IConn* pConn);

	virtual IPlayer* __createPlayer(int uid, evwork::IConn* pConn);

	virtual void __destroyPlayer(IPlayer* pPlayer);

	// 初使化本地数据
	bool __initLocal();

	// 销毁本地数据
	void __destroyLocal();

	void __showActiveTid();

	static void __cbTimerShowTid(struct ev_loop *loop, struct ev_timer *w, int revents);

protected:
	typedef std::tr1::unordered_map<int, CTable*> MAP_TID_TABLE_t;

	MAP_TID_TABLE_t m_mapTidTable;

	ev_timer m_evTimerShowTid;
};
