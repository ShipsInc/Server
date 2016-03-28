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

#ifndef _OPCODES_H
#define _OPCODES_H

enum OpcodeMisc : uint32
{
    MAX_OPCODE                                                     = 0x1FFF,
    NUM_OPCODE_HANDLERS                                            = (MAX_OPCODE + 1),
    UNKNOWN_OPCODE                                                 = (0xFFFF + 1),
    NULL_OPCODE                                                    = 0xBADD
};

#endif