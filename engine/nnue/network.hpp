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

#pragma once

#include "../includes.hpp"

constexpr int NNUE_INPUT_SIZE = 768;
constexpr int NNUE_INPUT_BUCKETS = 1;
constexpr int NNUE_ACCUMULATOR_SIZE = 32;
constexpr int NNUE_L2_SIZE = 16;
constexpr int NNUE_L3_SIZE = 32;
constexpr int NNUE_OUTPUT_BUCKETS = 8;
constexpr int NNUE_QA = 255;
constexpr int NNUE_QB = 64;
constexpr int NNUE_SCALE = 400;
constexpr int NNUE_KING_BUCKETS[64] = {
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
	0, 0, 0, 0, 1, 1, 1, 1,
};

constexpr bool valid_nnue_king_buckets() {
	for (int bucket : NNUE_KING_BUCKETS)
		if (bucket < 0 || bucket >= 2 * NNUE_INPUT_BUCKETS)
			return false;
	return true;
}
static_assert(valid_nnue_king_buckets(), "NNUE king layout references an unavailable input bucket");

struct NnueAccumulator {
	int16_t values[NNUE_ACCUMULATOR_SIZE]{};
};

struct Network {
	int16_t accumulator_weights[NNUE_INPUT_SIZE * NNUE_INPUT_BUCKETS][NNUE_ACCUMULATOR_SIZE];
	int16_t accumulator_biases[NNUE_ACCUMULATOR_SIZE];

	int8_t l1_weights[NNUE_OUTPUT_BUCKETS][NNUE_L2_SIZE][NNUE_ACCUMULATOR_SIZE];
	float l1_biases[NNUE_OUTPUT_BUCKETS][NNUE_L2_SIZE];

	float l2_weights[NNUE_OUTPUT_BUCKETS][NNUE_L2_SIZE][NNUE_L3_SIZE];
	float l2_biases[NNUE_OUTPUT_BUCKETS][NNUE_L3_SIZE];

	float output_weights[NNUE_OUTPUT_BUCKETS][NNUE_L3_SIZE];
	float output_biases[NNUE_OUTPUT_BUCKETS];

	void load();
};

int nnue_index(Square square, PieceType piece, bool side, bool perspective, int king_bucket);
int32_t nnue_eval(const Network &network, const NnueAccumulator &stm, const NnueAccumulator &ntm, int output_bucket);

extern Network nnue_network;
