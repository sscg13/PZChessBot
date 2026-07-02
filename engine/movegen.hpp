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

#include "bitboard.hpp"
#include "includes.hpp"

void white_pawn_moves(const Position &pos, pzstd::vector<Move> &moves);
void black_pawn_moves(const Position &pos, pzstd::vector<Move> &moves);
void pawn_moves(const Position &pos, pzstd::vector<Move> &moves);
void knight_moves(const Position &pos, pzstd::vector<Move> &moves);
void alfil_moves(const Position &pos, pzstd::vector<Move> &moves);
void ferz_moves(const Position &pos, pzstd::vector<Move> &moves);
void rook_moves(const Position &pos, pzstd::vector<Move> &moves);
void king_moves(const Position &pos, pzstd::vector<Move> &moves);

Bitboard rook_attacks(Square sq, Bitboard occ);
Bitboard alfil_attacks(Square sq);
Bitboard ferz_attacks(Square sq);
Bitboard knight_attacks(Square sq);
Bitboard king_attacks(Square sq);
Bitboard pawn_attacks(Square sq, bool color);
