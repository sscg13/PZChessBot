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
#include "move.hpp"
#include "ttable.hpp"

// Selects the occupancy array by xoring 6 with side (white: false = 0 ^ 6 = 6, black: true = 1 ^ 6 = 7)
#define OCC(side) (6 ^ (side))
#define OPPOCC(side) (7 ^ (side))

constexpr Bitboard FileABits = 0x0101010101010101ULL;
constexpr Bitboard FileBBits = FileABits << 1;
constexpr Bitboard FileGBits = FileABits << 6;
constexpr Bitboard FileHBits = FileABits << 7;

constexpr Bitboard Rank1Bits = 0xff;
constexpr Bitboard Rank2Bits = Rank1Bits << (8 * 1);
constexpr Bitboard Rank7Bits = Rank1Bits << (8 * 6);
constexpr Bitboard Rank8Bits = Rank1Bits << (8 * 7);

// clang-format off
constexpr Bitboard square_bits(Square x) { return 1ULL << x; }
constexpr Bitboard square_bits(Rank r, File f) { return 1ULL << (r * 8 + f); }
// clang-format on

void print_bitboard(Bitboard);

struct Position {
	Bitboard piece_boards[8] = {};
	Bitboard side_control[2] = {};
	Bitboard pinned[2];
	Bitboard pinners[2];
	Bitboard checkers[2];
	bool side = WHITE;
	uint8_t halfmove = 0;
	uint64_t zobrist = 0;
	uint64_t piece_hashes[15];

	// Mailbox representation of the board for faster queries of certain data
	Piece mailbox[64];

	int fullmove = 0;

	Position() {
		reset_pos();
	}

	Position(std::string fen) {
		load_fen(fen);
	}

	void load_fen(std::string);
	std::string get_fen() const;
	void print_board() const;
	bool sanity_check(char *);

	void reset_pos();
	void reset_startpos() { reset_pos(); }
	void reset(std::string fen) { reset_pos(); load_fen(fen); }

	void make_move(Move);
	void update_control();

	void legal_moves(pzstd::vector<Move> &) const;
	void captures(pzstd::vector<Move> &) const;
	bool control(int, bool) const;
	bool see(Move, int);
	Bitboard lva_(Square, int side, PieceType &p, Bitboard occ) const;
	bool is_pseudolegal(Move) const;
	bool is_legal(Move) const;
	constexpr bool is_capture(Move m) const {
		return m != NullMove && (piece_boards[OPPOCC(side)] & square_bits(m.dst()));
	}

	void recompute_hash();

	bool bare_king(bool color) const;
	bool two_kings() const;
	bool has_legal_move() const;

	uint64_t pawn_hash() const;
	uint64_t nonpawn_hash(bool color) const;
	uint64_t major_hash() const;
	uint64_t minor_hash() const;
};

struct RepetitionHandler {
	pzstd::vector<uint64_t, 1024> hash_hist;

	RepetitionHandler() {
		hash_hist.clear();
	}

	void clear() {
		hash_hist.clear();
	}

	bool threefold(int ply, uint64_t hash);

	void push_hash(uint64_t hash) { hash_hist.push_back(hash); }
	void pop_hash() { hash_hist.pop_back(); }
};
