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

#include "network.hpp"
#include "incbin.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" {
	INCBIN(network_weights, NNUE_PATH);
}

namespace {
constexpr size_t NETWORK_FILE_SIZE =
	NNUE_INPUT_BUCKETS * NNUE_INPUT_SIZE * NNUE_ACCUMULATOR_SIZE * sizeof(int16_t)
	+ NNUE_ACCUMULATOR_SIZE * sizeof(int16_t)
	+ NNUE_OUTPUT_BUCKETS * NNUE_L2_SIZE * NNUE_ACCUMULATOR_SIZE * sizeof(int8_t)
	+ NNUE_OUTPUT_BUCKETS * NNUE_L2_SIZE * sizeof(float)
	+ NNUE_OUTPUT_BUCKETS * NNUE_L2_SIZE * NNUE_L3_SIZE * sizeof(float)
	+ NNUE_OUTPUT_BUCKETS * NNUE_L3_SIZE * sizeof(float)
	+ NNUE_OUTPUT_BUCKETS * NNUE_L3_SIZE * sizeof(float)
	+ NNUE_OUTPUT_BUCKETS * sizeof(float);

// Bullet writes piece planes in chess order (P, N, B, R, Q, K), while the
// engine indexes them in shatranj order (P, A, F, N, R, K). This is the same
// first-layer conversion used by Prolix, repeated for the black planes.
constexpr int NET_TO_SHATRANJ_PIECE[12] = {0, 3, 1, 4, 2, 5, 6, 9, 7, 10, 8, 11};
} // namespace

void Network::load() {
	if (gnetwork_weightsSize != NETWORK_FILE_SIZE) {
		std::cerr << "Invalid embedded NNUE size for " << NNUE_PATH << ": expected "
		          << NETWORK_FILE_SIZE << " bytes, got " << gnetwork_weightsSize << std::endl;
		std::abort();
	}

	const unsigned char *ptr = gnetwork_weightsData;
	auto read = [&ptr](auto &value) {
		std::memcpy(&value, ptr, sizeof(value));
		ptr += sizeof(value);
	};

	for (int input_bucket = 0; input_bucket < NNUE_INPUT_BUCKETS; input_bucket++)
		for (int feature = 0; feature < NNUE_INPUT_SIZE; feature++) {
			int net_piece = feature / 64;
			int square = feature % 64;
			int engine_feature = input_bucket * NNUE_INPUT_SIZE
				+ NET_TO_SHATRANJ_PIECE[net_piece] * 64 + square;
			std::memcpy(accumulator_weights[engine_feature], ptr, sizeof(accumulator_weights[engine_feature]));
			ptr += sizeof(accumulator_weights[engine_feature]);
		}
	read(accumulator_biases);
	read(l1_weights);
	read(l1_biases);

	// The trainer serializes this matrix output-major.
	for (int bucket = 0; bucket < NNUE_OUTPUT_BUCKETS; bucket++)
		for (int output = 0; output < NNUE_L3_SIZE; output++)
			for (int input = 0; input < NNUE_L2_SIZE; input++)
				read(l2_weights[bucket][input][output]);

	read(l2_biases);
	read(output_weights);
	read(output_biases);
}

int nnue_index(Square square, PieceType piece, bool side, bool perspective, int king_bucket) {
	if (king_bucket & 1)
		square = Square(square ^ 7);
	int input_bucket = king_bucket / 2;
	if (perspective) {
		side = !side;
		square = Square(square ^ 56);
	}
	return input_bucket * NNUE_INPUT_SIZE + side * 64 * 6 + piece * 64 + square;
}

int32_t nnue_eval(const Network &network, const NnueAccumulator &stm, const NnueAccumulator &ntm, int output_bucket) {
	uint8_t l1[NNUE_ACCUMULATOR_SIZE];
	for (int i = 0; i < NNUE_ACCUMULATOR_SIZE / 2; i++) {
		int stm_a = std::clamp<int>(stm.values[i], 0, NNUE_QA);
		int stm_b = std::clamp<int>(stm.values[i + NNUE_ACCUMULATOR_SIZE / 2], 0, NNUE_QA);
		int ntm_a = std::clamp<int>(ntm.values[i], 0, NNUE_QA);
		int ntm_b = std::clamp<int>(ntm.values[i + NNUE_ACCUMULATOR_SIZE / 2], 0, NNUE_QA);
		// Match the legacy rounded high multiply used by the SIMD inference.
		l1[i] = uint8_t((stm_a * stm_b + 128) >> 8);
		l1[i + NNUE_ACCUMULATOR_SIZE / 2] = uint8_t((ntm_a * ntm_b + 128) >> 8);
	}

	float l2[NNUE_L2_SIZE];
	constexpr float L1_SCALE = 256.0f / (NNUE_QA * NNUE_QA * NNUE_QB);
	for (int i = 0; i < NNUE_L2_SIZE; i++) {
		int sum = 0;
		for (int j = 0; j < NNUE_ACCUMULATOR_SIZE; j++)
			sum += int(l1[j]) * int(network.l1_weights[output_bucket][i][j]);
		float value = std::fma(float(sum), L1_SCALE, network.l1_biases[output_bucket][i]);
		value = std::clamp(value, 0.0f, 1.0f);
		l2[i] = value * value;
	}

	float l3[NNUE_L3_SIZE];
	for (int i = 0; i < NNUE_L3_SIZE; i++) {
		float value = network.l2_biases[output_bucket][i];
		for (int j = 0; j < NNUE_L2_SIZE; j++)
			value = std::fma(l2[j], network.l2_weights[output_bucket][j][i], value);
		l3[i] = value;
	}

	float score = network.output_biases[output_bucket];
	for (int i = 0; i < NNUE_L3_SIZE; i++) {
		float value = std::clamp(l3[i], 0.0f, 1.0f);
		score = std::fma(value * value, network.output_weights[output_bucket][i], score);
	}
	return int32_t(score * NNUE_SCALE);
}
