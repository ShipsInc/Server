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

#include "MessageBuffer.h"
#include "Opcodes.h"
#include "Packet.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <queue>
#include <memory>
#include <functional>
#include <type_traits>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

class Session;

using boost::asio::ip::tcp;

using namespace boost::asio;

struct ClientHeader
{
    uint16 Size;
    uint16 Command;

    static bool IsValidSize(uint32 size) { return size < 10240; }
    static bool IsValidOpcode(uint32 opcode) { return opcode < NUM_OPCODE_HANDLERS; }
};

#define READ_BLOCK_SIZE 4096
#ifdef BOOST_ASIO_HAS_IOCP
#define TC_SOCKET_USE_IOCP
#endif

class Socket : public std::enable_shared_from_this<Socket>
{
public:
    explicit Socket(tcp::socket&& socket);
    ~Socket();

    void Start()
    {
        AsyncRead();
    }

    bool Update()
    {
        if (!IsOpen())
            return false;

#ifndef TC_SOCKET_USE_IOCP
        std::unique_lock<std::mutex> guard(_writeLock);
        if (!guard)
            return true;

        if (_isWritingAsync || (!_writeBuffer.GetActiveSize() && _writeQueue.empty()))
            return true;

        for (; WriteHandler(guard);)
            ;
#endif

        return true;
    }

    boost::asio::ip::address GetRemoteIpAddress() const
    {
        return _remoteAddress;
    }

    uint16 GetRemotePort() const
    {
        return _remotePort;
    }

    void AsyncRead()
    {
        if (!IsOpen())
            return;

        _readBuffer.Normalize();
        _readBuffer.EnsureFreeSpace();
        _socket.async_read_some(boost::asio::buffer(_readBuffer.GetWritePointer(), _readBuffer.GetRemainingSpace()), std::bind(&Socket::ReadHandlerInternal, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }

    void QueuePacket(MessageBuffer&& buffer, std::unique_lock<std::mutex>& guard)
    {
        _writeQueue.push(std::move(buffer));

#ifdef TC_SOCKET_USE_IOCP
        AsyncProcessQueue(guard);
#else
        (void)guard;
#endif
    }

    bool IsOpen() const { return !_closed && !_closing; }

    void CloseSocket();

    /// Marks the socket for closing after write buffer becomes empty
    void DelayedCloseSocket() { _closing = true; }

    MessageBuffer& GetReadBuffer() { return _readBuffer; }
protected:
    bool AsyncProcessQueue(std::unique_lock<std::mutex>&)
    {
        if (_isWritingAsync)
            return false;

        _isWritingAsync = true;

#ifdef TC_SOCKET_USE_IOCP
        MessageBuffer& buffer = _writeQueue.front();
        //_socket.async_write_some(boost::asio::buffer(buffer.GetReadPointer(), buffer.GetActiveSize()), std::bind(&Socket::WriteHandler,
       //     std::placeholders::_1, std::placeholders::_2));
#else
        _socket.async_write_some(boost::asio::null_buffers(), std::bind(&Socket::WriteHandlerWrapper,
            this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
#endif

        return false;
    }

    std::mutex _writeLock;
    std::queue<MessageBuffer> _writeQueue;
#ifndef TC_SOCKET_USE_IOCP
    MessageBuffer _writeBuffer;
#endif

    boost::asio::io_service& io_service() { return _socket.get_io_service(); }

private:
    void ReadHandlerInternal(boost::system::error_code error, size_t transferredBytes)
    {
        if (error)
        {
            CloseSocket();
            return;
        }

		std::cout << "Recive bytes: " << transferredBytes << std::endl;

        _readBuffer.WriteCompleted(transferredBytes);
        ReadHandler();
    }

#ifdef TC_SOCKET_USE_IOCP

    void WriteHandler(boost::system::error_code error, std::size_t transferedBytes)
    {
        if (!error)
        {
            std::unique_lock<std::mutex> deleteGuard(_writeLock);

            _isWritingAsync = false;
            _writeQueue.front().ReadCompleted(transferedBytes);
            if (!_writeQueue.front().GetActiveSize())
                _writeQueue.pop();

            if (!_writeQueue.empty())
                AsyncProcessQueue(deleteGuard);
            else if (_closing)
                CloseSocket();
        }
        else
            CloseSocket();
    }

#else

    void WriteHandlerWrapper(boost::system::error_code /*error*/, std::size_t /*transferedBytes*/)
    {
        std::unique_lock<std::mutex> guard(_writeLock);
        _isWritingAsync = false;
        WriteHandler(guard);
    }

    bool WriteHandler(std::unique_lock<std::mutex>& guard)
    {
        if (!IsOpen())
            return false;

        std::size_t bytesToSend = _writeBuffer.GetActiveSize();

        if (bytesToSend == 0)
            return HandleQueue(guard);

        boost::system::error_code error;
        std::size_t bytesWritten = _socket.write_some(boost::asio::buffer(_writeBuffer.GetReadPointer(), bytesToSend), error);

        if (error)
        {
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
                return AsyncProcessQueue(guard);

            return false;
        }
        else if (bytesWritten == 0)
            return false;
        else if (bytesWritten < bytesToSend)
        {
            _writeBuffer.ReadCompleted(bytesWritten);
            _writeBuffer.Normalize();
            return AsyncProcessQueue(guard);
        }

        // now bytesWritten == bytesToSend
        _writeBuffer.Reset();

        return HandleQueue(guard);
    }

    bool HandleQueue(std::unique_lock<std::mutex>& guard)
    {
        if (_writeQueue.empty())
            return false;

        MessageBuffer& queuedMessage = _writeQueue.front();

        std::size_t bytesToSend = queuedMessage.GetActiveSize();

        boost::system::error_code error;
        std::size_t bytesSent = _socket.write_some(boost::asio::buffer(queuedMessage.GetReadPointer(), bytesToSend), error);

        if (error)
        {
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
                return AsyncProcessQueue(guard);

            _writeQueue.pop();
            return false;
        }
        else if (bytesSent == 0)
        {
            _writeQueue.pop();
            return false;
        }
        else if (bytesSent < bytesToSend) // now n > 0
        {
            queuedMessage.ReadCompleted(bytesSent);
            return AsyncProcessQueue(guard);
        }

        _writeQueue.pop();
        return !_writeQueue.empty();
    }

#endif

public:
    void SendPacket(Packet const& packet);
    void SetSession(Session* session);
private:
    void ReadHandler();
    bool ReadHeaderHandler();

    enum class ReadDataHandlerResult
    {
        Ok = 0,
        Error = 1,
        WaitingForQuery = 2
    };
    void WritePacketToBuffer(Packet const& packet, MessageBuffer& buffer);

    std::mutex _worldSessionLock;
    Session* _session;
    bool _authed;

    tcp::socket _socket;

    boost::asio::ip::address _remoteAddress;
    uint16 _remotePort;

    MessageBuffer _readBuffer;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;

    std::atomic<bool> _closed;
    std::atomic<bool> _closing;

    bool _isWritingAsync;
};

#endif // __SOCKET_H__
