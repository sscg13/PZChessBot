/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#pragma once

#include "../bitboard.hpp"
#include "network.hpp"

struct AccumulatorManager {
	struct AccumulatorPair {
		int white_bucket = 0;
		int black_bucket = 0;
		NnueAccumulator white;
		NnueAccumulator black;
		bool correct = false;
	};

	struct Update {
		int white_indices[3]{};
		int black_indices[3]{};
		int removals = 0;
		int additions = 0;
	};

	static constexpr int KingBucketCount = NNUE_INPUT_BUCKETS * 2;

	struct Cache {
		AccumulatorPair accumulators[KingBucketCount];
		Piece mailboxes[KingBucketCount][2][64];

		Cache();
	};

	AccumulatorPair accumulators[MAX_PLY + 5];
	Update updates[MAX_PLY + 5];
	Cache cache;
	int index = 0;

	AccumulatorManager(const AccumulatorManager &) = delete;
	explicit AccumulatorManager(Position &pos) { reset(pos); }

	AccumulatorPair &current() { return accumulators[index]; }

	void reset(Position &pos);
	void full_refresh(Position &pos, int destination);
	void refresh_from_cache(Position &pos, int destination);
	void apply_lazy(Position &pos);
	void make_move(Position &pos, Move move, Position &pos_after);

	void pop_move() {
		accumulators[index].correct = false;
		index--;
	}
};
