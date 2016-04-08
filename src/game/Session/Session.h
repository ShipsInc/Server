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

#include "Server.h"
#include "Define.h"
#include "Socket.h"

#define SOCKET_TIMEOUT 30000

class Packet;

class Session
{
    public:
        Session(uint32 id, std::string&& name, std::shared_ptr<Socket> sock);
        ~Session();

        bool Update(uint32 diff);

        uint32 GetAccountId() const { return m_accountId; }
        std::string const& GetAccountName() const { return m_accountName; }
        std::string const& GetRemoteAddress() const { return m_Address; }

        void KickPlayer();

        void UpdateTimeOutTime(uint32 diff)
        {
            m_timeOutTime -= int32(diff);
        }

        void ResetTimeOutTime()
        {
            m_timeOutTime = SOCKET_TIMEOUT;
        }

        bool IsConnectionIdle() const
        {
            return m_timeOutTime <= 0;
        }

        void SendPacket(Packet const* packet);
        void QueuePacket(Packet* packet);

        void Handle_NULL(Packet& recvPacket);
    private:
        std::shared_ptr<Socket> m_Socket;
        std::atomic<int32> m_timeOutTime; // Socket timeout

        uint32 m_accountId;
        std::string m_accountName;

        std::string m_Address;

        bool m_forceExit;

        LockedQueue<Packet*> _recvQueue;

        // Disable copy
        Session(Session const& right) = delete;
        Session& operator=(Session const& right) = delete;
};
