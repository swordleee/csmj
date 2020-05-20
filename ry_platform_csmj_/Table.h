#pragma once 

#include "Player.h"
#include "TableAttr.h"
#include "TableLocal.h"
#include "Struct.h"

#define MAX_SEAT	4

struct SSeat
{
	int seatid;
	bool occupied;

	int uid;

	bool ready;

	static int getSeatId(int pos)
	{
		static int seatid_array[] = {1, 2, 3, 4};
		if ( pos >= (int) (sizeof(seatid_array)/sizeof(int)) )
			return -1;

		return seatid_array[pos];
	}
	static int getNextSeatid(int seatid)
	{
		static int seatid_array[] = {1, 2, 3, 4};

		int pos = -1;
		for (int i = 0; i < (int) (sizeof(seatid_array)/sizeof(int)); ++i)
		{
			if (seatid_array[i] == seatid)
			{
				pos = i;
				break;
			}
		}

		if (pos == -1)
			return -1;

		return getSeatId( (pos + 1) % MAX_SEAT );
	}

	SSeat()
	{
		seatid = -1;
		occupied = false;
		uid = -1;

		ready = false;
	}
};

enum EGameState
{
	S_INIT = 0,
	S_FIRSTCARDS,
	S_QISHOUHU,
	S_PLAY,
	S_END,
	S_GOON,
	S_OVER
};

struct STempVariable
{
	uint32_t uTotalFreeTimes;

	CARDS_t gangCards;

	std::vector<std::pair<int, int> > vecZhaniao;

	std::set<int> setCannotHu;

	STempVariable()
	{
		uTotalFreeTimes = 0;
	}

	int zhaniaoRate(int seatid)
	{
		int ret = 1;

		for (std::vector<std::pair<int, int> >::iterator iter = vecZhaniao.begin(); iter != vecZhaniao.end(); ++iter)
		{
			if (iter->second == seatid)
				ret *= 2;
		}

		return ret;
	}

	int zhaniaoCount(int seatid)
	{
		int ret = 0;

		for (std::vector<std::pair<int, int> >::iterator iter = vecZhaniao.begin(); iter != vecZhaniao.end(); ++iter)
		{
			if (iter->second == seatid)
				++ret;
		}

		return ret;
	}

	void setPassHu(int seatid)
	{
		setCannotHu.insert(seatid);
	}
	void cleanPassHu(int seatid)
	{
		setCannotHu.erase(seatid);
	}
	void cleanPassHu()
	{
		setCannotHu.clear();
	}
	bool isCannotHu(int seatid)
	{
		return (setCannotHu.find(seatid) != setCannotHu.end());
	}
};

class CDissolveSession;

class CTable
{
public:
	friend class CDissolveSession;

	CTable(int tid);
	virtual ~CTable();

	int getTid();

	bool init();

public:
	//---------------------------------------------------------------------------------
	// 框架模板

	bool enterPlayer(CPlayer* pPlayer);

	bool exitPlayer(CPlayer* pPlayer);

	void recoverPlayer(CPlayer* pPlayer);

	//---------------------------------------------------------------------------------
	// 消息回调/辅助

	void cbPlayerChat(CPlayer* pPlayer, int type, int index, const std::string& text, int chatid);

	void cbPlayerShare(CPlayer* pPlayer, const std::string& data);

	void cbPlayerReady(CPlayer* pPlayer, int type);

	void cbPlayerPutCard(CPlayer* pPlayer, Card card);

	void cbPlayerAction(CPlayer* pPlayer, const SActionParam& action);

	void cbPlayerTingTP(CPlayer* pPlayer);

	void cbTickPlayer(CPlayer* pPlayer, int seatid);

	void cbTransOwnner(CPlayer* pPlayer, int seatid);

	void cbDissolveRoom(CPlayer* pPlayer);

	void cbAckDissolveRoom(CPlayer* pPlayer, int ack);

	void cbDissolveRoomResult(int resul);

private:
	// 业务逻辑辅助

	void __initTableEntry();

	void __goonTableEntry();

	void __overTableEntry();

	void __firstCards(int bankerType);

	void __qishouHu();

	void __playCards();

	void __putCardLogic();

	void __sendCardLogic();

	void __gangSendCardLogic();
	void __gangHuAskLogic();

	void __haidiCardLogic();

	bool __checkMyGangBuButOtherHu(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __passCurAction();

	void __haidiBuYaoLogic();

	void __haidiYaoCardLogic(bool bForce);

	void __chiCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __pengCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __gangCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __buCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __huCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __ganghuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __haidihuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __qiangganghuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __qiangbuhuCardLogic(const std::map<int, SSelectAction>& mapSeatSelectAction);

	void __commonHuCalls(/*const std::vector<int>& vecHuSeatId, */bool bZhaniao);

	void __liujuLogic();

	void __calcZhaniao(bool gang);

	void __makeBalanceInfo(int huSeatId, const SSelectAction& selectAction);
	void __makeGangBalanceInfo(int huSeatId, const SSelectAction& selectAction);
	void __makeHaidiBalanceInfo(int huSeatId, const SSelectAction& selectAction);
	void __makeQiangGangBalanceInfo(int huSeatId, const SSelectAction& selectAction);
	void __makeQiangBuBalanceInfo(int huSeatId, const SSelectAction& selectAction);
	void __makeSeatHuTypes(int huSeatId, int huFlag);

	void __goonRoundLogic();

	void __checkCanDissolveRoom();

	void __tryCoolRoom();

	void __sendOptionActions(int seatid, const SOptionActionS& optionActions);

	int __decideOptionAction(std::vector<int>& vecDecideSeatId);

	bool __checkCardFromHands(const SSeatLocal& seatLocal, const Card& card);

	bool __isTableEmpty();

	bool __isAllReady();

	void __cleanPlayerReady();

	void __tryBindOwnner();

	int __getOwnnerSeatId();

	void __transBindOwnner(int fromseatid, int fromuid, int toseatid, int touid);

	void __bindPlayers();

	void __tickAllPlayer();

	int __chooseSeatId(int uid);

	std::vector<int> __getOtherSeatid(int seatid);

	SSeat& __getSeatById(int seatid);

	bool __isGameing();

	void __switchToState(const EGameState& state);

	int __convertClientGameState(int gamestate);

	// 写入牌局记录
	void __appendRecord(int type, const std::string& strRecord, bool bJsonFormat);

	// 牌局记录：初使化
	void __recordInit();

	// 牌局记录：开始发牌
	void __recordFirstcard(int bankerType);

	// 牌局记录：起牌胡
	void __recordQibaihu(const Json::Value& jsRecord);

	// 牌局记录：玩家出牌
	void __recordPutcard(int seatid, const Card& card, const CARDS_t& handCards);

	// 牌局记录：玩家发牌
	void __recordSendcard(int seatid, const Card& card, int decks_count);

	// 牌局记录：玩家选择操作
	void __recordAction(int seatid, int type);

	// 牌局记录：玩家操作结果
	void __recordResult(const Json::Value& jsRecord);

	// 牌局记录：杠上发牌
	void __recordGangcard(int seatid, const Card& card1, const Card& card2, int decks_count);

	// 牌局记录：海底牌
	void __recordHaidi(int seatid, const Card& card);

	// 牌局记录：结算
	void __recordBalance(const Json::Value& jsRecord);

private:
	// 发包辅助

	void __broadPlayerEnter(CPlayer* pPlayer);

	void __broadPlayerExit(CPlayer* pPlayer, int seatid, bool bbinded);

	void __sendTableInfo(CPlayer* pPlayer, bool bRecover);

	void __sendReentryInfo(CPlayer* pPlayer);

	void __sendDissolveSession(CPlayer* pPlayer);

	void __broadGameState();

	void __broadCurrentBanker(int select_type);

	void __broadShowCards(/*const std::vector<int>& vecHuSeatId*/);

	void __broadBalance();

	void __broadTotalBalance();

	void __sendPacketToSeatId(int seatid, const std::string& packet);

	void __broadPacket(CPlayer* pPlayer, const std::string& packet);

private:

	// 定时器相关

	void __initTimerLocal();

	void __startQiShouHu();
	void __stopQiShouHu();

	void __startPlayCards(bool bHasQiShouHu);
	void __stopPlayCards();

	void __startGoonRound();
	void __stopGoonRound();

	void __startGangHuAsk();
	void __stopGangHuAsk();

	void __doTimerDeamon();
	void __doTimerQiShouHu();
	void __doTimerPlayCards();
	void __doTimerGoonRound();
	void __doTimerGangHuAsk();

	static void __cbTimerDeamon(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void __cbTimerQiShouHu(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void __cbTimerPlayCards(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void __cbTimerGoonRound(struct ev_loop *loop, struct ev_timer *w, int revents);
	static void __cbTimerGangHuAsk(struct ev_loop *loop, struct ev_timer *w, int revents);

private:
	int m_tid;
	SSeat m_seats[MAX_SEAT];

	CTableAttr m_tableAttr;

	CTableLocal m_tableLocal;

	STempVariable m_tempVar;

	ev_timer m_evTimerDeamon;

	ev_timer m_evTimerQiShouHu;
	ev_timer m_evTimerPlayCards;
	ev_timer m_evTimerGoonRound;
	ev_timer m_evGangHuAsk;
	bool m_bStartQiShouHu;
	bool m_bStartPlayCards;
	bool m_bStartGoonRound;
	bool m_bStartGangHuAsk;

	CDissolveSession* m_pDissolveSession;
};


class CDissolveSession
{
public:
	CDissolveSession(CTable* pTable, int sponsorSeatid, int expireSession);
	~CDissolveSession();

	void askDissolve();
	void askDissolve(int seatid);

	void setSeatAck(int seatid, int ack);

private:

	void __sessionTimeout();

	void __startSessionTimer();
	void __stopSessionTimer();

	static void __cbTimerDissolveSession(struct ev_loop *loop, struct ev_timer *w, int revents);

private:
	CTable* m_pTable;

	int m_sponsorSeatid;
	int m_expireSession;

	std::map<int, int> m_mapSeatAck;

	ev_timer m_evTimerDissolveSession;
	ev_timer m_evTimerCheck;
};
