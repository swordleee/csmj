#pragma once 

#include "libevwork/FormDef.h"

#include "IPlayer.h"

#include <tr1/unordered_map>

class IGame
	: public evwork::PHClass
	, public evwork::ILinkEvent
{
public:
	DECLARE_FORM_MAP;

	IGame();
	virtual ~IGame();

	// 连接关注
	virtual void onConnected(evwork::IConn* pConn);
	virtual void onClose(evwork::IConn* pConn);

	// 协议处理
	void onSysEcho(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onSysOnline(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onHeartBeat(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onLoginReq(evwork::Jpacket& packet, evwork::IConn* pConn);
	void onLogoutReq(evwork::Jpacket& packet, evwork::IConn* pConn);

	// 发送报文
	void sendPacket(int uid, const std::string& strPacket);
	void sendPacket(IPlayer* pPlayer, const std::string& strPacket);
	void sendPacket(evwork::IConn* pConn, const std::string& strPacket);
	void sendCode(int uid, int cmd, int code);
	void sendCode(IPlayer* pPlayer, int cmd, int code);
	void sendCode(evwork::IConn* pConn, int cmd, int code);

	// 广播报文
	void bcbPacket(const std::string& strPacket);

	// 查找玩家
	IPlayer* findPlayerByUid(int uid);
	IPlayer* findPlayerByCid(int cid);

	// 踢掉玩家
	bool tickPlayer(int uid);
	bool tickPlayer(IPlayer* pPlayer);

protected:

	// 登录认证
	virtual bool __loginCheck(evwork::Jpacket& packet, evwork::IConn* pConn) = 0;

	// 创建新玩家
	virtual IPlayer* __createPlayer(int uid, evwork::IConn* pConn) = 0;

	// 销毁玩家
	virtual void __destroyPlayer(IPlayer* pPlayer) = 0;

private:

	void __tickConn(evwork::IConn* pConn);
	void __delConns();
	void __printInfo();

	static void __cbTimerTickConn(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void __cbTimerPrint(struct ev_loop *loop, struct ev_timer *w, int revents);

private:
	typedef std::tr1::unordered_map<int, IPlayer*> MAP_UID_PLAYER_t;
	typedef std::tr1::unordered_map<int, IPlayer*> MAP_CID_PLAYER_t;

	MAP_UID_PLAYER_t m_mapUidPlayer;
	MAP_CID_PLAYER_t m_mapCidPlayer;

	typedef std::set<evwork::IConn*> SET_CONN_t;
	SET_CONN_t m_setConnTick;

	ev_timer m_evTimerTickConn;
	ev_timer m_evTimerPrint;
};
