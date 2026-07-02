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

#include "eval.hpp"

// Adapted from Prolix's MIT-licensed PRF evaluator by Chris Bao.
// See LICENSES/Prolix.txt.
namespace {
constexpr int FileTable[6][8] = {
	{40, 77, 75, 79, 99, 78, 76, 41},
	{91, 65, 103, 69, 79, 92, 64, 64},
	{73, 110, 109, 127, 122, 122, 100, 76},
	{418, 437, 453, 456, 454, 444, 423, 400},
	{726, 742, 737, 741, 737, 743, 737, 743},
	{-24, -2, -1, 3, 1, 9, 6, -2},
};

constexpr int RankTable[6][8] = {
	{0, 50, 68, 64, 67, 61, 88, 0},
	{57, 0, 89, 0, 72, 0, 57, 0},
	{52, 62, 112, 124, 120, 94, 94, 71},
	{392, 406, 435, 449, 454, 470, 409, 349},
	{717, 714, 699, 717, 742, 751, 763, 778},
	{-18, -23, -4, 17, 24, 8, 5, -27},
};

constexpr int Tempo = 3;

constexpr int piece_square(PieceType piece, Square square, bool color) {
	int relative_square = color == WHITE ? square : (square ^ 56);
	return RankTable[piece][relative_square >> 3] + FileTable[piece][relative_square & 7];
}
} // namespace

Value simple_eval(Position &pos) {
	Value score = 0;
	for (int i = 0; i < 6; i++) {
		score += PieceValue[i] * arch::popcnt(pos.piece_boards[i] & pos.piece_boards[OCC(WHITE)]);
		score -= PieceValue[i] * arch::popcnt(pos.piece_boards[i] & pos.piece_boards[OCC(BLACK)]);
	}
	return score;
}

Value eval(Position &pos) {
	int score = 0;
	for (int color = WHITE; color <= BLACK; color++) {
		Bitboard pieces = pos.piece_boards[OCC(color)];
		while (pieces) {
			Square square = Square(arch::tzcnt(pieces));
			PieceType piece = PieceType(pos.mailbox[square] & 7);
			int value = piece_square(piece, square, color);
			score += color == WHITE ? value : -value;
			pieces = arch::blsr(pieces);
		}
	}
	return Value(score + (pos.side == WHITE ? Tempo : -Tempo));
}

std::array<Value, 8> debug_eval(Position &pos) {
	if (!(pos.piece_boards[KING] & pos.piece_boards[OCC(BLACK)])) {
		// If black has no king, this is mate for white
		return {VALUE_MATE, 0, 0, 0, 0, 0, 0, 0};
	}
	if (!(pos.piece_boards[KING] & pos.piece_boards[OCC(WHITE)])) {
		// Likewise, if white has no king, this is mate for black
		return {-VALUE_MATE, 0, 0, 0, 0, 0, 0, 0};
	}
	if (pos.halfmove >= 140)
		return {0, 0, 0, 0, 0, 0, 0, 0}; // Draw by 70 moves
	if (pos.two_kings())
		return {0, 0, 0, 0, 0, 0, 0, 0};
	if (pos.bare_king(!pos.side)) {
		Value result = pos.side == WHITE ? VALUE_MATE : -VALUE_MATE;
		return {result, 0, 0, 0, 0, 0, 0, 0};
	}

	Value score = eval(pos);
	return {score, score, score, score, score, score, score, score};
}
