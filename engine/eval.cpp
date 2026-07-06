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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with PZShatranjBot. If not, see <https://www.gnu.org/licenses/>.
 */

#include "eval.hpp"

#include <algorithm>
#include <cstdlib>

Network nnue_network;

__attribute__((constructor)) void init_network() {
	nnue_network.load();
}

namespace {
void refresh_accumulators(const Position &pos, NnueAccumulator &white, NnueAccumulator &black) {
	std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases), white.values);
	std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases), black.values);

	Square white_king = Square(arch::tzcnt(pos.piece_boards[KING] & pos.piece_boards[OCC(WHITE)]));
	Square black_king = Square(arch::tzcnt(pos.piece_boards[KING] & pos.piece_boards[OCC(BLACK)]));
	int white_bucket = NNUE_KING_BUCKETS[white_king];
	int black_bucket = NNUE_KING_BUCKETS[black_king ^ 56];

	for (int square = 0; square < 64; square++) {
		Piece piece = pos.mailbox[square];
		if (piece == NO_PIECE)
			continue;
		PieceType type = PieceType(piece & 7);
		bool side = piece >> 3;
		int white_index = nnue_index(Square(square), type, side, false, white_bucket);
		int black_index = nnue_index(Square(square), type, side, true, black_bucket);
		for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++) {
			white.values[i] += nnue_network.accumulator_weights[white_index][i];
			black.values[i] += nnue_network.accumulator_weights[black_index][i];
		}
	}
}

Value evaluate_bucket(Position &pos, int bucket) {
	NnueAccumulator white, black;
	refresh_accumulators(pos, white, black);
	if (pos.side == WHITE)
		return Value(nnue_eval(nnue_network, white, black, bucket));
	return Value(-nnue_eval(nnue_network, black, white, bucket));
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
	int pieces = arch::popcnt(pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)]);
	int bucket = std::clamp((pieces - 2) / 4, 0, NNUE_OUTPUT_BUCKETS - 1);
	return evaluate_bucket(pos, bucket);
}

std::array<Value, 8> debug_eval(Position &pos) {
	if (!(pos.piece_boards[KING] & pos.piece_boards[OCC(BLACK)]))
		return {VALUE_MATE, 0, 0, 0, 0, 0, 0, 0};
	if (!(pos.piece_boards[KING] & pos.piece_boards[OCC(WHITE)]))
		return {-VALUE_MATE, 0, 0, 0, 0, 0, 0, 0};
	if (pos.halfmove >= 140 || pos.two_kings())
		return {0, 0, 0, 0, 0, 0, 0, 0};
	if (pos.bare_king(!pos.side)) {
		Value result = pos.side == WHITE ? VALUE_MATE : -VALUE_MATE;
		return {result, 0, 0, 0, 0, 0, 0, 0};
	}

	std::array<Value, 8> scores{};
	for (int bucket = 0; bucket < NNUE_OUTPUT_BUCKETS; bucket++)
		scores[bucket] = evaluate_bucket(pos, bucket);
	return scores;
}
