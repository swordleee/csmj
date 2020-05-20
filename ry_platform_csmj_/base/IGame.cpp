#include "IGame.h"

#include "Entry.h"
#include "../Protocol.h"

using namespace evwork;

BEGIN_FORM_MAP(IGame)
	ON_REQUEST_CONN(SYS_ECHO, &IGame::onSysEcho)
	ON_REQUEST_CONN(SYS_ONLINE, &IGame::onSysOnline)
	ON_REQUEST_CONN(CLIENT_HEARTBEAT, &IGame::onHeartBeat)
	ON_REQUEST_CONN(CLIENT_LOGIN_REQ, &IGame::onLoginReq)
	ON_REQUEST_CONN(CLIENT_LOGOUT_REQ, &IGame::onLogoutReq)
END_FORM_MAP()

IGame::IGame()
{
	m_evTimerTickConn.data = this;
	ev_timer_init(&m_evTimerTickConn, IGame::__cbTimerTickConn, 1, 1);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerTickConn);

	m_evTimerPrint.data = this;
	ev_timer_init(&m_evTimerPrint, IGame::__cbTimerPrint, 10, 10);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerPrint);
}
IGame::~IGame()
{
	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerTickConn);

	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerPrint);
}

// 连接关注

void IGame::onConnected(evwork::IConn* pConn)
{
	LOG(Info, "[IGame::%s] #conn# conn:[%d]", __FUNCTION__, pConn->getcid());
}

void IGame::onClose(evwork::IConn* pConn)
{
	LOG(Info, "[IGame::%s] #conn# conn:[%d]", __FUNCTION__, pConn->getcid());

	m_setConnTick.erase(pConn);

	IPlayer* pPlayer = findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
		return;

	LOG(Info, "[IGame::%s] #login# uid:[%d] conn:[%d] closed", __FUNCTION__, pPlayer->getUid(), pConn->getcid());

	if (!pPlayer->cbTryLogout(false))
	{
		pPlayer->changeConn(NULL);

		m_mapCidPlayer.erase(pConn->getcid());
	}
	else
	{
		m_mapCidPlayer.erase(pConn->getcid());
		m_mapUidPlayer.erase(pPlayer->getUid());

		__destroyPlayer(pPlayer);
	}
}

// 协议处理

void IGame::onSysEcho(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	LOG(Info, "[IGame::%s] conn:[%d] sys echo", __FUNCTION__, pConn->getcid());

	packet.end();
	sendPacket(pConn, packet.tostring());
}

void IGame::onSysOnline(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	std::string strPeerIp = "";
	uint16_t uPeerPort16 = 0;
	pConn->getPeerInfo(strPeerIp, uPeerPort16);

	LOG(Info, "[IGame::%s] from:[%s:%u] get online:[%u]", __FUNCTION__, strPeerIp.c_str(), uPeerPort16, m_mapUidPlayer.size());

	Jpacket packet_r;
	packet_r.val["cmd"] = SYS_ONLINE;
	packet_r.val["online"] = (int)m_mapUidPlayer.size();
	packet_r.end();

	sendPacket(pConn, packet_r.tostring());
}

void IGame::onHeartBeat(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	LOG(Info, "[IGame::%s] conn:[%d] heartbeat", __FUNCTION__, pConn->getcid());

	packet.end();
	sendPacket(pConn, packet.tostring());
}

void IGame::onLoginReq(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	Json::Value &val = packet.tojson();

	int uid = val.get("uid", -1).asInt();

	if (uid == -1)
	{
		LOG(Error, "[IGame::%s] #error# conn:[%d] format error!", __FUNCTION__, pConn->getcid());
		return;
	}

	LOG(Info, "[IGame::%s] #login# uid:[%d] conn:[%d] login...", __FUNCTION__, uid, pConn->getcid());

	if (!__loginCheck(packet, pConn))
		return;

	IPlayer* pPlayer = findPlayerByUid(uid);

	if (pPlayer)
	{
		evwork::IConn* pOldConn = pPlayer->changeConn(pConn);

		if (pOldConn != pConn && pOldConn)
		{
			m_mapCidPlayer.erase(pOldConn->getcid());
		}
	}
	else
	{
		if ( NULL == (pPlayer = __createPlayer(uid, pConn)) )
		{
			LOG(Error, "[IGame::%s] #error# #login# uid:[%d] conn:[%d] init player failed!", __FUNCTION__, uid, pConn->getcid());
			return;
		}
	}

	m_mapUidPlayer[pPlayer->getUid()] = pPlayer;
	m_mapCidPlayer[pConn->getcid()] = pPlayer;

	pPlayer->cbLoginSuccess();

	LOG(Info, "[IGame::%s] #login# uid:[%d] conn:[%d] login ok!", __FUNCTION__, uid, pConn->getcid());
}

void IGame::onLogoutReq(evwork::Jpacket& packet, evwork::IConn* pConn)
{
	IPlayer* pPlayer = findPlayerByCid(pConn->getcid());
	if (pPlayer == NULL)
	{
		LOG(Error, "[IGame::%s] #error# conn:[%d] not login", __FUNCTION__, pConn->getcid());
		return;
	}

	LOG(Info, "[IGame::%s] #login# uid:[%d] conn:[%d] logout...", __FUNCTION__, pPlayer->getUid(), pConn->getcid());

	if (!pPlayer->cbTryLogout(true))
	{
		LOG(Error, "[IGame::%s] #error# #login# uid:[%d] conn:[%d] cbTryLogout(true) failed", __FUNCTION__, pPlayer->getUid(), pConn->getcid());
		return;
	}

	LOG(Info, "[IGame::%s] #login# uid:[%d] conn:[%d] logout ok!", __FUNCTION__, pPlayer->getUid(), pConn->getcid());

	m_mapCidPlayer.erase(pConn->getcid());
	m_mapUidPlayer.erase(pPlayer->getUid());

	__destroyPlayer(pPlayer);
}

// 发送报文

void IGame::sendPacket(int uid, const std::string& strPacket)
{
	MAP_UID_PLAYER_t::iterator iter = m_mapUidPlayer.find(uid);
	if (iter == m_mapUidPlayer.end())
		return;

	sendPacket(iter->second, strPacket);
}

void IGame::sendPacket(IPlayer* pPlayer, const std::string& strPacket)
{
	if (pPlayer->getConn())
	{
		sendPacket(pPlayer->getConn(), strPacket);
	}
}

void IGame::sendPacket(evwork::IConn* pConn, const std::string& strPacket)
{
	pConn->sendBin(strPacket.data(), strPacket.size());
}

void IGame::sendCode(int uid, int cmd, int code)
{
	Jpacket packet;
	packet.val["cmd"] = SERVER_CMD_CODE_UC;
	packet.val["cmd_req"] = cmd;
	packet.val["code_res"] = code;
	packet.end();

	sendPacket(uid, packet.tostring());
}

void IGame::sendCode(IPlayer* pPlayer, int cmd, int code)
{
	Jpacket packet;
	packet.val["cmd"] = SERVER_CMD_CODE_UC;
	packet.val["cmd_req"] = cmd;
	packet.val["code_res"] = code;
	packet.end();

	sendPacket(pPlayer, packet.tostring());
}

void IGame::sendCode(evwork::IConn* pConn, int cmd, int code)
{
	Jpacket packet;
	packet.val["cmd"] = SERVER_CMD_CODE_UC;
	packet.val["cmd_req"] = cmd;
	packet.val["code_res"] = code;
	packet.end();

	sendPacket(pConn, packet.tostring());
}

// 广播报文
void IGame::bcbPacket(const std::string& strPacket)
{
	for (MAP_UID_PLAYER_t::iterator iter = m_mapUidPlayer.begin(); iter != m_mapUidPlayer.end(); ++iter)
	{
		sendPacket(iter->second, strPacket);
	}
}

// 查找玩家

IPlayer* IGame::findPlayerByUid(int uid)
{
	MAP_UID_PLAYER_t::iterator iter = m_mapUidPlayer.find(uid);
	if (iter == m_mapUidPlayer.end())
		return NULL;

	return iter->second;
}

IPlayer* IGame::findPlayerByCid(int cid)
{
	MAP_CID_PLAYER_t::iterator iter = m_mapCidPlayer.find(cid);
	if (iter == m_mapCidPlayer.end())
		return NULL;

	return iter->second;
}

// 踢掉玩家

bool IGame::tickPlayer(int uid)
{
	IPlayer* pPlayer = findPlayerByUid(uid);

	if (pPlayer == NULL)
		return false;

	return tickPlayer(pPlayer);
}

bool IGame::tickPlayer(IPlayer* pPlayer)
{
	LOG(Info, "[IGame::%s] #login# player uid:[%d] tick...", __FUNCTION__, pPlayer->getUid());

	IConn* pConn = pPlayer->getConn();

	std::stringstream ssConn;
	if (pConn)
		ssConn << pConn->getcid();
	else
		ssConn << "NULL";

	if (!pPlayer->cbTryLogout(false))
	{
		LOG(Error, "[IGame::%s] #error# #login# player uid:[%d] conn:[%s] cbTryLogout(false) failed", __FUNCTION__, pPlayer->getUid(), ssConn.str().c_str());
		return false;
	}

	LOG(Info, "[IGame::%s] #login# player uid:[%d] conn:[%s] tick ok!", __FUNCTION__, pPlayer->getUid(), ssConn.str().c_str());

	m_mapCidPlayer.erase(pConn->getcid());
	m_mapUidPlayer.erase(pPlayer->getUid());

	__destroyPlayer(pPlayer);

	if (pConn)
	{
		__tickConn(pConn);
	}

	return true;
}

void IGame::__tickConn(evwork::IConn* pConn)
{
	m_setConnTick.insert(pConn);
}

void IGame::__delConns()
{
	SET_CONN_t setConnTick = m_setConnTick;

	for (SET_CONN_t::iterator iter = setConnTick.begin(); iter != setConnTick.end(); ++iter)
	{
		delete (*iter);
	}

	m_setConnTick.clear();
}

void IGame::__printInfo()
{
	LOG(Info, "[IGame::%s] connections:[%d] players:[%d]", __FUNCTION__, m_mapCidPlayer.size(), m_mapUidPlayer.size());

	for (MAP_UID_PLAYER_t::iterator iter = m_mapUidPlayer.begin(); iter != m_mapUidPlayer.end(); ++iter)
	{
		IPlayer* pPlayer = iter->second;

		if (pPlayer->getConn() == NULL)
		{
			LOG(Info, "[IGame::%s] player uid:[%d] is trust!", __FUNCTION__, pPlayer->getUid());
		}
	}
}

void IGame::__cbTimerTickConn(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	IGame* pThis = (IGame*)w->data;

	pThis->__delConns();
}

void IGame::__cbTimerPrint(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	IGame* pThis = (IGame*)w->data;

	pThis->__printInfo();
}
