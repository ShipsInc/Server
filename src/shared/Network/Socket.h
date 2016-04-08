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

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <mutex>
#include <memory>

#include "Define.h"
#include "Opcodes.h"
#include "MessageBuffer.h"

#include <boost/asio/ip/tcp.hpp>

class MessageBuffer;
class Session;
class Packet;

struct ClientHeader
{
    uint16 Command;
    uint16 Size;

    static bool IsValidSize(uint32 size) { return size < 10240; }
    static bool IsValidOpcode(uint32 opcode) { return opcode < NUM_OPCODE_HANDLERS; }
};

#define READ_BLOCK_SIZE 4096

class Socket : public std::enable_shared_from_this<Socket>
{
public:
     explicit Socket(boost::asio::ip::tcp::socket&& socket);
    ~Socket();

    bool Update();

    boost::asio::ip::address GetRemoteIpAddress() const;
    uint16 GetRemotePort() const;
    void SetKeepAlive(bool set);

    void AsyncRead();
    void Start();

    void CloseSocket();

    bool IsOpen() const { return !_closed && !_closing; }

    /// Marks the socket for closing after write buffer becomes empty
    void DelayedCloseSocket() { _closing = true; }

    MessageBuffer& GetReadBuffer() { return _readBuffer; }
protected:
    std::mutex _writeLock;
    std::queue<MessageBuffer> _writeQueue;
    MessageBuffer _writeBuffer;
    boost::asio::io_service& io_service() { return _socket.get_io_service(); }
private:
    void ReadHandlerInternal(boost::system::error_code error, size_t transferredBytes);
    void ReadHandler();

    bool ReadHeaderHandler();
    void WriteHandlerWrapper(boost::system::error_code /*error*/, std::size_t /*transferedBytes*/);
    bool WriteHandler(std::unique_lock<std::mutex>& guard);
    bool HandleQueue(std::unique_lock<std::mutex>& guard);

    // Handlers
    void HandleAuth(Packet& packet);
public:
    void SendPacket(Packet const& packet);
    void SetSession(Session* session);
private:
    void QueuePacket(MessageBuffer&& buffer, std::unique_lock<std::mutex>&);
    bool AsyncProcessQueue(std::unique_lock<std::mutex>&);
    bool ReadDataHandler();
    void WritePacketToBuffer(Packet const& packet, MessageBuffer& buffer);

    std::mutex _sessionLock;
    Session* _session;
    bool _authed;

    boost::asio::ip::address _remoteAddress;
    uint16 _remotePort;

    boost::asio::ip::tcp::socket _socket;

    MessageBuffer _readBuffer;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;

    std::atomic<bool> _closed;
    std::atomic<bool> _closing;

    bool _isWritingAsync;
};

#endif // __SOCKET_H__
