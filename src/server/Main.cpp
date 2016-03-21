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

#include <iostream>
#include "SocketMgr.h"
#include "Timer.h"
#include "Server.h"

#include <boost/asio/io_service.hpp>
#include <boost/bind/bind.hpp>

boost::asio::io_service _ioService;

#define SERVER_SLEEP_CONST 50
#define PORT 8085
#define THREAD_POOL 2

void ServerUpdateLoop()
{
    uint32 realCurrTime = 0;
    uint32 realPrevTime = getMSTime();

    uint32 prevSleepTime = 0;

    ///- Work server
    while (!Server::IsStopped())
    {
        ++Server::m_serverLoopCounter;
        realCurrTime = getMSTime();

        uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);

        sServer->Update(diff);
        realPrevTime = realCurrTime;

        if (diff <= SERVER_SLEEP_CONST + prevSleepTime)
        {
            prevSleepTime = SERVER_SLEEP_CONST + prevSleepTime - diff;
            std::this_thread::sleep_for(std::chrono::milliseconds(prevSleepTime));
        }
        else
            prevSleepTime = 0;
    }
}

int main()
{
	std::cout << "Ships: Start server" << std::endl;

    sSocketMgr.StartNetwork(_ioService, "0.0.0.0", PORT);

    // Start the Boost based thread pool
    int numThreads = THREAD_POOL;
    std::vector<std::thread> threadPool;
    if (numThreads < 1)
        numThreads = 1;

    for (int i = 0; i < numThreads; ++i)
        threadPool.push_back(std::thread(boost::bind(&boost::asio::io_service::run, &_ioService)));

    ServerUpdateLoop();

	sSocketMgr.StopNetwork();
}
