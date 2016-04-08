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

#ifndef SERVER_PACKET_H
#define SERVER_PACKET_H

#include "ByteBuffer.h"

class Packet : public ByteBuffer
{
    public:
                                                            // just container for later use
        Packet() : ByteBuffer(0), m_opcode(0)
        {
        }

        Packet(uint32 opcode, size_t res = 200) : ByteBuffer(res), m_opcode(opcode) { }

        Packet(Packet&& packet) : ByteBuffer(std::move(packet)), m_opcode(packet.m_opcode)
        {
        }

        Packet(Packet const& right) : ByteBuffer(right), m_opcode(right.m_opcode)
        {
        }

        Packet& operator=(Packet const& right)
        {
            if (this != &right)
            {
                m_opcode = right.m_opcode;
                ByteBuffer::operator =(right);
            }

            return *this;
        }

        Packet& operator=(Packet&& right)
        {
            if (this != &right)
            {
                m_opcode = right.m_opcode;
                ByteBuffer::operator=(std::move(right));
            }

            return *this;
        }

        Packet(uint32 opcode, MessageBuffer&& buffer) : ByteBuffer(std::move(buffer)), m_opcode(opcode) { }

        void Initialize(uint32 opcode, size_t newres = 200)
        {
            clear();
            _storage.reserve(newres);
            m_opcode = opcode;
        }

        uint32 GetOpcode() const { return m_opcode; }
        void SetOpcode(uint32 opcode) { m_opcode = opcode; }
    protected:
        uint32 m_opcode;
};

#endif
