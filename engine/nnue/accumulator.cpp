/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include "accumulator.hpp"

#include <algorithm>

namespace {
int white_king_bucket(const Position &pos) {
	Square king = Square(arch::tzcnt(pos.piece_boards[KING] & pos.piece_boards[OCC(WHITE)]));
	return NNUE_KING_BUCKETS[king];
}

int black_king_bucket(const Position &pos) {
	Square king = Square(arch::tzcnt(pos.piece_boards[KING] & pos.piece_boards[OCC(BLACK)]));
	return NNUE_KING_BUCKETS[king ^ 56];
}

void add_feature(NnueAccumulator &accumulator, int feature) {
	for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
		accumulator.values[i] += nnue_network.accumulator_weights[feature][i];
}

void remove_feature(NnueAccumulator &accumulator, int feature) {
	for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
		accumulator.values[i] -= nnue_network.accumulator_weights[feature][i];
}
} // namespace

AccumulatorManager::Cache::Cache() {
	std::fill(&mailboxes[0][0][0], &mailboxes[0][0][0] + KingBucketCount * 2 * 64, NO_PIECE);
	for (int bucket = 0; bucket < KingBucketCount; bucket++) {
		std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases),
		          accumulators[bucket].white.values);
		std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases),
		          accumulators[bucket].black.values);
	}
}

void AccumulatorManager::reset(Position &pos) {
	index = 0;
	full_refresh(pos, 0);
}

void AccumulatorManager::full_refresh(Position &pos, int destination) {
	AccumulatorPair &pair = accumulators[destination];
	std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases), pair.white.values);
	std::copy(std::begin(nnue_network.accumulator_biases), std::end(nnue_network.accumulator_biases), pair.black.values);

	pair.white_bucket = white_king_bucket(pos);
	pair.black_bucket = black_king_bucket(pos);

	for (int square = 0; square < 64; square++) {
		Piece piece = pos.mailbox[square];
		if (piece == NO_PIECE)
			continue;

		PieceType type = PieceType(piece & 7);
		bool side = piece >> 3;
		add_feature(pair.white, nnue_index(Square(square), type, side, false, pair.white_bucket));
		add_feature(pair.black, nnue_index(Square(square), type, side, true, pair.black_bucket));
	}

	pair.correct = true;
}

void AccumulatorManager::refresh_from_cache(Position &pos, int destination) {
	AccumulatorPair &pair = accumulators[destination];
	pair.white_bucket = white_king_bucket(pos);
	pair.black_bucket = black_king_bucket(pos);

	NnueAccumulator &cached_white = cache.accumulators[pair.white_bucket].white;
	NnueAccumulator &cached_black = cache.accumulators[pair.black_bucket].black;
	std::copy(std::begin(cached_white.values), std::end(cached_white.values), pair.white.values);
	std::copy(std::begin(cached_black.values), std::end(cached_black.values), pair.black.values);

	for (int square = 0; square < 64; square++) {
		Piece piece = pos.mailbox[square];
		Piece previous_white = cache.mailboxes[pair.white_bucket][WHITE][square];
		Piece previous_black = cache.mailboxes[pair.black_bucket][BLACK][square];

		if (piece != previous_white) {
			if (piece != NO_PIECE)
				add_feature(pair.white, nnue_index(Square(square), PieceType(piece & 7), piece >> 3, false, pair.white_bucket));
			if (previous_white != NO_PIECE)
				remove_feature(pair.white, nnue_index(Square(square), PieceType(previous_white & 7), previous_white >> 3,
				                                      false, pair.white_bucket));
		}

		if (piece != previous_black) {
			if (piece != NO_PIECE)
				add_feature(pair.black, nnue_index(Square(square), PieceType(piece & 7), piece >> 3, true, pair.black_bucket));
			if (previous_black != NO_PIECE)
				remove_feature(pair.black, nnue_index(Square(square), PieceType(previous_black & 7), previous_black >> 3,
				                                      true, pair.black_bucket));
		}
	}

	cache.accumulators[pair.white_bucket].white = pair.white;
	cache.accumulators[pair.black_bucket].black = pair.black;
	for (int square = 0; square < 64; square++) {
		cache.mailboxes[pair.white_bucket][WHITE][square] = pos.mailbox[square];
		cache.mailboxes[pair.black_bucket][BLACK][square] = pos.mailbox[square];
	}
	pair.correct = true;
}

void AccumulatorManager::apply_lazy(Position &pos) {
	if (current().correct)
		return;

	int base = index;
	while (base > 0) {
		base--;
		if (accumulators[base].white_bucket != current().white_bucket ||
		    accumulators[base].black_bucket != current().black_bucket) {
			refresh_from_cache(pos, index);
			return;
		}
		if (accumulators[base].correct)
			break;
	}

	if (!accumulators[base].correct) {
		refresh_from_cache(pos, index);
		return;
	}

	for (int destination = base + 1; destination <= index; destination++) {
		AccumulatorPair &pair = accumulators[destination];
		const AccumulatorPair &previous = accumulators[destination - 1];
		const Update &update = updates[destination];

		if (update.removals == 1) {
			for (int feature = 0; feature < NNUE_ACCUMULATOR_SIZE; feature++) {
				pair.white.values[feature] = previous.white.values[feature]
				                           - nnue_network.accumulator_weights[update.white_indices[0]][feature]
				                           + nnue_network.accumulator_weights[update.white_indices[1]][feature];
				pair.black.values[feature] = previous.black.values[feature]
				                           - nnue_network.accumulator_weights[update.black_indices[0]][feature]
				                           + nnue_network.accumulator_weights[update.black_indices[1]][feature];
			}
		} else {
			for (int feature = 0; feature < NNUE_ACCUMULATOR_SIZE; feature++) {
				pair.white.values[feature] = previous.white.values[feature]
				                           - nnue_network.accumulator_weights[update.white_indices[0]][feature]
				                           - nnue_network.accumulator_weights[update.white_indices[1]][feature]
				                           + nnue_network.accumulator_weights[update.white_indices[2]][feature];
				pair.black.values[feature] = previous.black.values[feature]
				                           - nnue_network.accumulator_weights[update.black_indices[0]][feature]
				                           - nnue_network.accumulator_weights[update.black_indices[1]][feature]
				                           + nnue_network.accumulator_weights[update.black_indices[2]][feature];
			}
		}
		pair.correct = true;
	}
}

void AccumulatorManager::make_move(Position &pos, Move move, Position &pos_after) {
	index++;
	AccumulatorPair &pair = accumulators[index];
	const AccumulatorPair &previous = accumulators[index - 1];
	pair.correct = false;
	pair.white_bucket = white_king_bucket(pos_after);
	pair.black_bucket = black_king_bucket(pos_after);

	if (pair.white_bucket != previous.white_bucket || pair.black_bucket != previous.black_bucket) {
		refresh_from_cache(pos_after, index);
		return;
	}

	Update &update = updates[index];
	update = {};
	bool capture = pos.is_capture(move);
	Piece moving_piece = pos.mailbox[move.src()];
	PieceType moving_type = PieceType(moving_piece & 7);

	auto add_delta = [&](int slot, Square square, PieceType type, bool side) {
		update.white_indices[slot] = nnue_index(square, type, side, false, pair.white_bucket);
		update.black_indices[slot] = nnue_index(square, type, side, true, pair.black_bucket);
	};

	// Removals precede additions so the compact update can apply each group in order.
	add_delta(update.removals++, move.src(), moving_type, pos.side);
	if (capture) {
		Piece captured = pos.mailbox[move.dst()];
		add_delta(update.removals++, move.dst(), PieceType(captured & 7), !pos.side);
	}

	PieceType destination_type = move.type() == PROMOTION ? FERZ : moving_type;
	add_delta(update.removals + update.additions++, move.dst(), destination_type, pos.side);
}
