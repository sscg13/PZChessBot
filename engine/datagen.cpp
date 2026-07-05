/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include "datagen.hpp"

#include "bitboard.hpp"
#include "search.hpp"
#include "threads.hpp"
#include "ttable.hpp"

#include <chrono>
#include <cctype>
#include <random>
#include <vector>

namespace {

constexpr int RandomOpeningPlies = 8;
constexpr int WinAdjudicationScore = 2000;
constexpr int WinAdjudicationPlies = 6;
constexpr int DrawAdjudicationScore = 10;
constexpr int DrawAdjudicationPlies = 8;
constexpr int DrawAdjudicationMinPly = 80;
constexpr uint64_t SoftNodeLimit = 5000;
constexpr uint64_t HardNodeLimit = 100000;
constexpr int MaximumGamePlies = 1000;

enum class GameResult { ONGOING, WHITE_WIN, DRAW, BLACK_WIN };

struct TrainingPosition {
	std::string fen;
	Value score;
};

const char *result_string(GameResult result) {
	switch (result) {
	case GameResult::WHITE_WIN: return "1.0";
	case GameResult::DRAW: return "0.5";
	case GameResult::BLACK_WIN: return "0.0";
	default: return "";
	}
}

GameResult winner(bool color) {
	return color == WHITE ? GameResult::WHITE_WIN : GameResult::BLACK_WIN;
}

uint64_t randomized_seed() {
	std::random_device random;
	uint64_t seed = uint64_t(random()) << 32 | random();
	return seed ^ uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
}

// Prolix-style FRS has 360 legal back ranks: alfils start on opposite
// square colors, then the ferz, king, knights, and rooks fill the remainder.
std::string frs_backrank(int seed, bool black) {
	constexpr int triangle[] = {-12, 0, 1, 2, 4};
	int order[] = {1, 2, 3, 4, 5, 6, 7, 8};
	char rank[8] = {};
	auto put = [&](int square, char piece) {
		rank[square] = black ? char(std::tolower(piece)) : piece;
		order[square] = 0;
		for (int i = square + 1; i < 8; i++) order[i] = std::max(order[i] - 1, 0);
	};

	put(2 + 4 * (seed % 2), 'B');
	seed /= 2;
	put(1 + 4 * (seed % 2), 'B');
	seed /= 2;

	int choice = -1;
	for (int square = 0, available = -1; square < 8; square += 2) {
		if (order[square] && ++available == seed % 3) choice = square;
	}
	put(choice, 'Q');
	seed /= 3;

	choice = -1;
	for (int square = 0, available = -1; square < 8; square++) {
		if (order[square] && ++available == seed % 5) choice = square;
	}
	put(choice, 'K');
	seed /= 5;

	for (int first = 0; first < 7; first++) {
		for (int second = first + 1; second < 8; second++) {
			if (triangle[order[first]] + triangle[order[second]] == seed + 1) {
				rank[first] = black ? 'n' : 'N';
				rank[second] = black ? 'n' : 'N';
				order[first] = order[second] = 0;
			}
		}
	}
	for (int square = 0; square < 8; square++)
		if (order[square]) rank[square] = black ? 'r' : 'R';
	return std::string(rank, 8);
}

std::string frs_fen(std::mt19937_64 &rng) {
	int position = rng() % 360;
	return frs_backrank(position, true) + "/pppppppp/8/8/8/8/PPPPPPPP/"
	     + frs_backrank(position, false) + " w - - 0 1";
}

GameResult terminal_result(Position &pos, RepetitionHandler &rp) {
	// Match search's rule priority: automatic draws are resolved before wins.
	if (rp.threefold(0, pos.zobrist) || pos.halfmove >= 140 || pos.two_kings())
		return GameResult::DRAW;

	// The bare side has already had its one reply if it is not to move.
	if (pos.bare_king(!pos.side))
		return winner(pos.side);

	if (!pos.has_legal_move())
		return pos.checkers[pos.side] ? winner(!pos.side) : winner(pos.side);

	return GameResult::ONGOING;
}

bool make_random_opening(Position &pos, RepetitionHandler &rp, std::mt19937_64 &rng, int &ply) {
	for (int i = 0; i < RandomOpeningPlies; i++) {
		pzstd::vector<Move> pseudo;
		pzstd::vector<Move> legal;
		pos.legal_moves(pseudo);
		for (Move move : pseudo)
			if (pos.is_legal(move)) legal.push_back(move);
		if (legal.empty()) return false;

		Move move = legal[rng() % legal.size()];
		pos.make_move(move);
		rp.push_hash(pos.zobrist);
		ply++;
		if (terminal_result(pos, rp) != GameResult::ONGOING) return false;
	}
	return true;
}

} // namespace

int run_datagen(uint64_t target_positions, std::optional<uint64_t> requested_seed, const std::string &output_file) {
	std::ofstream output(output_file, std::ios::app);
	if (!output) {
		std::cerr << "Could not open datagen output file: " << output_file << std::endl;
		return 1;
	}

	uint64_t seed = requested_seed.value_or(randomized_seed());
	std::mt19937_64 rng(seed);
	Pool pool;
	uint64_t positions = 0;
	uint64_t games = 0;

	const bool old_softnodes = do_softnodes;
	const bool old_suppress_output = suppress_search_output;
	do_softnodes = true;
	suppress_search_output = true;

	std::cout << "Internal datagen: " << target_positions << " positions, seed " << seed
	          << ", soft node limit " << SoftNodeLimit << ", hard node limit " << HardNodeLimit << std::endl;
	std::cout << "Output: " << output_file << std::endl;

	while (positions < target_positions) {
		pool.clear_search_vars();
		ttable.init_ttable();
		Position pos(frs_fen(rng));
		RepetitionHandler rp;
		rp.push_hash(pos.zobrist);
		int ply = 0;
		if (!make_random_opening(pos, rp, rng, ply)) continue;

		std::vector<TrainingPosition> game;
		GameResult result = GameResult::ONGOING;
		int winning_color = -1;
		int win_count = 0;
		int draw_count = 0;
		bool rejected_opening = false;

		while (result == GameResult::ONGOING && ply < MaximumGamePlies) {
			result = terminal_result(pos, rp);
			if (result != GameResult::ONGOING) break;

			pool.search(pos, rp, 1e9, MAX_PLY, SoftNodeLimit, true, HardNodeLimit);
			auto [best_move, score] = pool.wait_finished();
			if (best_move == NullMove || !pos.is_pseudolegal(best_move) || !pos.is_legal(best_move)) {
				game.clear();
				break;
			}
			if (ply == RandomOpeningPlies && abs(score) > 400) {
				rejected_opening = true;
				break;
			}

			Value white_score = pos.side == WHITE ? score : Value(-score);
			if (pos.halfmove < 40 && !pos.checkers[pos.side] && !pos.is_capture(best_move)
			    && best_move.type() != PROMOTION && abs(score) < VALUE_WIN)
				game.push_back({pos.get_fen(), white_score});

			if (white_score >= WinAdjudicationScore) {
				if (winning_color == WHITE) win_count++;
				else { winning_color = WHITE; win_count = 1; }
			} else if (white_score <= -WinAdjudicationScore) {
				if (winning_color == BLACK) win_count++;
				else { winning_color = BLACK; win_count = 1; }
			} else {
				winning_color = -1;
				win_count = 0;
			}

			pos.make_move(best_move);
			rp.push_hash(pos.zobrist);
			ply++;

			result = terminal_result(pos, rp);
			if (result != GameResult::ONGOING) break;

			if (win_count >= WinAdjudicationPlies) {
				result = winner(winning_color);
				break;
			}

			if (ply > DrawAdjudicationMinPly && abs(score) <= DrawAdjudicationScore)
				draw_count++;
			else
				draw_count = 0;
			if (draw_count >= DrawAdjudicationPlies) {
				result = GameResult::DRAW;
				break;
			}
		}

		if (result == GameResult::ONGOING && ply >= MaximumGamePlies)
			result = GameResult::DRAW;
		if (rejected_opening || result == GameResult::ONGOING || game.empty()) continue;

		for (const TrainingPosition &entry : game)
			output << entry.fen << " | " << entry.score << " | " << result_string(result) << '\n';
		output.flush();
		positions += game.size();
		games++;
		std::cout << positions << '/' << target_positions << " positions in " << games << " games" << std::endl;
	}

	do_softnodes = old_softnodes;
	suppress_search_output = old_suppress_output;
	return 0;
}
