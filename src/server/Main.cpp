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
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "SocketMgr.h"

boost::asio::io_service _ioService;

int main()
{
	std::cout << "Ships: Start server" << std::endl;

	sSocketMgr.StartNetwork(_ioService, "0.0.0.0", 8085);
	_ioService.run();

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	sSocketMgr.StopNetwork();

	system("pause");
}