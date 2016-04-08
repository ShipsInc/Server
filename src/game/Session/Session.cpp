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

#include "Packet.h"
#include "Socket.h"
#include "Opcodes.h"
#include "Session.h"

// Session constructor
Session::Session(uint32 id, std::string&& name, std::shared_ptr<Socket> sock) : m_accountId(id), m_accountName(std::move(name)), m_forceExit(false), m_timeOutTime(0)
{
    if (sock)
        m_Address = sock.get()->GetRemoteIpAddress().to_string();

    m_Socket = sock;
}

// Session destructor
Session::~Session()
{
    if (m_Socket)
    {
        m_Socket->CloseSocket();
        m_Socket.reset();
    }
}

void Session::KickPlayer()
{
    if (m_Socket)
    {
        m_Socket->CloseSocket();
        m_forceExit = true;
    }
}

bool Session::Update(uint32 diff)
{
    /// Update Timeout timer.
    UpdateTimeOutTime(diff);

    if (IsConnectionIdle()) // Connection socket timeout
        return false;

    Packet* packet = nullptr;
    while (m_Socket && m_Socket->IsOpen() && !_recvQueue.empty() && _recvQueue.next(packet))
    {
        OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
        try
        {
            (this->*opHandle.handler)(*packet);
        }
        catch (ByteBufferException const&)
        {
           // TC_LOG_FATAL("network", "WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %s) from client %s, accountid=%i. Skipped packet.",
           //     GetOpcodeNameForLogging(static_cast<OpcodeClient>(packet->GetOpcode())).c_str(), GetRemoteAddress().c_str(), GetAccountId());
            packet->hexlike();
            KickPlayer();
        }

        delete packet;
    }

    /// Cleanup sockets
    if (!m_Socket || m_forceExit || (m_Socket && !m_Socket->IsOpen()))
        return false;

    return true;
}

void Session::QueuePacket(Packet* packet)
{
    _recvQueue.add(packet);
}

void Session::SendPacket(Packet const* packet)
{
    m_Socket->SendPacket(*packet);
}

void Session::Handle_NULL(Packet& recvPacket)
{
    std::cout << "Session: received unimplemented opcode " << LookupOpcodeName(recvPacket.GetOpcode()) << "(" << recvPacket.GetOpcode() << ")";
}
