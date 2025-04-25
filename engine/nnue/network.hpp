#pragma once

#include "../includes.hpp"
#include "threats.hpp"

constexpr int HL_SIZE = 256;
constexpr int NBUCKETS = 8;
constexpr int SCALE = 340;
constexpr int QA = 255;
constexpr int QB = 64;

struct PieceChange {
    int piece_num;
    Piece piece[3];
    Square from[3];
    Square to[3];
};

PieceChange get_piece_change(const Board& board, const Move& move);

struct Accumulator {
    int16_t accumulation[2][HL_SIZE];
    IndexList<96> threats[2];
	Square ksq[2];

	template<bool Perspective> void reset(const int16_t* biases);
    template<bool Perspective> void add_weights(const int16_t* weights);
    template<bool Perspective> void sub_weights(const int16_t* weights);
};

struct Network {
	int16_t accumulator_weights[Full_Threats::InputSize][HL_SIZE];
	int16_t accumulator_biases[HL_SIZE];
	int16_t output_weights[NBUCKETS][2 * HL_SIZE];
	int16_t output_bias[NBUCKETS];
    Accumulator accumulators[MAX_PLY];
    Full_Threats threat_indexer;
    int ply;

	void load();
	void initialize(const Board& board);
    template<bool Perspective> void update_perspective_scratch(const Board& board);
    template<bool Perspective> void update_perspective_forward(const Board& board, const PieceChange& diff);
	void update_forward(const Board& board, const PieceChange& diff) { ply++; update_perspective_forward<WHITE>(board, diff); update_perspective_forward<BLACK>(board, diff); }
	void update_backward() { ply--; }
    int32_t evaluate(bool color, int bucket);
};