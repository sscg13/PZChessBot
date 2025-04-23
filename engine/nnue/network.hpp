#pragma once

#include "../includes.hpp"
#include "threats.hpp"

#define HL_SIZE 256
#define NBUCKETS 8
#define SCALE 400
#define QA 255
#define QB 64

struct Accumulator {
	int16_t val[HL_SIZE] = {};
};

struct Network {
	int16_t accumulator_weights[Full_Threats::InputSize][HL_SIZE];
	int16_t accumulator_biases[HL_SIZE];
	int16_t output_weights[NBUCKETS][2 * HL_SIZE];
	int16_t output_bias[NBUCKETS];

	void load();
};

int calculate_index(Square sq, PieceType pt, bool side, bool perspective);

void accumulator_add(const Network &net, Accumulator &acc, uint16_t index);

void accumulator_sub(const Network &net, Accumulator &acc, uint16_t index);

int32_t nnue_eval(const Network &net, const Accumulator &stm, const Accumulator &ntm, uint8_t nbucket);