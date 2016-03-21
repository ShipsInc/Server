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

#ifndef NetworkThread_h__
#define NetworkThread_h__

#include "Define.h"
#include "Socket.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

class NetworkThread
{
public:
    NetworkThread() : _connections(0), _stopped(false), _thread(nullptr)
    {
    }

    virtual ~NetworkThread()
    {
        Stop();
        if (_thread)
        {
            Wait();
            delete _thread;
        }
    }

    void Stop()
    {
        _stopped = true;
    }

    bool Start()
    {
        if (_thread)
            return false;

        _thread = new std::thread(&NetworkThread::Run, this);
        return true;
    }

    void Wait()
    {
        //ASSERT(_thread);

        _thread->join();
        delete _thread;
        _thread = nullptr;
    }

    int32 GetConnectionCount() const
    {
        return _connections;
    }

    void AddSocket(std::shared_ptr<Socket> sock)
    {
        std::lock_guard<std::mutex> lock(_newSocketsLock);

        ++_connections;
        _newSockets.insert(sock);
    }

protected:

    void AddNewSockets()
    {
        std::lock_guard<std::mutex> lock(_newSocketsLock);

        if (_newSockets.empty())
            return;

        for (SocketSet::const_iterator i = _newSockets.begin(); i != _newSockets.end(); ++i)
        {
            if (!(*i)->IsOpen())
                --_connections;
            else
                _Sockets.insert(*i);
        }

        _newSockets.clear();
    }

    void Run()
    {
		std::cout << "Network Thread Starting" << std::endl;

        SocketSet::iterator i;

        uint32 sleepTime = 10;
        uint32 tickStart = 0, diff = 0;
        while (!_stopped)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

			tickStart = time(NULL);

            AddNewSockets();

            for (i = _Sockets.begin(); i != _Sockets.end();)
            {
                if (!(*i)->Update())
                {
                    if ((*i)->IsOpen())
                        (*i)->CloseSocket();

                    --_connections;
                    _Sockets.erase(i++);
                }
                else
                    ++i;
            }

            diff =  time(NULL) - tickStart;
            sleepTime = diff > 10 ? 0 : 10 - diff;
        }

		std::cout << "Network Thread exits" << std::endl;
        _newSockets.clear();
        _Sockets.clear();
    }

private:
    typedef std::set<std::shared_ptr<Socket>> SocketSet;

    std::atomic<int32> _connections;
    std::atomic<bool> _stopped;

    std::thread* _thread;

    SocketSet _Sockets;

    std::mutex _newSocketsLock;
    SocketSet _newSockets;
};

#endif // NetworkThread_h__
