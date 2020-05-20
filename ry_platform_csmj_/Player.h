#pragma once 

#include "base/IPlayer.h"

class CTable;

class CPlayer
	: public IPlayer
{
public:

	static CPlayer* getPlayer(int uid);
	static void freePlayer(CPlayer* p);

	CPlayer(int uid, evwork::IConn* pConn); 
	virtual ~CPlayer();

	// 初使化玩家
	bool init();

	// 更新资料
	bool updateInfo();

	// 更新金币
	bool updateMoney();

	// 更新游戏场次
	bool incPlayCount();

	// 设置zid
	void setZid(int zid);
	void setVid(int vid);

	// 操作房卡
	bool incRoomCard(int card, int& curCard);

	// 操作金币
	bool incMoney(long long int llMoney);

	// 进入桌子，初使化相关资源
	void initForEnterTable();
	// 退出桌子，清理相关资源
	void cleanForExitTable();

public:
	// 框架模板

	// 通知玩家登录成功
	virtual void cbLoginSuccess();

	// 尝试玩家退出，参数表示是否发送退出响应，返回true表示退出成功
	virtual bool cbTryLogout(bool bReponse = false);

	// 恢复客户端
	virtual void cbRecoverClient();

	// 托管玩家
	virtual void cbTrustPlayer();

	// 通知客户端被踢出
	virtual void cbTickClient();

private:

	CTable* __getTable();

public:

	int m_tid;
	int m_seatid;

	std::string			m_name;
	int					m_sex;
	std::string			m_avatar;
	int					m_avatar_auth;
	std::string			m_ps;
	long long int		m_money;
	int					m_vlevel;
	int					m_gid;

	int					m_rmb;
	int					m_exp;
};
