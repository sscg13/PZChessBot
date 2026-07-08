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
Value evaluate_bucket(Position &pos, AccumulatorManager &accumulators, int bucket) {
	accumulators.apply_lazy(pos);
	const auto &pair = accumulators.current();
#ifdef NNUE_ACCUMULATOR_CHECK
	AccumulatorManager reference(pos);
	const auto &expected = reference.current();
	if (!std::equal(std::begin(pair.white.values), std::end(pair.white.values), std::begin(expected.white.values)) ||
	    !std::equal(std::begin(pair.black.values), std::end(pair.black.values), std::begin(expected.black.values)))
		std::abort();
#endif
	if (pos.side == WHITE)
		return Value(nnue_eval(nnue_network, pair.white, pair.black, bucket));
	return Value(-nnue_eval(nnue_network, pair.black, pair.white, bucket));
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
	AccumulatorManager accumulators(pos);
	return eval(pos, accumulators);
}

Value eval(Position &pos, AccumulatorManager &accumulators) {
	int pieces = arch::popcnt(pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)]);
	int bucket = std::clamp((pieces - 2) / 4, 0, NNUE_OUTPUT_BUCKETS - 1);
	return evaluate_bucket(pos, accumulators, bucket);
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
	AccumulatorManager accumulators(pos);
	for (int bucket = 0; bucket < NNUE_OUTPUT_BUCKETS; bucket++)
		scores[bucket] = evaluate_bucket(pos, accumulators, bucket);
	return scores;
}
