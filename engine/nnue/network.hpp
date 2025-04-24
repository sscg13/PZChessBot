#pragma once

#include "../includes.hpp"
#include "threats.hpp"

constexpr int HL_SIZE = 256;
constexpr int NBUCKETS = 8;
constexpr int SCALE = 340;
constexpr int QA = 255;
constexpr int QB = 64;

struct Accumulator {
    int16_t accumulation[2][HL_SIZE];
    Bitboard piece_bbs[8];
    Piece mailbox[64];
    IndexList<96> threats[2];

    template<bool Perspective> void add_weights(const int16_t* weights);
    template<bool Perspective> void sub_weights(const int16_t* weights);
};

struct Network {
	int16_t accumulator_weights[Full_Threats::InputSize][HL_SIZE];
	int16_t accumulator_biases[HL_SIZE];
	int16_t output_weights[NBUCKETS][2 * HL_SIZE];
	int16_t output_bias[NBUCKETS];

	void load();
};


struct AccumulatorStack {
    Accumulator accumulators[MAX_PLY];
    Network& network;
    Full_Threats threat_indexer;
    int ply;

    template<bool Perspective> void update_scratch(const Board& board);
    template<bool Perspective> void update_forwards(const Board& board);
    int evaluate(int16_t* weights, bool color);
}