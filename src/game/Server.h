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

#ifndef __SERVER_H
#define __SERVER_H

#include "Timer.h"
#include "Define.h"
#include "LockedQueue.h"

class Session;

#include <unordered_map>

typedef std::unordered_map<uint32, Session*> SessionMap;

/// The World
class Server
{
    public:
        static Server* instance()
        {
            static Server instance;
            return &instance;
        }

        static std::atomic<uint32> m_serverLoopCounter;

        // Sessions
        Session* FindSession(uint32 id) const;
        void AddSession(Session* s);
        void RemoveSession(uint32 id);
        void AddSession_(Session* s);
        uint32 GetActiveSessions() const { return m_sessions.size(); }

        void Update(uint32 diff);
        void UpdateSessions(uint32 diff);

        static bool IsStopped() { return m_stopEvent; }
    private:
        static std::atomic<bool> m_stopEvent;
        LockedQueue<Session*> m_sessionQueue;
        SessionMap m_sessions;
};

#define sServer Server::instance()
#endif
