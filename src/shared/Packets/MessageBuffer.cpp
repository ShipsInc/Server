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

#include "MessageBuffer.h"

#include <sstream>

MessageBuffer::MessageBuffer() : _wpos(0), _rpos(0), _storage()
{
    _storage.resize(4096);
}

MessageBuffer::MessageBuffer(std::size_t initialSize) : _wpos(0), _rpos(0), _storage()
{
    _storage.resize(initialSize);
}

MessageBuffer::MessageBuffer(MessageBuffer const& right) : _wpos(right._wpos), _rpos(right._rpos), _storage(right._storage)
{
}

MessageBuffer::MessageBuffer(MessageBuffer&& right) : _wpos(right._wpos), _rpos(right._rpos), _storage(right.Move()) { }
