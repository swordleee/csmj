#pragma once 

#define GAME_TYPE_CHANGSHA			1		// 长沙麻将

#define SYS_ECHO					1		// 系统检测
#define SYS_ONLINE					2		// 在线人数
#define SYS_RECORDS					3		// 牌局记录
#define SYS_SCORE1_UPLOAD			4		// 总战绩上传
#define SYS_SCORE2_UPLOAD			5		// 每局战绩上传

#define CLIENT_HEARTBEAT			1000	// 心跳
#define CLIENT_LOGIN_REQ			1001	// 登录请求
#define CLIENT_LOGOUT_REQ			1002	// 退出请求
#define CLIENT_CHAT_REQ				1003	// 聊天请求
#define CLIENT_UINFO_UPDATE			1004	// 玩家信息更新
#define CLIENT_PLAYER_SHARE			1005	// 玩家数据分享

#define CLIENT_ENTER_ROOM			1010	// 玩家请求加入房间
#define CLIENT_DISSOLVE_ROOM		1011	// 玩家请求解散房间
#define CLIENT_ACK_DISSOLVE_ROOM	1012	// 玩家应答解散房间
#define CLIENT_IAM_READY			1013	// 玩家准备开始游戏
#define CLIENT_PLAYER_PUTCARD		1014	// 玩家出牌
#define CLIENT_PLAYER_ACTION		1015	// 玩家执行某些操作
#define CLIENT_PLAYER_TING_TP		1016	// 玩家请求听牌提示
#define CLIENT_TICK_PLAYER			1017	// 踢人请求
#define CLIENT_TRANS_OWNER			1018	// 转让房主

#define SERVER_COMPATIBLE_LOGIN_UC  4000    // 登录响应兼容协议
#define SERVER_CMD_CODE_UC			4001	// 统一响应码
#define SERVER_TABLE_INFO_UC		4002	// 桌子信息响应
#define SERVER_PLAYER_ENTER_BC		4003	// 服务器广播玩家进入房间
#define SERVER_PLAYER_EXIT_BC		4004	// 服务器广播玩家退出房间
#define SERVER_CHAT_BC				4005	// 聊天广播
#define SERVER_UINFO_UPDATE_BC		4006	// 玩家信息更新
#define SERVER_PLAYER_SHARE			4007	// 玩家数据分享

#define SERVER_ASK_DISSOLVE_ROOM_BC 4010	// 服务器询问解散房间
#define SERVER_ACK_DISSOLVE_ROOM_BC 4011	// 服务器广播解散应答
#define SERVER_DISSOLVE_ROOM_RESULT_BC 4012	// 服务器广播房间解散结果
#define SERVER_PLAYER_READY_BC		4013	// 服务器广播玩家准备好	
#define SERVER_GAMEREADY_BC			4014	// 服务器广播游戏开始
#define SERVER_GAMESTATE_BC			4015	// 服务器广播游戏状态变更
#define SERVER_CUR_BANKER_BC		4016	// 服务器广播当前庄家
#define SERVER_FIRSTCARD_UC			4017	// 服务器向每个玩家起牌
#define SERVER_FIRSTCARD_HU_BC		4018	// 服务器广播玩家起牌胡
#define SERVER_CUR_PUTPLAYER_BC		4019	// 服务器广播当前出牌方
#define SERVER_NOTIFY_PUTCARD_UC	4020	// 服务器提醒玩家出牌
#define SERVER_PLAYER_PUTCARD_BC	4021	// 服务器广播玩家出牌
#define SERVER_SEND_CARD_BC			4022	// 服务器广播向玩家发牌
#define SERVER_NOTIFY_ACTION_UC		4023	// 服务器提醒玩家执行某些操作
#define SERVER_HAIDICARD_BC			4024	// 服务器广播当前海底牌
#define SERVER_ACTION_RESULT_BC		4025	// 服务器广播玩家操作类型及手牌变化
#define SERVER_SHOWCARD_BC			4026	// 服务器广播全部翻牌
#define SERVER_BALANCE_BC			4027	// 服务器广播结算
#define SERVER_ROOM_EXPIRE_BC		4028	// 服务器广播房间次数用玩
#define SERVER_GANG_SEND_CARD_BC	4029	// 服务器广播杠上发牌
#define SERVER_ZHANIAO_BC			4030	// 服务器广播扎鸟
#define SERVER_HAIDI_ASK_BC			4031	// 服务器广播海底牌要不要询问
#define SERVER_HAIDI_ACK_BC			4032	// 服务器广播海底牌要不要回答
#define SERVER_HAIDI_SEND_CARD_BC	4033	// 服务器广播发海底牌
#define SERVER_TOTAL_BALANCE_BC		4034	// 服务器广播总结算
#define SERVER_PLAYER_TING_UC		4035	// 服务器通知玩家听牌
#define SERVER_PLAYER_TING_TP_UC	4036	// 服备器响应玩家听牌提示
#define SERVER_PLAYER_TICKED_BC		4037	// 服务器通知玩家被踢出
#define SERVER_OWNER_TRANSED_BC		4038	// 服务器通知房主变更


#define CODE_SUCCESS				0		// 操作成功
#define CODE_LOGIN_AUTH				1		// 认证失败
#define CODE_LOGIN_INIT				2		// 玩家初使化失败
#define CODE_OTHER_LOGIN			3		// 异地登录被踢出
#define CODE_NO_PRIVILEGE			4		// 无操作权限
#define CODE_GAMEING				5		// 游戏中，禁止操作
#define CODE_TABLE_NOTEXIST			6		// 桌子不存在
#define CODE_SEAT_NOFREE			7		// 无空闲的位子
#define CODE_CARDS_INVALID			8		// 牌型不合法
#define CODE_CANNOT_CHANCARDS		9		// 听牌状态，禁止换牌


#define ACTION_PASS					1	// 跳过
#define ACTION_HAIDI_YAO			2	// 海底要牌
#define ACTION_HAIDI_BUYAO			3	// 海底不要
#define ACTION_CHI					4	// 吃牌
#define ACTION_PENG					5	// 碰牌
#define ACTION_BU					6	// 补牌
#define ACTION_GANG					7	// 杠牌
#define ACTION_HU					8	// 胡牌
#define ACTION_GANGHU				9	// 杠上胡牌
#define ACTION_HAIDIHU				10  // 海底胡牌
#define ACTION_QIANGGANGHU			11	// 抢杠胡
#define ACTION_QIANGBUHU			12	// 抢补胡

#define HU_TYPE_NORMAL				1	// 小平胡
#define HU_TYPE_ZIMO				2	// 自摸
#define HU_TYPE_SIXI				3   // 大四喜
#define HU_TYPE_BANBAN				4	// 板板胡
#define HU_TYPE_QUEYISE				5	// 缺一色
#define HU_TYPE_66SHUN				6	// 66顺
#define HU_TYPE_PENGPENG			7	// 碰碰胡
#define HU_TYPE_JIANGJIANG			8	// 将将胡
#define HU_TYPE_QINGYISE			9	// 清一色
#define HU_TYPE_QUANQIUREN			10	// 全求人
#define HU_TYPE_7DUIZI				11	// 七对子
#define HU_TYPE_HAOHUA7DUI			12	// 豪华七对子
#define HU_TYPE_CHAOHAOHUA7DUI		13	// 超豪华七对子
#define HU_TYPE_HAIDILAOYUE			14	// 海底捞月
#define HU_TYPE_GANGSHANGKAIHUA		15	// 杠上开花
#define HU_TYPE_QIANGGANG			16	// 抢杠胡
#define HU_TYPE_GANGSHANGPAO		17	// 杠上炮


#define RECORD_TYPE_INIT			0	// 记录类型：初使化
#define RECORD_TYPE_FIRSTCARD		1	// 记录类型：开始发牌
#define RECORD_TYPE_QIBAIHU			2	// 记录类型：起牌胡
#define RECORD_TYPE_PUTCARD			3	// 记录类型：玩家出牌
#define RECORD_TYPE_SENDCARD		4	// 记录类型：玩家发牌
#define RECORD_TYPE_ACTION			5	// 记录类型：玩家选择操作
#define RECORD_TYPE_RESULT			6	// 记录类型：玩家操作结果
#define RECORD_TYPE_GANGCARD		7	// 记录类型：杠上发牌
#define RECORD_TYPE_HAIDI			8	// 记录类型：海底牌
#define RECORD_TYPE_BALANCE			9	// 记录类型：结算