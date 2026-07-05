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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with PZShatranjBot. If not, see <https://www.gnu.org/licenses/>.
 */

#include "includes.hpp"

#include <random>
#include <sstream>
#include <thread>

#include "bitboard.hpp"
#include "datagen.hpp"
#include "eval.hpp"
#include "history.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "threads.hpp"
#include "ttable.hpp"

#define MAX_TT (262144)
#include "params.hpp"

// Options
size_t TT_SIZE = DEFAULT_TT_SIZE;
bool quiet = false;
int move_overhead = 0;

uint64_t timemgmt(int64_t remtime, int64_t inc = 0) {
	// Return time in ms that we can spend on this move
	return std::max(1ll, (long long)(remtime * (tm_rem() / 100.0) + inc * (tm_inc() / 100.0)));
}

void run_uci() {
	Pool pool;
	std::string command;
	Position pos = Position();
	RepetitionHandler rp;
	rp.push_hash(pos.zobrist);
	while (getline(std::cin, command)) {
		if (command == "uci") {
			std::cout << "id name PZShatranjBot " << VERSION << std::endl;
			std::cout << "id author kevlu8 and wdotmathree" << std::endl;
			std::cout << "option name UCI_Variant type combo default shatranj var shatranj" << std::endl;
			std::cout << "option name Hash type spin default 16 min 1 max " << MAX_TT << std::endl;
			std::cout << "option name Threads type spin default 1 min 1 max " << MAX_THREADS << std::endl;
			std::cout << "option name Quiet type check default false" << std::endl;
			std::cout << "option name Move Overhead type spin default 0 min 0 max 10000" << std::endl;
			std::cout << "option name softnodes type check default false" << std::endl;
			std::cout << "option name datagen type check default false" << std::endl;
			std::cout << "option name UCI_ShowWDL type check default false" << std::endl;
			print_uci();
			std::cout << "uciok" << std::endl;
		} else if (command == "icu") {
			return; // exit uci mode
		} else if (command == "isready") {
			std::cout << "readyok" << std::endl;
		} else if (command.substr(0, 9) == "setoption") {
			std::string optionname, optionvalue, token;
			std::stringstream ss(command);
			ss >> token;
			while (ss >> token) {
				if (token == "name") {
					ss >> optionname;
				} else if (token == "value") {
					ss >> optionvalue;
				}
			}
			if (optionname == "Hash") {
				long long optionint = std::stoll(optionvalue);
				if (optionint < 1 || optionint > MAX_TT) {
					std::cerr << "Invalid hash size: " << optionint << std::endl;
					continue;
				}
				TT_SIZE = MAX_TT;
				while (TT_SIZE > optionint) TT_SIZE /= 2;
				TT_SIZE *= 1024 * 1024 / sizeof(TTable::TTBucket);
				ttable.resize(TT_SIZE);
			} else if (optionname == "Quiet") {
				quiet = optionvalue == "true";
			} else if (optionname == "Threads") {
				size_t num_threads = std::stoi(optionvalue);
				if (num_threads < 1 || num_threads > MAX_THREADS) {
					std::cerr << "Invalid number of threads: " << num_threads << std::endl;
					num_threads = 1;
				}
				pool.resize(num_threads);
				std::cout << "info string Using " << num_threads << " threads" << std::endl;
			} else if (optionname == "Move") {
				int overhead = std::stoi(optionvalue);
				if (overhead < 0 || overhead > 10000) {
					std::cerr << "Invalid move overhead: " << overhead << std::endl;
					overhead = 0;
				}
				move_overhead = overhead;
			} else if (optionname == "softnodes") {
				do_softnodes = optionvalue == "true";
				std::cout << "info string softnodes " << (do_softnodes ? "enabled" : "disabled") << std::endl;
			} else if (optionname == "datagen") {
				do_datagen = optionvalue == "true";
				std::cout << "info string datagen " << (do_datagen ? "enabled" : "disabled") << std::endl;
			} else if (optionname == "UCI_ShowWDL") {
				show_wdl = (optionvalue == "true");
			} else {
				handle_set(optionname, optionvalue);
			}
		} else if (command == "ucinewgame") {
			stop_search = true;
			pool.wait_finished();
			pos = Position();
			rp.clear();
			rp.push_hash(pos.zobrist);
			ttable.resize(TT_SIZE);
			pool.clear_search_vars();
		} else if (command.substr(0, 8) == "position") {
			// either `position startpos` or `position fen ...`
			if (command.find("startpos") != std::string::npos) {
				pos.reset_startpos();
				rp.clear();
				rp.push_hash(pos.zobrist);
			} else if (command.find("fen") != std::string::npos) {
				std::string fen = command.substr(command.find("fen") + 4);
				if (fen.find("moves") != std::string::npos) {
					fen = fen.substr(0, fen.find("moves"));
				}
				pos.reset(fen);
				rp.clear();
				rp.push_hash(pos.zobrist);
			}
			if (command.find("moves") != std::string::npos) {
				std::string moves = command.substr(command.find("moves") + 6);
				std::stringstream ss(moves);
				std::string move;
				while (ss >> move) {
					pos.make_move(Move::from_string(move, &pos));
					rp.push_hash(pos.zobrist);
				}
			}
		} else if (command == "quit") {
			stop_search = true;
			pool.wait_finished();
			exit(0);
		} else if (command == "stop") {
			stop_search = true;
			pool.wait_finished();
		} else if (command == "eval") {
			std::array<Value, 8> score = debug_eval(pos);
			pos.print_board();
			std::cout << "info string fen " << pos.get_fen() << std::endl;
			int nbucket = (arch::popcnt(pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)]) - 2) / 4;
			for (int i = 0; i < 8; i++) {
				std::cout << "info string eval " << i << ": " << score[i];
				if (i == nbucket) {
					std::cout << " (current)";
				}
				std::cout << std::endl;
			}
		} else if (command.substr(0, 2) == "go") {
			pool.wait_finished();
			// `go wtime ... btime ... winc ... binc ...`
			// only care about wtime and btime
			std::stringstream ss(command);
			std::string token;
			int wtime = 0, btime = 0, winc = 0, binc = 0;
			int depth = -1;
			int nodes = -1;
			bool inf = false;
			int movetime = -1;
			int perft_depth = -1;
			ss >> token;
			while (ss >> token) {
				if (token == "wtime") {
					ss >> wtime;
				} else if (token == "btime") {
					ss >> btime;
				} else if (token == "winc") {
					ss >> winc;
				} else if (token == "binc") {
					ss >> binc;
				} else if (token == "depth") {
					ss >> depth;
				} else if (token == "infinite") {
					inf = true;
				} else if (token == "nodes") {
					ss >> nodes;
				} else if (token == "movetime") {
					ss >> movetime;
				} else if (token == "perft") {
					ss >> perft_depth;
				}
			}
			if (perft_depth != -1) {
				uint64_t tot_nodes = 0;
				pzstd::vector<Move> moves;
				pos.legal_moves(moves);
				for (Move &move : moves) {
					if (!pos.is_legal(move))
						continue;
					Position pos_after = pos;
					pos_after.make_move(move);
					rp.push_hash(pos_after.zobrist);
					uint64_t cnt = perft(pos_after, perft_depth - 1);
					rp.pop_hash();
					std::cout << move.to_string() << ": " << cnt << std::endl;
					tot_nodes += cnt;
				}
				std::cout << "Total nodes: " << tot_nodes << std::endl;
				continue;
			}
			int timeleft = pos.side ? btime : wtime;
			int inc = pos.side ? binc : winc;

			if (!quiet)
				std::cout << "info string Starting search..." << std::endl;

			timeleft = std::max(1, timeleft - move_overhead);

			if (inf)
				pool.search(pos, rp, 1e18, MAX_PLY, 1e18, quiet);
			else if (depth != -1)
				pool.search(pos, rp, 1e18, depth, 1e18, quiet);
			else if (nodes != -1)
				pool.search(pos, rp, 1e18, MAX_PLY, nodes, quiet);
			else if (movetime != -1)
				pool.search(pos, rp, movetime, MAX_PLY, 1e18, quiet);
			else
				pool.search(pos, rp, timemgmt(timeleft, inc), MAX_PLY, 1e18, quiet);
		} else if (command == "wait") {
			pool.wait_finished();
		}
	}
	stop_search = true;
	pool.wait_finished();
}

int main(int argc, char *argv[]) {
	print_config();
	if (argc >= 2 && (std::string(argv[1]) == "datagen" || std::string(argv[1]).starts_with("datagen "))) {
		std::string command;
		for (int i = 1; i < argc; i++) {
			if (!command.empty()) command += ' ';
			command += argv[i];
		}
		std::stringstream ss(command);
		std::string token;
		uint64_t target_positions = 0;
		std::optional<uint64_t> seed;
		std::string output_file = "data.bullet.txt";
		ss >> token >> target_positions;
		while (ss >> token) {
			if (token == "seed") {
				uint64_t value;
				ss >> value;
				seed = value;
			}
			else if (token == "output") ss >> output_file;
		}
		if (target_positions == 0) {
			std::cerr << "Usage: pzshatranjbot datagen <positions> [seed <seed>] [output <file>]" << std::endl;
			return 1;
		}
		return run_datagen(target_positions, seed, output_file);
	}
	if (argc >= 2 && std::string(argv[1]) == "bench") {
		// Shatranj positions from the Prolix benchmark suite.
		const std::string bench_positions[] = {
			"r5r1/1k6/1pqb4/1Bppn1p1/P1n1p2p/P1N1P2P/2KQ1p2/1RBR2N1 w - - 0 45",
			"8/1R6/4q3/3Nk1p1/2P3p1/3PK3/8/8 w - - 2 83",
			"8/8/8/1KQQQ3/2P3qP/5k2/7b/8 b - - 20 76",
			"2r1r3/p1pk1ppp/bpnpp2b/8/3P4/BPQ1PN1P/P1P1KPP1/R6R b - - 1 14",
			"3kq3/3p4/3p1p2/6pK/1R1Q4/1P1B1r2/8/8 w - - 2 44",
			"1nbkq3/1rpppr1p/3b1p2/p1PP1Pp1/1p6/PP1NP1PB/3Q3n/RNBKR3 w - - 0 20",
			"r4br1/8/p2k2qp/7n/1R1N4/3BB1P1/P2PPQ1P/3K4 w - - 3 32",
			"8/8/8/8/3Qk1n1/2K1P3/8/8 b - - 46 162",
			"rnbkqbnr/ppppp1p1/5p1p/8/8/3P2P1/PPP1PP1P/RNBKQBNR w - - 0 1",
			"5b1r/8/1p1pq1p1/p1k3P1/5RP1/P1PB4/4KQ2/8 w - - 1 44",
			"2r1qr2/8/1pkp2pb/p2pn1N1/3R2PP/3BP1Q1/P1P1R3/2K5 b - - 6 30",
			"1r2q3/R4pn1/1p1pkn2/3p1p2/1PpP2p1/N1P1K1P1/3Q3P/2B1R3 b - - 5 31",
			"8/1Q6/3Q4/3p1p2/2pkq2R/5q2/5K2/8 w - - 2 116",
			"8/4k3/4R3/2PK4/1P3Nn1/P2PPn2/5r2/8 b - - 2 58",
		};
		int bench_depth = 18;
		if (argc == 3) {
			// get bench depth
			bench_depth = std::stoi(argv[2]);
		}
		Pool pool;
		Position pos = Position();
		RepetitionHandler rp;
		uint64_t tot_nodes = 0;
		uint64_t start = clock();
		for (const auto &fen : bench_positions) {
			pos.reset(fen);
			rp.clear();
			pool.clear_search_vars();
			pool.search(pos, rp, 1e9, bench_depth, 1e18, 0);
			pool.wait_finished();
			tot_nodes += nodes[0].get();
		}
		uint64_t end = clock();
		std::cout << tot_nodes << " nodes " << int(tot_nodes / ((double)(end - start) / CLOCKS_PER_SEC)) << " nps" << std::endl;
		return 0;
	}
	if (argc == 2 && std::string(argv[1]) == "pawnvalue") {
		// calculate pawn value
		Position pos = Position();
		int tot = 0;
		Value startpos_score = eval(pos);
		for (int i = 0; i < 8; i++) {
			pos.reset_startpos();
			Square square = Square(SQ_A2 + i);
			pos.mailbox[square] = NO_PIECE;
			pos.piece_boards[PAWN] ^= square_bits(square);
			pos.piece_boards[OCC(WHITE)] ^= square_bits(square);
			Value score = eval(pos);
			int diff = startpos_score - score;
			tot += diff;
		}
		tot /= 8;
		tot = tot * 3 / 4; // scale down a little because startpos pawns are generally more valuable
		std::cout << "info string Pawn value: " << tot << std::endl;
		return 0;
	}
	if (argc == 2 && std::string(argv[1]) == "avgeval") {
		// assume book is at ./lichess-big3-resolved.txt
		Position pos = Position();
		std::ifstream bookfile("./lichess-big3-resolved.txt");
		std::string line;
		int64_t tot_eval = 0;
		int npositions = 0;
		if (!bookfile.is_open()) {
			std::cerr << "Could not open book file" << std::endl;
			return 1;
		}
		while (getline(bookfile, line)) {
			std::string fen = line.substr(0, line.find(' '));
			pos.reset(fen);
			Value score = abs(eval(pos));
			tot_eval += score;
			npositions++;
		}
		bookfile.close();
		std::cout << "info string Average eval over " << npositions << " positions: " << (tot_eval / npositions) << std::endl;
		return 0;
	}
	std::cout << "PZShatranjBot " << VERSION << " developed by kevlu8 and wdotmathree" << std::endl;
	run_uci();
}
