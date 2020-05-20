#include "IPlayer.h"

#include "Entry.h"

using namespace evwork;

IPlayer::IPlayer(int uid, evwork::IConn* pConn)
: m_uid(uid)
, m_pConn(pConn)
, m_bTimerStart(false)
{
}
IPlayer::~IPlayer()
{
	__stopTimerTick();
}

int IPlayer::getUid()
{
	return m_uid;
}

evwork::IConn* IPlayer::getConn()
{
	return m_pConn;
}

// 重新设置连接，返回旧连接
evwork::IConn* IPlayer::changeConn(evwork::IConn* pConn)
{
	evwork::IConn* pOldConn = m_pConn;

	if (pConn == m_pConn)
		return pOldConn;

	// 通知客户端被踢出
	if (m_pConn && pConn)
	{
		cbTickClient();
	}

	m_pConn = pConn;

	// 恢复客户端
	if (m_pConn)
	{
		cbRecoverClient();

		__stopTimerTick();
	}
	// 托管玩家
	else
	{
		cbTrustPlayer();

		__startTimerTick();
	}

	return pOldConn;
}

void IPlayer::__startTimerTick()
{
	if (m_bTimerStart)
		return;

	m_evTimerTickTrustMe.data = this;
	ev_timer_init(&m_evTimerTickTrustMe, IPlayer::__cbTimerTickTrustMe, 5, 5);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerTickTrustMe);

	m_bTimerStart = true;
}

void IPlayer::__stopTimerTick()
{
	if (!m_bTimerStart)
		return;

	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerTickTrustMe);

	m_bTimerStart = false;
}

void IPlayer::__tickTrustMe()
{
	if (m_pConn == NULL)
	{
		LOG(Info, "[IPlayer::%s] tick uid:[%d]", __FUNCTION__, m_uid);

		g_entry.pGame->tickPlayer(this);
	}
}

void IPlayer::__cbTimerTickTrustMe(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	IPlayer* pThis = (IPlayer*)w->data;

	pThis->__tickTrustMe();
}
