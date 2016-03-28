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

#include "SocketMgr.h"

static void OnSocketAccept(tcp::socket&& sock)
{
    sSocketMgr.OnSocketOpen(std::forward<tcp::socket>(sock));
}

bool SocketMgr::StartNetwork(boost::asio::io_service& service, std::string const& bindIp, uint16 port, uint16 threads)
{
    _threadCount = threads;

    if (_threadCount <= 0)
    {
        std::cout << "Network.Threads is wrong in your config file" << std::endl;
        return false;
    }

    try
    {
        _acceptor = new AsyncAcceptor(service, bindIp, port);
    }
    catch (boost::system::system_error const& err)
    {
        std::cout << "Exception caught in SocketMgr.StartNetwork (" << bindIp.c_str() << ":" << port << "): " << err.what();
        return false;
    }

    _acceptor->AsyncAcceptManaged(&OnSocketAccept);

    _threads = CreateThreads();

    for (int32 i = 0; i < _threadCount; ++i)
        _threads[i].Start();

    return true;
}

void SocketMgr::StopNetwork()
{
    _acceptor->Close();

    if (_threadCount != 0)
        for (int32 i = 0; i < _threadCount; ++i)
            _threads[i].Stop();

    Wait();

    delete _acceptor;
    _acceptor = nullptr;
    delete[] _threads;
    _threads = nullptr;
    _threadCount = 0;
}

void SocketMgr::Wait()
{
    if (_threadCount != 0)
        for (int32 i = 0; i < _threadCount; ++i)
            _threads[i].Wait();
}

void SocketMgr::OnSocketOpen(tcp::socket&& sock)
{
    size_t min = 0;

    for (int32 i = 1; i < _threadCount; ++i)
        if (_threads[i].GetConnectionCount() < _threads[min].GetConnectionCount())
            min = i;

    try
    {
        std::shared_ptr<Socket> newSocket = std::make_shared<Socket>(std::move(sock));
        newSocket->Start();

        std::cout << "Socket Open" << std::endl;
        _threads[min].AddSocket(newSocket);
    }
    catch (boost::system::system_error const& err)
    {
        std::cout << "Failed to retrieve client's remote address " << err.what() << std::endl;
    }
}
