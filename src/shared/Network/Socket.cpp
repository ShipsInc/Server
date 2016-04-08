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

#include "Socket.h"
#include "Packet.h"
#include "Headers.h"
#include "Session.h"

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "MessageBuffer.h"
#include "Packet.h"

uint32 const SizeOfClientHeader[2] = { 4, 4 };
uint32 const SizeOfServerHeader = sizeof(uint16) + sizeof(uint32);

Socket::Socket(boost::asio::ip::tcp::socket&& socket) : _session(nullptr), _authed(false), _socket(std::move(socket)), _readBuffer(), _headerBuffer(), _closed(false), _closing(false), _isWritingAsync(false)
{
    _readBuffer.Resize(READ_BLOCK_SIZE);
    _headerBuffer.Resize(SizeOfClientHeader[0]);

    _socket.remote_endpoint().address(_remoteAddress);
    _socket.remote_endpoint().port(_remotePort);
}

Socket::~Socket()
{
    _closed = true;
    boost::system::error_code error;
    _socket.close(error);
}

bool Socket::Update()
{
    if (!IsOpen())
        return false;

    return true;
}

boost::asio::ip::address Socket::GetRemoteIpAddress() const
{
    return _remoteAddress;
}

uint16 Socket::GetRemotePort() const
{
    return _remotePort;
}

void Socket::SetKeepAlive(bool set)
{
    boost::system::error_code error;
    boost::asio::socket_base::keep_alive option(set);
    _socket.set_option(option, error);
}

void Socket::AsyncRead()
{
    if (!IsOpen())
        return;

    _readBuffer.Normalize();
    _readBuffer.EnsureFreeSpace();
    _socket.async_read_some(boost::asio::buffer(_readBuffer.GetWritePointer(), _readBuffer.GetRemainingSpace()), std::bind(&Socket::ReadHandlerInternal, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void Socket::Start()
{
    AsyncRead();
}

void Socket::CloseSocket()
{
    if (_closed.exchange(true))
        return;

    boost::system::error_code shutdownError;
    _socket.shutdown(boost::asio::socket_base::shutdown_send, shutdownError);
    if (shutdownError)
        std::cout << "Socket::CloseSocket: " << GetRemoteIpAddress().to_string().c_str() << " errored when shutting down socket: " << shutdownError.value() << " (" << shutdownError.message().c_str() << ")";

    {
        std::lock_guard<std::mutex> sessionGuard(_sessionLock);
        _session = nullptr;
    }
}

void Socket::ReadHandlerInternal(boost::system::error_code error, size_t transferredBytes)
{
    if (error)
    {
        CloseSocket();
        return;
    }

    _readBuffer.WriteCompleted(transferredBytes);
    ReadHandler();
}

void Socket::ReadHandler()
{
    if (!IsOpen())
        return;

    MessageBuffer& packet = GetReadBuffer();
    while (packet.GetActiveSize() > 0)
    {
        if (_headerBuffer.GetRemainingSpace() > 0)
        {
            // need to receive the header
            std::size_t readHeaderSize = std::min(packet.GetActiveSize(), _headerBuffer.GetRemainingSpace());
            _headerBuffer.Write(packet.GetReadPointer(), readHeaderSize);
            packet.ReadCompleted(readHeaderSize);

            if (_headerBuffer.GetRemainingSpace() > 0)
                break;

            // We just received nice new header
            if (!ReadHeaderHandler())
            {
                CloseSocket();
                return;
            }
        }

        // We have full read header, now check the data payload
        if (_packetBuffer.GetRemainingSpace() > 0)
        {
            // need more data in the payload
            std::size_t readDataSize = std::min(packet.GetActiveSize(), _packetBuffer.GetRemainingSpace());
            _packetBuffer.Write(packet.GetReadPointer(), readDataSize);
            packet.ReadCompleted(readDataSize);

            if (_packetBuffer.GetRemainingSpace() > 0)
            {
                // Couldn't receive the whole data this time.
                break;
            }
        }
        // just received fresh new payload
        if (!ReadDataHandler())
        {
            CloseSocket();
            _headerBuffer.Reset();
            return;
        }

        _headerBuffer.Reset();
    }

    AsyncRead();
}

bool Socket::ReadHeaderHandler()
{
    ClientHeader* header = reinterpret_cast<ClientHeader*>(_headerBuffer.GetReadPointer());
    uint32 opcode = header->Command;
    uint32 size = header->Size;

    if (!ClientHeader::IsValidSize(size) || !ClientHeader::IsValidOpcode(opcode))
    {
        std::cout << "Socket::ReadHeaderHandler(): client " << GetRemoteIpAddress().to_string().c_str() << " sent malformed packet (size: " << size << ", cmd: " << opcode << ")" << std::endl;
        return false;
    }

    _packetBuffer.Resize(size);
    return true;
}

void Socket::WriteHandlerWrapper(boost::system::error_code /*error*/, std::size_t /*transferedBytes*/)
{
    std::unique_lock<std::mutex> guard(_writeLock);
    _isWritingAsync = false;
    WriteHandler(guard);
}

bool Socket::WriteHandler(std::unique_lock<std::mutex>& guard)
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

bool Socket::HandleQueue(std::unique_lock<std::mutex>& guard)
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

void Socket::SetSession(Session* session)
{
    std::lock_guard<std::mutex> sessionGuard(_sessionLock);
    _session = session;
    _authed = true;
}

void Socket::SendPacket(Packet const& packet)
{
    if (!IsOpen())
        return;

    uint32 packetSize = packet.size();
    uint32 sizeOfHeader = SizeOfServerHeader;
    std::unique_lock<std::mutex> guard(_writeLock);

    MessageBuffer buffer(sizeOfHeader + packetSize);
    WritePacketToBuffer(packet, buffer);
    QueuePacket(std::move(buffer), guard);
}

void Socket::WritePacketToBuffer(Packet const& packet, MessageBuffer& buffer)
{
    ServerHeader header;
    uint32 sizeOfHeader = SizeOfServerHeader;
    uint32 opcode = packet.GetOpcode();
    uint32 packetSize = packet.size();

    // Reserve space for buffer
    uint8* headerPos = buffer.GetWritePointer();
    buffer.WriteCompleted(sizeOfHeader);

    if (!packet.empty())
        buffer.Write(packet.contents(), packet.size());

    header.Size = packetSize;
    header.Command = opcode;
    memcpy(headerPos, &header, sizeOfHeader);
}

void Socket::QueuePacket(MessageBuffer&& buffer, std::unique_lock<std::mutex>&)
{
    _writeQueue.push(std::move(buffer));
}

bool Socket::AsyncProcessQueue(std::unique_lock<std::mutex>&)
{
    if (_isWritingAsync)
        return false;

    _isWritingAsync = true;

    _socket.async_write_some(boost::asio::null_buffers(), std::bind(&Socket::WriteHandlerWrapper, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    return false;
}

bool Socket::ReadDataHandler()
{
    ClientHeader* header = reinterpret_cast<ClientHeader*>(_headerBuffer.GetReadPointer());

    Packet packet(header->Command, std::move(_packetBuffer));

    if (packet.GetOpcode() >= NUM_MSG_TYPES)
        return true;

    switch (header->Command)
    {
        case CMSG_AUTH:
        {
            HandleAuth(packet);
            break;
        }
        default:
        {
            std::lock_guard<std::mutex> guard(_sessionLock);

            if (!_session)
                break;

            _session->QueuePacket(new Packet(std::move(packet)));
            break;
        }
    }

    return true;
}

void Socket::HandleAuth(Packet& packet)
{
    std::string s1, s2;
    packet >> s1;
    packet >> s1;

    std::cout << s1 << " : " << s2 << std::endl;
    CloseSocket();
}
