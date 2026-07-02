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

#include "bitboard.hpp"
#include <cctype>
#include <random>

uint64_t zobrist_square[64][15];
uint64_t zobrist_side;

__attribute__((constructor)) void init_zobrist() {
	std::mt19937_64 rng(0xdeadbeef);
	std::uniform_int_distribution<uint64_t> dist;

	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 14; j++) {
			zobrist_square[i][j] = dist(rng);
		}
		zobrist_square[i][14] = 0;
	}

	zobrist_side = dist(rng);
}

void print_bitboard(Bitboard board) {
	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			std::cout << (((board >> (i * 8 + j)) & 1) ? "X " : ". ");
		}
		std::cout << '\n';
	}
}

std::string Move::to_string() const {
	if (data == 0)
		return "0000";
	std::string str = "";
	str += (char)('a' + (src() & 0b111));
	str += (char)('1' + (src() >> 3));
	str += (char)('a' + (dst() & 0b111));
	str += (char)('1' + (dst() >> 3));
	if ((data & 0xc000) == PROMOTION) {
		str += 'q';
	}
	return str;
}

Move Move::from_string(const std::string &str, const void *) {
	if (str == "0000")
		return NullMove;
	int src_file = str[0] - 'a';
	int src_rank = str[1] - '1';
	int dst_file = str[2] - 'a';
	int dst_rank = str[3] - '1';
	int src = src_rank * 8 + src_file;
	int dst = dst_rank * 8 + dst_file;

	if (str.size() == 5) {
		// Promotion move
		char promo = std::tolower(str[4]);
		// Shatranj promotion is always to ferz (q)
		if (promo != 'q')
			return NullMove;
		return Move::make<PROMOTION>(src, dst);
	}
	return Move(src, dst);
}

void Position::load_fen(std::string fen) {
	if (fen == "startpos")
		fen = "rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR w - -";

	memset(piece_boards, 0, sizeof(piece_boards));
	memset(mailbox, NO_PIECE, sizeof(mailbox));
	uint16_t rank = RANK_8;
	uint16_t file = FILE_A;
	int inputIdx = 0;
	// Load piece data
	while (rank <= RANK_8) { // Will catch wrap-around subtractions
		char cur = fen[inputIdx++];
		if (cur == '/') // Ignore slashes
			continue;
		if (cur <= '9') { // Character is a number indicating blank squares
			// Skip the required amount of squares
			file += cur - '0';
		} else { // Must be a piece
			// We ignore the first first part of the ASCII representation
			PieceType piece = letter_piece[(cur & 0x1f) - 2];
			piece_boards[piece] |= square_bits((Rank)rank, (File)file);
			// Uppercase in ASCII is 0b010xxxxx, lowercase is 0b011xxxxx
			// Chooses either [6] or [7] (white or black occupancy)
			piece_boards[(6 ^ 0b10) ^ (cur >> 5)] |= square_bits((Rank)rank, (File)file);
			mailbox[rank * 8 + file] = Piece(piece + ((cur >> 2) & 0b1000));
			file++;
		}
		// Fix overflow
		rank -= file >> 3;
		file &= 7;
	}
	// Set side according to FEN data ('w' (odd) for white, 'b' (even) for black)
	side = (~fen[inputIdx + 1]) & 1;
	inputIdx += 3;

	// Consume the two shatranj FEN placeholder fields.
	for (int field = 0; field < 2; field++) {
		while (inputIdx < fen.size() && fen[inputIdx] != ' ') inputIdx++;
		if (inputIdx < fen.size()) inputIdx++;
	}

	// Get halfmove clock
	halfmove = 0;
	while (inputIdx < fen.size() && std::isdigit(fen[inputIdx])) {
		halfmove *= 10;
		halfmove += fen[inputIdx] - '0';
		inputIdx++;
	}

	// Ignore the rest (who cares anyways)

	// Recompute hash
	recompute_hash();
	update_control();
}

void Position::reset_pos() {
	piece_boards[0] = Rank2Bits | Rank7Bits;  // Pawns
	piece_boards[ALFIL] = square_bits(SQ_C1) | square_bits(SQ_F1) | square_bits(SQ_C8) | square_bits(SQ_F8);
	piece_boards[FERZ] = square_bits(SQ_E1) | square_bits(SQ_E8);
	piece_boards[KNIGHT] = square_bits(SQ_B1) | square_bits(SQ_G1) | square_bits(SQ_B8) | square_bits(SQ_G8);
	piece_boards[ROOK] = square_bits(SQ_A1) | square_bits(SQ_H1) | square_bits(SQ_A8) | square_bits(SQ_H8);
	piece_boards[KING] = square_bits(SQ_D1) | square_bits(SQ_D8);
	piece_boards[6] = Rank1Bits | Rank2Bits;  // White occupancy
	piece_boards[7] = Rank7Bits | Rank8Bits;  // Black occupancy

	Piece starting_mailbox[64] = {
		WHITE_ROOK, WHITE_KNIGHT, WHITE_ALFIL, WHITE_KING, WHITE_FERZ, WHITE_ALFIL, WHITE_KNIGHT, WHITE_ROOK,
		WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,  WHITE_PAWN, WHITE_PAWN,   WHITE_PAWN,   WHITE_PAWN,
		NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,    NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,
		NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,    NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,
		NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,    NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,
		NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,    NO_PIECE,   NO_PIECE,     NO_PIECE,     NO_PIECE,
		BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,  BLACK_PAWN, BLACK_PAWN,   BLACK_PAWN,   BLACK_PAWN,
		BLACK_ROOK, BLACK_KNIGHT, BLACK_ALFIL, BLACK_KING, BLACK_FERZ, BLACK_ALFIL, BLACK_KNIGHT, BLACK_ROOK
	};
	memcpy(mailbox, starting_mailbox, sizeof(mailbox));

	side = WHITE;
	halfmove = 0;
	fullmove = 0;
	recompute_hash();
	update_control();
}

std::string Position::get_fen() const {
	std::string res = "";
	for (int rank = RANK_8; rank >= 0; rank--) {
		int empty = 0;
		for (int file = FILE_A; file <= FILE_H; file++) {
			if (mailbox[rank * 8 + file] == NO_PIECE) {
				empty++;
			} else {
				if (empty > 0) {
					res += std::to_string(empty);
					empty = 0;
				}
				res += piece_letter[mailbox[rank * 8 + file]];
			}
		}
		if (empty > 0) {
			res += std::to_string(empty);
		}
		if (rank > 0) {
			res += '/';
		}
	}
	res += side ? " b - - " : " w - - ";
	// Halfmove clock
	res += std::to_string(halfmove);

	// Fullmove number
	res += ' ';
	res += std::to_string((fullmove / 2) + 1);
	return res;
}

bool Position::sanity_check(char *print) {
	bool error = 0;
	// Start at -1 because we increment before processing to guarantee it happens in every case
	int printIdx = -1;
	// Occupancy
	Bitboard occ = piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)];
	// Used for sanity checks during debugging (piece set but no occupancy)
	Bitboard sanity = (piece_boards[0] | piece_boards[1] | piece_boards[2] | piece_boards[3] | piece_boards[4] | piece_boards[5]) ^ occ;
	for (int rank = RANK_8; rank >= 0; rank--) {
		for (int file = FILE_A; file <= FILE_H; file++) {
			Bitboard bits = square_bits((Rank)rank, (File)file);
			printIdx++;
			if (mailbox[rank * 8 + file] != NO_PIECE && !(occ & square_bits((Rank)rank, (File)file))) { // Occupancy and mailbox differ
				print[printIdx] = '$';
				std::cerr << "Occupancy and mailbox differ on square " << char(file + 'a') << char(rank + '1') << '\n';
				error = 1;
				continue;
			}
			if (sanity & bits) { // Occupancy and collective piece boards differ
				if (occ & bits) { // Occupied but no piece specified
					print[printIdx] = '?';
					std::cerr << "Occupied but no piece specified on square " << char(file + 'a') << char(rank + '1') << '\n';
					error = 1;
				} else { // Piece specified but no occupancy
					print[printIdx] = '!';
					std::cerr << "Piece specified but no occupancy on square " << char(file + 'a') << char(rank + '1') << '\n';
					error = 1;
				}
				continue;
			}
			if (!(occ & bits)) {
				print[printIdx] = '.';
				continue;
			}

			bool put = false;
			// Scan through piece types trying to find one that matches
			for (int type = PAWN; type < NO_PIECETYPE; type++) {
				if (!(piece_boards[type] & bits))
					continue;
				if (put) { // More than two pieces on one square
					print[printIdx] = '&';
					std::cerr << "More than two pieces on square " << char(file + 'a') << char(rank + '1') << '\n';
					error = 1;
					break;
				}
				put = true;
				if (mailbox[rank * 8 + file] != type + (!!(piece_boards[OCC(BLACK)] & bits) << 3)) { // Bitboard and mailbox representations differ
					print[printIdx] = '$';
					std::cerr << "Bitboard and mailbox representations differ on square " << char(file + 'a') << char(rank + '1') << '\n';
					error = 1;
					break;
				}
				print[printIdx] = piecetype_letter[type & 7] - (!(piece_boards[OCC(BLACK)] & bits) << 5);
			}
		}
	}

	bool ctrlerror = 0;
	for (int i = 0; i < 64; i++) {
		if (control((Square)i, WHITE) != !!(side_control[WHITE] & square_bits((Square)i))) {
			std::cerr << "Control bitboard for white on square " << char((i & 7) + 'a') << char((i >> 3) + '1') << " is incorrect\n";
			ctrlerror = 1;
		}
		if (control((Square)i, BLACK) != !!(side_control[BLACK] & square_bits((Square)i))) {
			std::cerr << "Control bitboard for black on square " << char((i & 7) + 'a') << char((i >> 3) + '1') << " is incorrect\n";
			ctrlerror = 1;
		}
	}
	if (ctrlerror) {
		std::cerr << "White control bitboard: " << std::hex << side_control[WHITE] << std::dec << '\n';
		print_bitboard(side_control[WHITE]);
		Bitboard ctrl = 0;
		for (int i = 0; i < 64; i++) {
			if (control((Square)i, WHITE)) {
				ctrl |= square_bits((Square)i);
			}
		}
		std::cerr << "Computed white control bitboard: " << std::hex << ctrl << std::dec << '\n';
		print_bitboard(ctrl);

		std::cerr << "Black control bitboard: " << std::hex << side_control[BLACK] << std::dec << '\n';
		print_bitboard(side_control[BLACK]);
		ctrl = 0;
		for (int i = 0; i < 64; i++) {
			if (control((Square)i, BLACK)) {
				ctrl |= square_bits((Square)i);
			}
		}
		std::cerr << "Computed black control bitboard: " << std::hex << ctrl << std::dec << '\n';
		print_bitboard(ctrl);
		std::cerr << std::endl;

		error = 1;
	}

	return error;
}

void Position::print_board() const {
	std::cout << "- - " << (side ? "black" : "white") << '\n';

#ifdef DEBUG
	char print[64];
	// Start at -1 because we increment before processing to guarantee it happens in every case
	int printIdx = -1;
	// Occupancy
	Bitboard occ = piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)];
	// Used for sanity checks during debugging (piece set but no occupancy)
	Bitboard sanity = (piece_boards[0] | piece_boards[1] | piece_boards[2] | piece_boards[3] | piece_boards[4] | piece_boards[5]) ^ occ;
	for (int rank = RANK_8; rank >= 0; rank--) {
		for (int file = FILE_A; file <= FILE_H; file++) {
			Bitboard bits = square_bits((Rank)rank, (File)file);
			printIdx++;
			if (mailbox[rank * 8 + file] != NO_PIECE && !(occ & square_bits((Rank)rank, (File)file))) { // Occupancy and mailbox differ
				print[printIdx] = '$';
				continue;
			}
			if (sanity & bits) { // Occupancy and collective piece boards differ
				if (occ & bits) // Occupied but no piece specified
					print[printIdx] = '?';
				else // Piece specified but no occupancy
					print[printIdx] = '!';
				continue;
			}
			if (!(occ & bits)) {
				print[printIdx] = '.';
				continue;
			}

			bool put = false;
			// Scan through piece types trying to find one that matches
			for (int type = PAWN; type < NO_PIECETYPE; type++) {
				if (!(piece_boards[type] & bits))
					continue;
				if (put) { // More than two pieces on one square
					print[printIdx] = '&';
					break;
				}
				put = true;
				if (mailbox[rank * 8 + file] != type + (!!(piece_boards[OCC(BLACK)] & bits) << 3)) { // Bitboard and mailbox representations differ
					print[printIdx] = '$';
					break;
				}
				print[printIdx] = piece_letter[mailbox[rank * 8 + file]];
			}
		}
	}
	// Print the board
	for (int i = 0; i < 64; i++) {
		std::cout << print[i];
		if (i % 8 == 7)
			std::cout << '\n';
		else
			std::cout << ' ';
	}
#else
	for (int rank = RANK_8; rank >= 0; rank--) {
		for (int file = FILE_A; file <= FILE_H; file++) {
			std::cout << piece_letter[mailbox[rank * 8 + file]];
			if (file == FILE_H)
				std::cout << '\n';
			else
				std::cout << ' ';
		}
	}
#endif
}

void Position::make_move(Move move) {
#ifdef SANCHECK
	char before[64];
	sanity_check(before);
#endif

#ifdef HASHCHECK
	uint64_t old_hash = zobrist;
	uint64_t old_piece_hashes[15];
	memcpy(old_piece_hashes, piece_hashes, sizeof(piece_hashes));
	recompute_hash();
	if (old_hash != zobrist) {
		std::cerr << "Hash mismatch before move: expected " << zobrist << " got " << old_hash << '\n';
		abort();
	}
	for (int i = 0; i < 15; i++) {
		if (old_piece_hashes[i] != piece_hashes[i]) {
			std::cerr << "Piece hash mismatch before move for piece " << i << ": expected " << piece_hashes[i] << " got " << old_piece_hashes[i] << '\n';
			abort();
		}
	}
#endif

	if (move == NullMove) {
		side = !side;
		zobrist ^= zobrist_side;
		return;
	}

	// Handle captures
	if (is_capture(move)) {
		// Remove whatever piece it was
		uint8_t piece = mailbox[move.dst()] & 0b111;
		piece_boards[piece] ^= square_bits(move.dst());
		piece_boards[OPPOCC(side)] ^= square_bits(move.dst());
		zobrist ^= zobrist_square[move.dst()][mailbox[move.dst()]];
		piece_hashes[mailbox[move.dst()]] ^= zobrist_square[move.dst()][mailbox[move.dst()]];

		halfmove = -1;
	}

	if ((mailbox[move.src()] & 0b111) == PAWN)
		halfmove = -1;

	switch (move.type()) {
	case PROMOTION: {
		// Remove the pawn on the src and add the piece on the dst
		Piece promo_piece = WHITE_FERZ;

		zobrist ^= zobrist_square[move.src()][mailbox[move.src()]];
		piece_hashes[mailbox[move.src()]] ^= zobrist_square[move.src()][mailbox[move.src()]];
		mailbox[move.src()] = NO_PIECE;
		mailbox[move.dst()] = Piece(promo_piece + ((!!side) << 3));
		zobrist ^= zobrist_square[move.dst()][mailbox[move.dst()]];
		piece_hashes[mailbox[move.dst()]] ^= zobrist_square[move.dst()][mailbox[move.dst()]];
		piece_boards[PAWN] ^= square_bits(move.src());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[promo_piece] ^= square_bits(move.dst());
		break;
	}

	case NORMAL: {
		uint8_t piece = mailbox[move.src()] & 0b111;

		zobrist ^= zobrist_square[move.src()][mailbox[move.src()]] ^ zobrist_square[move.dst()][mailbox[move.src()]];
		piece_hashes[mailbox[move.src()]] ^= zobrist_square[move.src()][mailbox[move.src()]] ^ zobrist_square[move.dst()][mailbox[move.src()]];
		mailbox[move.dst()] = mailbox[move.src()];
		mailbox[move.src()] = NO_PIECE;
		piece_boards[piece] ^= square_bits(move.src()) | square_bits(move.dst());
		piece_boards[OCC(side)] ^= square_bits(move.src()) | square_bits(move.dst());
		break;
	}

	default:
		__builtin_unreachable();
	}

	// Switch sides
	side = !side;
	zobrist ^= zobrist_side;

	halfmove++;
	fullmove++;

	update_control();

#ifdef HASHCHECK
	old_hash = zobrist;
	memcpy(old_piece_hashes, piece_hashes, sizeof(piece_hashes));
	recompute_hash();
	if (zobrist != old_hash) {
		print_board();
		std::cerr << "Hash mismatch after make: expected " << old_hash << " got " << zobrist << std::endl;
		std::cerr << "Move: " << move.to_string() << std::endl;
		abort();
	}
	for (int i = 0; i < 15; i++) {
		if (old_piece_hashes[i] != piece_hashes[i]) {
			print_board();
			std::cerr << "Piece hash mismatch after make for piece " << i << ": expected " << old_piece_hashes[i] << " got " << piece_hashes[i] << std::endl;
			std::cerr << "Move: " << move.to_string() << std::endl;
			abort();
		}
	}
#endif

#ifdef SANCHECK
	char after[64];
	if (sanity_check(after)) {
		for (int i = 0; i < 64; i++) {
			std::cout << before[i];
			if (i % 8 == 7)
				std::cout << '\n';
			else
				std::cout << ' ';
		}
		std::cout << "Sanity check failed after make " << move.to_string() << std::endl;
		for (int i = 0; i < 64; i++) {
			std::cout << after[i];
			if (i % 8 == 7)
				std::cout << '\n';
			else
				std::cout << ' ';
		}
		abort();
	}
#endif
}

void Position::recompute_hash() {
	zobrist = 0;
	for (int i = 0; i < 15; i++) piece_hashes[i] = 0;
	for (int i = 0; i < 64; i++) {
		zobrist ^= zobrist_square[i][mailbox[i]];
		piece_hashes[mailbox[i]] ^= zobrist_square[i][mailbox[i]];
	}
	zobrist ^= zobrist_side * side;
}

bool RepetitionHandler::threefold(int ply, uint64_t hash) {
	int cnt = 0, plies = 0;
	for (int idx = hash_hist.size() - 1; idx >= 0; idx--) {
		const uint64_t& h = hash_hist[idx];
		if (h == hash)
			cnt++;
		if (plies < ply && cnt >= 2)
			return true;
		plies++;
		if (cnt >= 3) return true;
	}
	return false;
}

bool Position::bare_king(bool color) const {
	Bitboard pieces = piece_boards[OCC(color)];
	Bitboard king = piece_boards[KING] & pieces;
	return king != 0 && pieces == king;
}

bool Position::two_kings() const {
	Bitboard occupied = piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)];
	return arch::popcnt(piece_boards[KING]) == 2 && occupied == piece_boards[KING];
}

uint64_t Position::pawn_hash() const {
	return piece_hashes[WHITE_PAWN] ^ piece_hashes[BLACK_PAWN];
}

uint64_t Position::nonpawn_hash(bool color) const {
	return piece_hashes[KING + (color << 3)] ^ piece_hashes[FERZ + (color << 3)] ^ piece_hashes[ROOK + (color << 3)] ^ piece_hashes[ALFIL + (color << 3)] ^ piece_hashes[KNIGHT + (color << 3)];
}

uint64_t Position::major_hash() const {
	return piece_hashes[WHITE_KING] ^ piece_hashes[WHITE_FERZ] ^ piece_hashes[WHITE_ROOK] ^ piece_hashes[BLACK_KING] ^ piece_hashes[BLACK_FERZ] ^ piece_hashes[BLACK_ROOK];
}

uint64_t Position::minor_hash() const {
	return piece_hashes[WHITE_KING] ^ piece_hashes[WHITE_ALFIL] ^ piece_hashes[WHITE_KNIGHT] ^ piece_hashes[BLACK_KING] ^ piece_hashes[BLACK_ALFIL] ^ piece_hashes[BLACK_KNIGHT];
}
