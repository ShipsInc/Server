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

/// \addtogroup u2w
/// @{
/// \file

#ifndef _OPCODES_H
#define _OPCODES_H

#include "Define.h"

enum Opcodes : uint32
{
    CMSG_AUTH                                                      = 0x001,
    CMSG_REGISTRATION                                              = 0x002,
    SMSG_AUTH_RESPONSE                                             = 0x003,
    CMSG_REGISTRATION_RESPONSE                                     = 0x004,
    NUM_MSG_TYPES                                                  = 0x005
};

enum OpcodeMisc : uint32
{
    NUM_OPCODE_HANDLERS                                            = (NUM_MSG_TYPES + 1),
    NULL_OPCODE                                                    = 0xBADD
};

class Packet;
class Session;

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct OpcodeHandler
{
    char const* name;
    void (Session::*handler)(Packet& recvPacket);
};

extern OpcodeHandler opcodeTable[NUM_MSG_TYPES];

/// Lookup opcode name for human understandable logging
inline const char* LookupOpcodeName(uint16 id)
{
    if (id > NUM_OPCODE_HANDLERS)
        return "Received unknown opcode, it's more than max!";
    return opcodeTable[id].name;
}

#endif
/// @}
