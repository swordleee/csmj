#pragma once 

struct EventLog 
{
	int uid;           // uid为0，表示台费
	int tid;
	int vid;
	int zid;
	int type;          // 流水类型 200玩牌 201大喇叭 202淘汰场金币 203兑奖券 204RMB 205彩票 206互动表情
	int alter_type;    // 更改类型   1 rmb  2 money  3 coin
	int alter_value;   // 更改的值
	int current_value; // 当前的值
	int ts;            // 当前时间戳 秒
	int usec;          // 当前时间戳 微秒
};

class CEventLog
{
public:
	int commit_eventlog(int my_uid, int my_tid, int my_alter_value, int my_current_value, int my_type, int my_alter_type, bool bRsyslog);

private:
	int __incr_eventlog(EventLog &el);

	int __incr_eventlog2(EventLog &el);
};
