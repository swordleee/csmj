#include "ClientConn.h"

#include "EVWork.h"

using namespace evwork;

#define DEF_CONN_TIMEOUT	60
#define MAX_INPUT_SIZE		8*1024*1024
#define MAX_OUTPUT_SIZE		8*1024*1024
#define READ_BUFF_SIZE		8*1024

CClientConn::CClientConn(const std::string& strPeerIp, uint16_t uPeerPort16)
: m_strPeerIp(strPeerIp)
, m_uPeerPort16(uPeerPort16)
, m_bConnected(false)
, m_strInput("")
, m_strOutput("")
, m_bTimerNoDataStart(false)
, m_bTimerDestroyStart(false)
, m_hRead(this)
, m_hWrite(this)
{
	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (m_fd == -1)
		throw exception_errno( toString("[CClientConn::%s] socket(ip:%s port:%u)", __FUNCTION__, m_strPeerIp.c_str(), m_uPeerPort16) );

	__noblock();

	struct sockaddr_in sinto;
	sinto.sin_family = AF_INET;
	sinto.sin_addr.s_addr = ::inet_addr(m_strPeerIp.c_str());
	sinto.sin_port = htons(m_uPeerPort16);

	connect(m_fd, (struct sockaddr*)&sinto, sizeof(sinto));

	m_hRead.setEv(EV_READ);
	m_hRead.setFd(m_fd);

	m_hWrite.setEv(EV_WRITE);
	m_hWrite.setFd(m_fd);

	CEnv::getEVLoop()->setHandle(&m_hWrite);
}

CClientConn::CClientConn(int fd, const std::string& strPeerIp, uint16_t uPeerPort16)
: m_strPeerIp(strPeerIp)
, m_uPeerPort16(uPeerPort16)
, m_bConnected(true)
, m_strInput("")
, m_strOutput("")
, m_bTimerNoDataStart(false)
, m_bTimerDestroyStart(false)
, m_hRead(this)
, m_hWrite(this)
{
	m_fd = fd;

	__noblock();

	m_hRead.setEv(EV_READ);
	m_hRead.setFd(m_fd);

	m_hWrite.setEv(EV_WRITE);
	m_hWrite.setFd(m_fd);

	CEnv::getEVLoop()->setHandle(&m_hRead);

	__initTimerNoData();

	if (CEnv::getLinkEvent())
		CEnv::getLinkEvent()->onConnected(this);
}

CClientConn::~CClientConn()
{
	CEnv::getEVLoop()->delHandle(&m_hRead);
	CEnv::getEVLoop()->delHandle(&m_hWrite);

	__destroyTimerNoData();
	__destroyTimerDestry();

	if (CEnv::getLinkEvent())
		CEnv::getLinkEvent()->onClose(this);

	if (m_fd != -1)
	{
		close(m_fd);
		m_fd = -1;
	}
}

void CClientConn::getPeerInfo(std::string& strPeerIp, uint16_t& uPeerPort16)
{
	strPeerIp = m_strPeerIp;
	uPeerPort16 = m_uPeerPort16;
}

bool CClientConn::sendBin(const char* pData, size_t uSize)
{
	try
	{
		__appendBuffer(pData, uSize);

		if (m_bConnected)
		{
			CEnv::getEVLoop()->setHandle(&m_hWrite);
		}

		return true;
	}
	catch (exception_errno& e)
	{
		LOG(Info, "[CClientConn::%s] catch exception:[%s]", __FUNCTION__, e.what());

		__initTimerDestry();

		return false;
	}
}

void CClientConn::cbEvent(int revents)
{
	if ((revents & EV_READ) == EV_READ)
	{
		__onRead();
	}

	if ((revents & EV_WRITE) == EV_WRITE)
	{
		__onWrite();
	}
}

void CClientConn::__noblock()
{
	int nFlags = fcntl(m_fd, F_GETFL);
	if (nFlags == -1)
		throw exception_errno( toString("[CListenConn::%s] fcntl(%d, F_GETFL) failed!", __FUNCTION__, m_fd) );

	nFlags |= O_NONBLOCK;

	int nRet = fcntl(m_fd, F_SETFL, nFlags);
	if (nRet == -1)
		throw exception_errno( toString("[CListenConn::%s] fcntl(%d, F_SETFL) failed!", __FUNCTION__, m_fd) );
}

void CClientConn::__initTimerNoData()
{
	if (m_bTimerNoDataStart)
		return;

	float fTimeout = DEF_CONN_TIMEOUT;
	if (CEnv::getEVParam().uConnTimeout != (uint32_t)-1)
	{
		fTimeout = CEnv::getEVParam().uConnTimeout;
	}

	m_evTimerNoData.data = this;
	ev_timer_init(&m_evTimerNoData, CClientConn::__cbTimerNoData, fTimeout, fTimeout);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerNoData);

	m_bTimerNoDataStart = true;
}

void CClientConn::__destroyTimerNoData()
{
	if (!m_bTimerNoDataStart)
		return;

	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerNoData);

	m_bTimerNoDataStart = false;
}

void CClientConn::__updateTimerNoData()
{
	if (!m_bTimerNoDataStart)
		return;

	ev_timer_again(CEnv::getEVLoop()->getEvLoop(), &m_evTimerNoData);
}

void CClientConn::__cbTimerNoData(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CClientConn* pThis = (CClientConn*)w->data;

	LOG(Info, "[CClientConn::%s] fd:[%d] peer:[%s:%u] timeout", __FUNCTION__, pThis->m_fd, pThis->m_strPeerIp.c_str(), pThis->m_uPeerPort16);

	pThis->__willFreeMyself( "timeout" );
}

void CClientConn::__initTimerDestry()
{
	if (m_bTimerDestroyStart)
		return;

	m_evTimerDestroy.data = this;
	ev_timer_init(&m_evTimerDestroy, CClientConn::__cbTimerDestry, 0.1, 0);
	ev_timer_start(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDestroy);

	m_bTimerDestroyStart = true;
}

void CClientConn::__destroyTimerDestry()
{
	if (!m_bTimerDestroyStart)
		return;

	ev_timer_stop(CEnv::getEVLoop()->getEvLoop(), &m_evTimerDestroy);

	m_bTimerDestroyStart = false;
}

void CClientConn::__cbTimerDestry(struct ev_loop *loop, struct ev_timer *w, int revents)
{
	CClientConn* pThis = (CClientConn*)w->data;

	LOG(Info, "[CClientConn::%s] fd:[%d] peer:[%s:%u] destroy", __FUNCTION__, pThis->m_fd, pThis->m_strPeerIp.c_str(), pThis->m_uPeerPort16);

	pThis->__willFreeMyself( "destroy" );
}

void CClientConn::__onRead()
{
	__updateTimerNoData();

	while (true)
	{
		char szBuf[READ_BUFF_SIZE] = {0};
		size_t uBytesRecv = 0;

		try
		{
			uBytesRecv = __recvData(szBuf, READ_BUFF_SIZE);
		}
		catch (exception_errno& e)
		{
			__willFreeMyself( toString("exception:%s", e.what()) );
			return;
		}

		if (uBytesRecv > 0)
		{
			if (m_strInput.size() + uBytesRecv > MAX_INPUT_SIZE)
			{
				__willFreeMyself( "input buffer flow!!!" );
				return;
			}

			m_strInput.append(szBuf, uBytesRecv);
		}

		if (uBytesRecv < READ_BUFF_SIZE)
			break;
	}

	if (!m_strInput.empty())
	{
		int nRetSize = 0;

		if (CEnv::getDataEvent())
			nRetSize = CEnv::getDataEvent()->onData(this, m_strInput.data(), m_strInput.size());
		else
			nRetSize = m_strInput.size();

		if (nRetSize < 0)
		{
			__willFreeMyself( "onData" );
		}
		else if (nRetSize > 0)
		{
			m_strInput.erase(0, nRetSize);
		}
	}
}

void CClientConn::__onWrite()
{
	if (!m_bConnected)
	{
		int e = 0;
		socklen_t l = sizeof(e);

		getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &e, &l);
		if (e)
		{
			__willFreeMyself( "connect failed" );
			return;
		}

		m_bConnected = true;

		CEnv::getEVLoop()->setHandle(&m_hRead);

		__initTimerNoData();

		if (CEnv::getLinkEvent())
			CEnv::getLinkEvent()->onConnected(this);
	}

	try
	{
		__sendBuffer();
	}
	catch (exception_errno& e)
	{
		__willFreeMyself( toString("exception:%s", e.what()) );
		return;
	}

	if (m_strOutput.empty())
	{
		CEnv::getEVLoop()->delHandle(&m_hWrite);
	}
}

void CClientConn::__appendBuffer(const char* pData, size_t uSize)
{
	if (m_strOutput.size() + uSize > MAX_OUTPUT_SIZE)
		throw exception_errno( toString("[CClientConn::%s] buffer overflow", __FUNCTION__) );

	m_strOutput.append(pData, uSize);
}

void CClientConn::__sendBuffer()
{
	if (!m_strOutput.empty())
	{
		int nSendBytes = __sendData(m_strOutput.data(), m_strOutput.size());

		if (nSendBytes > 0)
			m_strOutput.erase(0, nSendBytes);
	}
}

size_t CClientConn::__sendData(const char* pData, size_t uSize)
{
	size_t bytes_total = 0;
	while (bytes_total < uSize)
	{
		int bytes_out = ::send(m_fd, (pData + bytes_total), (uSize - bytes_total), 0);
		if (bytes_out == -1)
		{
			if (errno == EINTR)
				continue;
			else if (errno == EAGAIN)
				break;
			else
				throw exception_errno( toString("[CClientConn::%s] ::send", __FUNCTION__) );
		}
		else if (bytes_out > 0)
		{
			bytes_total += (size_t)bytes_out;
		}
	}

	return bytes_total;
}

size_t CClientConn::__recvData(char* pData, size_t uSize)
{
	size_t bytes_total = 0;
	while (bytes_total < uSize)
	{
		int bytes_in = ::recv(m_fd, (pData + bytes_total), (uSize - bytes_total), 0);
		if (bytes_in == -1)
		{
			if (errno == EINTR)
				continue;
			else if (errno == EAGAIN)
				break;
			else
				throw exception_errno( toString("[CClientConn::%s] ::recv", __FUNCTION__) );
		}
		else if (bytes_in == 0)
		{
			throw exception_errno( toString("[CClientConn::%s] ::recv peer close", __FUNCTION__) );
		}
		else
		{
			bytes_total += (size_t)bytes_in;
		}
	}

	return bytes_total;
}

void CClientConn::__willFreeMyself(const std::string& strDesc)
{
	LOG(Info, "[CClientConn::%s] fd:[%d] peer:[%s:%u], %s, delete myself", __FUNCTION__, m_fd, m_strPeerIp.c_str(), m_uPeerPort16, strDesc.c_str());

	delete this;
}
