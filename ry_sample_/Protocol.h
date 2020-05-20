#pragma once 

// 视业务情况定义协议和错误码

#define SYS_ECHO				1		// 系统检测
#define SYS_ONLINE				2		// 在线人数

#define CLIENT_HEARTBEAT		1000	// 心跳
#define CLIENT_LOGIN_REQ		1001	// 登录请求
#define CLIENT_LOGOUT_REQ		1002	// 退出请求

#define CLIENT_ENTER_ROOM		1010	// 玩家请求加入房间

#define SERVER_CMD_CODE_UC		4000	// 统一响应码

#define SERVER_PLAYER_ENTER_BC	4001	// 玩家进入广播
#define SERVER_PLAYER_EXIT_BC	4002	// 玩家退出广播
#define SERVER_TABLE_INFO_UC	4003	// 桌游信息响应

#define CODE_SUCCESS			0		// 无错误
#define CODE_LOGIN_AUTH			1		// 认证失败
#define CODE_LOGIN_INIT			2		// 玩家初使化失败
#define CODE_OTHER_LOGIN		3		// 异地登录被踢出
#define CODE_NO_FREE_TABLE		4		// 无空桌
#define CODE_TABLE_NOTEXIST		5		// 桌子不存在
#define CODE_SEAT_NOFREE		6		// 无空位
#define CODE_MONEY_LESS			7		// 金币不足
#define CODE_GAMEING			8		// 游戏中，禁止操作