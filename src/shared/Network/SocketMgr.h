/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SocketMgr_h__
#define SocketMgr_h__

#include "AsyncAcceptor.h"
#include "Define.h"
#include "NetworkThread.h"
#include "Socket.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>

using boost::asio::ip::tcp;

class SocketMgr
{
public:
    ~SocketMgr()
    {
        //ASSERT(!_threads && !_acceptor && !_threadCount, "StopNetwork must be called prior to SocketMgr destruction");
    }

	static SocketMgr& Instance()
	{
		static SocketMgr instance;
		return instance;
	}

	bool StartNetwork(boost::asio::io_service& service, std::string const& bindIp, uint16 port);
	void StopNetwork();
	void Wait();
	void OnSocketOpen(tcp::socket&& sock);
    int32 GetNetworkThreadCount() const { return _threadCount; }
protected:
    SocketMgr() : _acceptor(nullptr), _threads(nullptr), _threadCount(1) { }

	NetworkThread* CreateThreads()
	{
		return new NetworkThread[GetNetworkThreadCount()];
	}

    AsyncAcceptor* _acceptor;
    NetworkThread* _threads;
    int32 _threadCount;
};

#define sSocketMgr SocketMgr::Instance()

#endif // SocketMgr_h__
