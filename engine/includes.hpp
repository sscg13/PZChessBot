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

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stack>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "arch/arch.hpp"
#include "debug.hpp"
#include "pzstl/vector.hpp"

#ifndef VERSION
#define VERSION "v7.1"
#endif

typedef uint64_t Bitboard;

constexpr bool WHITE = false;
constexpr bool BLACK = true;

constexpr int MAX_PLY = 200;
constexpr int MAX_THREADS = 512;

typedef int16_t Value;
constexpr Value VALUE_ZERO = 0;
constexpr Value VALUE_INFINITE = 32000;
constexpr Value VALUE_MATE = 30002; // Add 2 as a consequence of our evaluation function only returning MATE when king is taken
constexpr Value VALUE_MATE_MAX_PLY = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_NONE = -31000;

constexpr Value VALUE_TB_WIN = 28000;
constexpr Value VALUE_TB_WIN_MAX_PLY = VALUE_TB_WIN - MAX_PLY;
constexpr Value VALUE_WIN = VALUE_TB_WIN_MAX_PLY; // They're the same thing

constexpr Value PawnValue = 100;
constexpr Value AlfilValue = 150;
constexpr Value FerzValue = 200;
constexpr Value KnightValue = 300;
constexpr Value RookValue = 525;
constexpr Value VALUE_MAX = FerzValue * 9 + (KnightValue + AlfilValue + RookValue) * 2;

constexpr Value MAX_HISTORY = 16384;
constexpr Value MAX_CORRHIST = 1024;

#define CLOCKS_PER_MS (CLOCKS_PER_SEC / 1000)

// clang-format off

// Ordered by approximate shatranj material value. FEN keeps the orthodox
// letters: b is an alfil and q is a ferz.
enum PieceType : uint8_t { PAWN, ALFIL, FERZ, KNIGHT, ROOK, KING, NO_PIECETYPE };

enum Piece : uint8_t {
	WHITE_PAWN,
	WHITE_ALFIL,
	WHITE_FERZ,
	WHITE_KNIGHT,
	WHITE_ROOK,
	WHITE_KING,
	BLACK_PAWN = 8,
	BLACK_ALFIL,
	BLACK_FERZ,
	BLACK_KNIGHT,
	BLACK_ROOK,
	BLACK_KING,
	NO_PIECE
};

constexpr Value PieceValue[] = {PawnValue, AlfilValue, FerzValue, KnightValue, RookValue, VALUE_INFINITE - VALUE_MAX, 0};
constexpr Value MVV[] = { 800, 1200, 1600, 2400, 4800, 16000 };

constexpr PieceType letter_piece[] = {ALFIL, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, NO_PIECETYPE, KING, NO_PIECETYPE, NO_PIECETYPE, KNIGHT, NO_PIECETYPE, PAWN, FERZ, ROOK};
constexpr char piecetype_letter[] = {'p', 'b', 'q', 'n', 'r', 'k', '?'};
constexpr char piece_letter[] = {'P','B','Q','N','R','K','?','?','p','b','q','n','r','k','?'};

enum File : uint16_t {
	FILE_A,
	FILE_B,
	FILE_C,
	FILE_D,
	FILE_E,
	FILE_F,
	FILE_G,
	FILE_H,
};

inline constexpr File operator++(File &file, int) {
	return file = File(file + 1);
}

enum Rank : uint16_t {
	RANK_1,
	RANK_2,
	RANK_3,
	RANK_4,
	RANK_5,
	RANK_6,
	RANK_7,
	RANK_8,
};

inline constexpr Rank operator++(Rank &rank, int) {
	return rank = Rank(rank + 1);
}

enum Square : uint8_t {
	SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
	SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
	SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
	SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
	SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
	SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
	SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
	SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
	SQ_NONE
};

inline constexpr Square operator++(Square &square, int) {
	return square = Square(square + 1);
}

inline std::string to_string(Square square) {
	if (square >= SQ_NONE) return "None";
	return std::string(1, 'a' + (square % 8)) + std::to_string(1 + (square / 8));
}

enum MoveType {
	NORMAL,
	PROMOTION = 1 << 14,
};

#define RESET "\033[0m"
#define CYAN "\033[36m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define WHITE_PIECE "\033[97m"
#define BLACK_PIECE "\033[30m"
#define LIGHT_SQUARE "\033[48;5;94m"
#define DARK_SQUARE "\033[48;5;223m"
#define BORDER "\033[36m"
#define COORDS "\033[33m"
#define CLEAR_LINE "\r\033[K"
#define CURSOR_UP "\033[A"
