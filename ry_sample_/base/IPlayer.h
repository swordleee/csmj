#pragma once 

#include "libevwork/EVWork.h"

class IPlayer
{
public:
	IPlayer(int uid, evwork::IConn* pConn);
	virtual ~IPlayer();

	int getUid();
	evwork::IConn* getConn();

	// 重新设置连接，返回旧连接
	evwork::IConn* changeConn(evwork::IConn* pConn);

	// 通知玩家登录成功
	virtual void cbLoginSuccess() = 0;

	// 尝试玩家退出，参数表示是否发送退出响应，返回true表示退出成功
	virtual bool cbTryLogout(bool bReponse = false) = 0;

	// 恢复客户端
	virtual void cbRecoverClient() = 0;

	// 托管玩家
	virtual void cbTrustPlayer() = 0;

	// 通知客户端被踢出
	virtual void cbTickClient() = 0;

private:

	void __startTimerTick();
	void __stopTimerTick();

	void __tickTrustMe();

	static void __cbTimerTickTrustMe(struct ev_loop *loop, struct ev_timer *w, int revents);

protected:
	int m_uid;
	evwork::IConn* m_pConn;

	ev_timer m_evTimerTickTrustMe;
	bool m_bTimerStart;
};
