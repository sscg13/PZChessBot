/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * PZShatranjBot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with PZShatranjBot. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "includes.hpp"

// A move needs 16 bits to be stored
// bits 0-5: Destination square (from 0 to 63)
// bits 6-11: Origin (from 0 to 63)
// bits 12-13: unused
// bit 14: promotion flag (shatranj pawns always promote to ferz)
struct Move {
	uint16_t data;
	constexpr Move() : data(0) {}
	constexpr explicit Move(uint16_t d) : data(d) {}
	constexpr Move(int from, int to) : data((from << 6) | to) {}
	template <MoveType T> static constexpr Move make(int from, int to) {
		return Move(T | (from << 6) | to);
	}
	constexpr Square src() const {
		return (Square)(data >> 6 & 0x3f);
	};
	constexpr Square dst() const {
		return (Square)(data & 0x3f);
	};
	constexpr MoveType type() const {
		return (MoveType)(data & 0xc000);
	};
	constexpr bool operator==(const Move &m) const {
		return data == m.data;
	};
	constexpr bool operator!=(const Move &m) const {
		return data != m.data;
	};
	std::string to_string() const;
	static Move from_string(const std::string &, const void *);
};

static constexpr Move NullMove = Move(0);
