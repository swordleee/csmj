#include "EventLog.h"

#include "Entry.h"

#include <sys/time.h>

using namespace evwork;
using namespace tinyredis;

int CEventLog::commit_eventlog(int my_uid, int my_tid, int my_alter_value, int my_current_value, int my_type, int my_alter_type, bool bRsyslog)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int ts = (int)tv.tv_sec;
	int usec = (int)tv.tv_usec;

	EventLog el;
	el.uid = my_uid;
	el.tid = my_tid;
	el.vid = g_entry.conf["tables"]["vid"].asInt();
	el.zid = g_entry.conf["tables"]["zid"].asInt();
	el.type = my_type;
	el.alter_type = my_alter_type;
	el.alter_value = my_alter_value;
	el.current_value = my_current_value;
	el.ts = ts;
	el.usec = usec;

	if (bRsyslog)
		return __incr_eventlog2(el);
	else
		return __incr_eventlog(el);
}

int CEventLog::__incr_eventlog(EventLog &el)
{
	char field[32] = {0};
	snprintf(field, 31, "%d%d%d%d", el.uid, el.ts, el.tid, el.zid);

	CRedisClient* pRedis = g_entry.log_db.getRedis(0);

	CResult result(true);
	result = pRedis->command("hmset log:%s uid %d tid %d vid %d zid %d type %d alter_type %d alter_value %d current_value %d ts %d",
		field, el.uid, el.tid, el.vid, el.zid, el.type, el.alter_type, el.alter_value, el.current_value, el.ts);

	if (!result)
	{
		LOG(Error, "[CEventLog::%s] uid[%d] tid[%d] failed, desc:[%s]", __FUNCTION__, el.uid, el.tid, pRedis->getErrStr().c_str());
		return -1;
	}

	return 0;
}

int CEventLog::__incr_eventlog2(EventLog &el)
{
	char field[32] = {0};
	snprintf(field, 31, "%d%d%d%d", el.uid, el.ts, el.tid, el.zid);

	LOG(Info, "#event# log:%s uid %d tid %d vid %d zid %d type %d alter_type %d alter_value %d current_value %d ts %d",
		field, el.uid, el.tid, el.vid, el.zid, el.type, el.alter_type, el.alter_value, el.current_value, el.ts);

	return 0;
}
