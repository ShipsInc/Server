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
#include "Headers.h"

uint32 const SizeOfClientHeader[2] = { 4, 4 };
uint32 const SizeOfServerHeader = sizeof(uint16) + sizeof(uint32);

Socket::Socket(tcp::socket&& socket) : _socket(std::move(socket)), _remoteAddress(_socket.remote_endpoint().address()), _remotePort(_socket.remote_endpoint().port()), _readBuffer(), _closed(false), _closing(false), _isWritingAsync(false)
{
    _readBuffer.Resize(READ_BLOCK_SIZE);
    _headerBuffer.Resize(SizeOfClientHeader[0]);
}

Socket::~Socket()
{
    _closed = true;
    boost::system::error_code error;
    _socket.close(error);
}

void Socket::CloseSocket()
{
    if (_closed.exchange(true))
        return;

    boost::system::error_code shutdownError;
    _socket.shutdown(boost::asio::socket_base::shutdown_both, shutdownError);
    if (shutdownError)
        std::cout << "Socket::CloseSocket: " << GetRemoteIpAddress().to_string().c_str() << " errored when shutting down socket: " << shutdownError.value() << " (" << shutdownError.message().c_str() << ")";

    {
        std::lock_guard<std::mutex> sessionGuard(_worldSessionLock);
        _session = nullptr;
    }
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
    }

    _headerBuffer.Reset();
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

void Socket::SetSession(Session* session)
{
    std::lock_guard<std::mutex> sessionGuard(_worldSessionLock);
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

#ifndef TC_SOCKET_USE_IOCP
    if (_writeQueue.empty() && _writeBuffer.GetRemainingSpace() >= sizeOfHeader + packetSize)
        WritePacketToBuffer(packet, _writeBuffer);
    else
#endif
    {
        MessageBuffer buffer(sizeOfHeader + packetSize);
        WritePacketToBuffer(packet, buffer);
        QueuePacket(std::move(buffer), guard);
    }
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
