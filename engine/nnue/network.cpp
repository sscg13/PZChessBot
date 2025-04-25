#include "network.hpp"
#include "incbin.h"

extern "C" {
	INCBIN(network_weights, NNUE_PATH);
}

void Network::load() {
	char *ptr = (char *)gnetwork_weightsData;
	memcpy(accumulator_weights, ptr, sizeof(accumulator_weights));
	ptr += sizeof(accumulator_weights);
	memcpy(accumulator_biases, ptr, sizeof(accumulator_biases));
	ptr += sizeof(accumulator_biases);
	memcpy(output_weights, ptr, sizeof(output_weights));
	ptr += sizeof(output_weights);
	memcpy(output_bias, ptr, sizeof(output_bias));
}

PieceChange get_piece_change(const Board& board, const Move& move) {
	PieceChange diff;
	if (move == NullMove) {
		diff.piece_num = 0;
		return diff;
	}
	Square from = move.src();
	Square to = move.dst();
	if (move.type() == NORMAL) {
		diff.piece_num = 1;
		diff.piece[0] = board.mailbox[from];
		diff.from[0] = from;
		diff.to[0] = to;
		if (board.mailbox[to] != NO_PIECE) {
			diff.piece_num = 2;
			diff.piece[1] = board.mailbox[to];
			diff.from[1] = to;
			diff.to[1] = SQ_NONE;
		}
	}
	else if (move.type() == PROMOTION) {
		diff.piece_num = 2;
		diff.piece[0] = board.mailbox[from];
		diff.from[0] = from;
		diff.to[0] = SQ_NONE;
		diff.piece[1] = Piece((board.side << 3) + move.promotion() + KNIGHT);
		diff.from[1] = SQ_NONE;
		diff.to[1] = to;
		if (board.mailbox[to] != NO_PIECE) {
			diff.piece_num = 3;
			diff.piece[2] = board.mailbox[to];
			diff.from[2] = to;
			diff.to[2] = SQ_NONE;
		}
	}
	else if (move.type() == CASTLING) {
		diff.piece_num = 2;
		if (to == SQ_G1) {
			diff.piece[0] = WHITE_KING;
			diff.from[0] = SQ_E1;
			diff.to[0] = SQ_G1;
			diff.piece[1] = WHITE_ROOK;
			diff.from[1] = SQ_H1;
			diff.to[1] = SQ_F1;
		}
		else if (to == SQ_C1) {
			diff.piece[0] = WHITE_KING;
			diff.from[0] = SQ_E1;
			diff.to[0] = SQ_C1;
			diff.piece[1] = WHITE_ROOK;
			diff.from[1] = SQ_A1;
			diff.to[1] = SQ_D1;
		}
		else if (to == SQ_G8) {
			diff.piece[0] = BLACK_KING;
			diff.from[0] = SQ_E8;
			diff.to[0] = SQ_G8;
			diff.piece[1] = BLACK_ROOK;
			diff.from[1] = SQ_H8;
			diff.to[1] = SQ_F8;
		}
		else if (to == SQ_C8) {
			diff.piece[0] = BLACK_KING;
			diff.from[0] = SQ_E8;
			diff.to[0] = SQ_C8;
			diff.piece[1] = BLACK_ROOK;
			diff.from[1] = SQ_A8;
			diff.to[1] = SQ_D8;
		}
	}
	else if (move.type() == EN_PASSANT) {
		diff.piece_num = 2;
		if (to / 8 == 5) {
			diff.piece[0] = WHITE_PAWN;
			diff.from[0] = from;
			diff.to[0] = to;
			diff.piece[1] = BLACK_PAWN;
			diff.from[1] = Square(to - 8);
			diff.to[1] = SQ_NONE;
		}
		else {
			diff.piece[0] = BLACK_PAWN;
			diff.from[0] = from;
			diff.to[0] = to;
			diff.piece[1] = WHITE_PAWN;
			diff.from[1] = Square(to + 8);
			diff.to[1] = SQ_NONE;
		}
	}
	return diff;
}

void Network::initialize(const Board& board) {
	ply = 0;
	Square whiteksq = Square(_tzcnt_u64(board.piece_boards[6+WHITE] & board.piece_boards[KING]));
	accumulators[ply].ksq[WHITE] = whiteksq;
	Square blackksq = Square(_tzcnt_u64(board.piece_boards[6+BLACK] & board.piece_boards[KING]));
	accumulators[ply].ksq[BLACK] = blackksq;
	update_perspective_scratch<WHITE>(board);
	update_perspective_scratch<BLACK>(board);
}

template<bool Perspective> void Accumulator::reset(const int16_t* biases) {
	for (int i = 0; i < HL_SIZE; i++) {
		accumulation[Perspective][i] = biases[i];
	}
}

template<bool Perspective> void Accumulator::add_weights(const int16_t* weights) {
    for (int i = 0; i < HL_SIZE; i++) {
        accumulation[Perspective][i] += weights[i];
    }
}

template<bool Perspective> void Accumulator::sub_weights(const int16_t* weights) {
    for (int i = 0; i < HL_SIZE; i++) {
        accumulation[Perspective][i] -= weights[i];
    }
}

template<bool Perspective> void Network::update_perspective_scratch(const Board& board) {
	accumulators[ply].reset<Perspective>(accumulator_biases);
	IndexList<96>& threats = accumulators[ply].threats[Perspective];
	threats.clear();
	IndexList<32> psq;
	psq.clear();
	threat_indexer.append_active_features<Perspective>(board.piece_boards, board.mailbox, psq, threats);
	for (int index : psq) {
		accumulators[ply].add_weights<Perspective>(accumulator_weights[index]);
	}
	for (int index : threats) {
		accumulators[ply].add_weights<Perspective>(accumulator_weights[index]);
	}
}

template<bool Perspective> void Network::update_perspective_forward(const Board& board, const PieceChange& diff) {
	Square ksq = Square(_tzcnt_u64(board.piece_boards[6+Perspective] & board.piece_boards[KING]));
	Square oldksq = accumulators[ply-1].ksq[Perspective];
	accumulators[ply].ksq[Perspective] = ksq;
	if (Full_Threats::OrientTBL[Perspective][ksq] != Full_Threats::OrientTBL[Perspective][oldksq]) {
		update_perspective_scratch<Perspective>(board);
	}
	else {
		IndexList<96>& threats = accumulators[ply].threats[Perspective];
		threats.clear();
		threat_indexer.append_active_threats<Perspective>(board.piece_boards, board.mailbox, threats);
		IndexList<96> added;
		IndexList<96> removed;
		added.clear();
		removed.clear();
		write_difference(threats, accumulators[ply-1].threats[Perspective], added, removed);
		for (int i = 0; i < diff.piece_num; i++) {
			Piece pc = diff.piece[i];
			Square from = diff.from[i];
			Square to = diff.to[i];
			if (from != SQ_NONE) {
				removed.push_back(threat_indexer.make_psq_index<Perspective>(pc, from, ksq));
			}
			if (to != SQ_NONE) {
				added.push_back(threat_indexer.make_psq_index<Perspective>(pc, to, ksq));
			}
		}
		accumulators[ply].reset<Perspective>(accumulators[ply-1].accumulation[Perspective]);
		for (int index : added) {
			accumulators[ply].add_weights<Perspective>(accumulator_weights[index]);
		}
		for (int index : removed) {
			accumulators[ply].sub_weights<Perspective>(accumulator_weights[index]);
		}
	}
}

int32_t Network::evaluate(bool color, int bucket) {
	/// TODO: vectorize
	int32_t score = 0;
	for (int i = 0; i < HL_SIZE; i++) {
		int input = std::clamp((int)accumulators[ply].accumulation[color][i], 0, QA);
		int weight = input * output_weights[bucket][i];
		score += input * weight;
	}
	for (int i = 0; i < HL_SIZE; i++) {
		int input = std::clamp((int)accumulators[ply].accumulation[!color][i], 0, QA);
		int weight = input * output_weights[bucket][HL_SIZE + i];
		score += input * weight;
	}
	score /= QA;
	score += output_bias[bucket];
	score *= SCALE;
	score /= QA * QB;
	return score;
}

template void Accumulator::reset<WHITE>(const int16_t* biases);
template void Accumulator::reset<BLACK>(const int16_t* biases);
template void Accumulator::add_weights<WHITE>(const int16_t* weights);
template void Accumulator::add_weights<BLACK>(const int16_t* weights);
template void Accumulator::sub_weights<WHITE>(const int16_t* weights);
template void Accumulator::sub_weights<BLACK>(const int16_t* weights);
template void Network::update_perspective_scratch<WHITE>(const Board& board);
template void Network::update_perspective_scratch<BLACK>(const Board& board);
template void Network::update_perspective_forward<WHITE>(const Board& board, const PieceChange& diff);
template void Network::update_perspective_forward<BLACK>(const Board& board, const PieceChange& diff);