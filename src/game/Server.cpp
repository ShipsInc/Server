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
#include "Socket.h"
#include "Session.h"

std::atomic<bool> Server::m_stopEvent(false);
std::atomic<uint32> Server::m_serverLoopCounter(0);

void Server::Update(uint32 diff)
{
    UpdateSessions(diff);
}

void Server::UpdateSessions(uint32 diff)
{
    ///- Add new sessions
    Session* sess = NULL;
    while (m_sessionQueue.next(sess))
        AddSession_(sess);

    ///- Then send an update signal to remaining ones
    if (!m_sessions.empty())
    {
        for (SessionMap::iterator itr = m_sessions.begin(), next; itr != m_sessions.end(); itr = next)
        {
            next = itr;
            ++next;

            ///- and remove not active sessions from the list
            Session* pSession = itr->second;
            if (!pSession->Update(diff))
            {
                m_sessions.erase(itr);
                delete pSession;

            }
        }
    }
}

Session* Server::FindSession(uint32 id) const
{
    SessionMap::const_iterator itr = m_sessions.find(id);
    if (itr != m_sessions.end())
        return itr->second;                                 // also can return NULL for kicked session

    return nullptr;
}

void Server::RemoveSession(uint32 id)
{
    SessionMap::const_iterator itr = m_sessions.find(id);
    if (itr != m_sessions.end() && itr->second)
        itr->second->KickPlayer();
}

void Server::AddSession(Session* s)
{
    m_sessionQueue.add(s);
}

void Server::AddSession_(Session* s)
{
    RemoveSession(s->GetAccountId());
    m_sessions[s->GetAccountId()] = s;
}
