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
	memcpy(&output_bias, ptr, sizeof(output_bias));
}

void Network::initialize(const Board& board) {
	std::cout << "initializing net..." << std::endl;
	ply = 0;
	Square whiteksq = Square(_tzcnt_u64(board.piece_boards[WHITE] & board.piece_boards[2+KING]));
	accumulators[ply].ksq[WHITE] = whiteksq;
	Square blackksq = Square(_tzcnt_u64(board.piece_boards[BLACK] & board.piece_boards[2+KING]));
	accumulators[ply].ksq[BLACK] = blackksq;
	Bitboard occ = (board.piece_boards[WHITE] | board.piece_boards[BLACK]);
	accumulators[ply].occ = occ;
	for (int i = 0; i < 64; i++) {
		accumulators[ply].mailbox[i] = board.mailbox[i];
	}
	std::cout << "set up king squares, occupancy, mailbox..." << std::endl;
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
	std::cout << "reset accumulators..." << std::endl;
	IndexList<96>& threats = accumulators[ply].threats[Perspective];
	threats.clear();
	IndexList<32> psq;
	psq.clear();
	std::cout << "preparing psq, threat index lists..." << std::endl;
	threat_indexer.append_active_features<Perspective>(board.piece_boards, board.mailbox, psq, threats);
	std::cout << "updating accumulators..." << std::endl;
	std::cout << "psq features..." << std::endl;
	for (int index : psq) {
		std::cout << index << " ";
		accumulators[ply].add_weights<Perspective>(accumulator_weights[index]);
	}
	std::cout << "threat features..." << std::endl;
	for (int index : threats) {
		std::cout << index << " ";
		accumulators[ply].add_weights<Perspective>(accumulator_weights[index]);
	}
	std::cout << std::endl << "success" << std::endl;
}

template<bool Perspective> void Network::update_perspective_forward(const Board& board) {
	Square ksq = Square(_tzcnt_u64(board.piece_boards[Perspective] & board.piece_boards[2+KING]));
	Square oldksq = accumulators[ply].ksq[Perspective];
	ply++;
	accumulators[ply].ksq[Perspective] = ksq;
	Bitboard occ = (board.piece_boards[WHITE] | board.piece_boards[BLACK]);
	accumulators[ply].occ = occ;
	for (int i = 0; i < 64; i++) {
		accumulators[ply].mailbox[i] = board.mailbox[i];
	}
	if (Full_Threats::OrientTBL[Perspective][ksq] != Full_Threats::OrientTBL[Perspective][oldksq]) {
		update_perspective_scratch<Perspective>(board);
	}
	else {
		Bitboard prevocc = accumulators[ply-1].occ;
		Bitboard commonocc = (occ & prevocc);
		occ ^= commonocc;
		prevocc ^= commonocc;
		threat_indexer.append_active_threats<Perspective>(board.piece_boards, board.mailbox, accumulators[ply].threats[Perspective]);
		IndexList<96> added;
		IndexList<96> removed;
		added.clear();
		removed.clear();
		write_difference(accumulators[ply].threats[Perspective], accumulators[ply-1].threats[Perspective], added, removed);
		while (occ) {
			Square sq = pop_lsb(occ);
			Piece pc = board.mailbox[sq];
			added.push_back(threat_indexer.make_psq_index<Perspective>(pc, sq, ksq));
		}
		while (prevocc) {
			Square sq = pop_lsb(occ);
			Piece pc = accumulators[ply-1].mailbox[sq];
			removed.push_back(threat_indexer.make_psq_index<Perspective>(pc, sq, ksq));
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

		input = std::clamp((int)accumulators[ply].accumulation[!color][i], 0, QA);
		weight = input * output_weights[bucket][HL_SIZE + i];
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
template void Network::update_perspective_forward<WHITE>(const Board& board);
template void Network::update_perspective_forward<BLACK>(const Board& board);