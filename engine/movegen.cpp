/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#include "movegen.hpp"

Bitboard knight_movetable[64];
Bitboard king_movetable[64];
Bitboard alfil_movetable[64];
Bitboard ferz_movetable[64];

#ifdef USE_PEXT
Bitboard rook_movetable[102400];

struct MagicEntry {
	Bitboard mask;
	Bitboard *ptr;
};
#else
#include "magics.hpp"
Bitboard sliding_movetable[88507];

struct MagicEntry {
	Bitboard mask;
	uint64_t magic;
	Bitboard *ptr;
};
#endif

Bitboard rook_blockers[64][64];
Bitboard rook_blockers_pure[64][64];
MagicEntry rook_magics[64];

static void gen_rook_moves(int sq, Bitboard piece) {
	Bitboard board = 0;
	Bitboard rank = 0x00000000000000ff;
	Bitboard file = 0x0101010101010101;
	Bitboard rankray = rank << (sq & 0b111000);
	Bitboard fileray = file << (sq & 0b111);
	Bitboard west = rankray & (piece - 1);
	Bitboard south = fileray & (piece - 1);
	Bitboard east = rankray ^ west ^ piece;
	Bitboard north = fileray ^ south ^ piece;

#ifdef USE_PEXT
	Bitboard *ptr = sq == 0 ? rook_movetable : rook_magics[sq].ptr;
#else
	Bitboard *ptr = sliding_movetable + rook_magics_src[sq].offset;
	rook_magics[sq].ptr = ptr;
#endif
	do {
		Bitboard moves = 0;
		moves |= west & board ? west & ~((1ULL << (63 - arch::lzcnt(west & board))) - 1) : west;
		moves |= south & board ? south & ~((1ULL << (63 - arch::lzcnt(south & board))) - 1) : south;
		moves |= east & arch::blsmsk(east & board);
		moves |= north & arch::blsmsk(north & board);
#ifdef USE_PEXT
		*ptr++ = moves;
#else
		uint64_t off = board | ~rook_magics[sq].mask;
		off *= rook_magics[sq].magic;
		off >>= 64 - 12;
		ptr[off] = moves;
#endif
		board = (board - rook_magics[sq].mask) & rook_magics[sq].mask;
	} while (board);
#ifdef USE_PEXT
	if (sq != 63) rook_magics[sq + 1].ptr = ptr;
#else
	rook_magics[sq].mask = ~rook_magics[sq].mask;
#endif

	board = square_bits(Square(sq));
	for (int dst = sq; dst < 64; dst++, board <<= 1) {
		Bitboard between = 0;
		if (board & east) between = east & (arch::blsmsk(board) >> 1);
		else if (board & north) between = north & (arch::blsmsk(board) >> 1);
		else continue;
		rook_blockers[sq][dst] = rook_blockers[dst][sq] = between;
		rook_blockers_pure[sq][dst] = rook_blockers_pure[dst][sq] = between;
	}
	rook_blockers[sq][sq] = 0;
}

__attribute__((constructor)) static void init_movetables() {
	memset(rook_blockers, 0xff, sizeof(rook_blockers));
	memset(rook_blockers_pure, 0, sizeof(rook_blockers_pure));
#ifdef USE_PEXT
	rook_magics[0].ptr = rook_movetable;
#endif

	Bitboard rank = 0x00000000000000ff;
	Bitboard file = 0x0101010101010101;
	for (int sq = 0; sq < 64; sq++) {
		Bitboard piece = square_bits(Square(sq));
		Bitboard hor1 = ((piece & ~FileHBits) << 1) | ((piece & ~FileABits) >> 1);
		Bitboard hor2 = ((piece & ~FileHBits & ~FileGBits) << 2) | ((piece & ~FileABits & ~FileBBits) >> 2);
		knight_movetable[sq] = (hor1 << 16) | (hor1 >> 16) | (hor2 << 8) | (hor2 >> 8);
		Bitboard king = hor1 | piece;
		king_movetable[sq] = (king | (king << 8) | (king >> 8)) ^ piece;

		int f = sq & 7, r = sq >> 3;
		ferz_movetable[sq] = alfil_movetable[sq] = 0;
		for (int df : {-1, 1}) for (int dr : {-1, 1}) {
			int ff = f + df, rr = r + dr;
			if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
				ferz_movetable[sq] |= square_bits(Square(rr * 8 + ff));
			ff = f + 2 * df; rr = r + 2 * dr;
			if (ff >= 0 && ff < 8 && rr >= 0 && rr < 8)
				alfil_movetable[sq] |= square_bits(Square(rr * 8 + ff));
		}

		Bitboard mask = (rank << (sq & 0b111000)) ^ (file << (sq & 0b111));
		if ((sq & 7) != FILE_A) mask &= ~FileABits;
		if ((sq >> 3) != RANK_1) mask &= ~Rank1Bits;
		if ((sq & 7) != FILE_H) mask &= ~FileHBits;
		if ((sq >> 3) != RANK_8) mask &= ~Rank8Bits;
		rook_magics[sq].mask = mask;
#ifndef USE_PEXT
		rook_magics[sq].magic = rook_magics_src[sq].magic;
#endif
		gen_rook_moves(sq, piece);
	}
}

Bitboard rook_attacks(Square sq, Bitboard occ) {
#ifdef USE_PEXT
	return rook_magics[sq].ptr[arch::pext(occ, rook_magics[sq].mask)];
#else
	uint64_t idx = (occ | rook_magics[sq].mask) * rook_magics[sq].magic;
	return rook_magics[sq].ptr[idx >> (64 - 12)];
#endif
}

Bitboard knight_attacks(Square sq) { return knight_movetable[sq]; }
Bitboard king_attacks(Square sq) { return king_movetable[sq]; }
Bitboard alfil_attacks(Square sq) { return alfil_movetable[sq]; }
Bitboard ferz_attacks(Square sq) { return ferz_movetable[sq]; }

Bitboard pawn_attacks(Square sq, bool color) {
	Bitboard pawn = square_bits(sq);
	if (color == WHITE)
		return ((pawn & ~FileABits) << 7) | ((pawn & ~FileHBits) << 9);
	return ((pawn & ~FileHBits) >> 7) | ((pawn & ~FileABits) >> 9);
}

void white_pawn_moves(const Position &pos, pzstd::vector<Move> &moves) {
	Bitboard pieces = pos.piece_boards[PAWN] & pos.piece_boards[OCC(WHITE)];
	Bitboard occ = pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)];
	Bitboard dsts = (pieces << 8) & ~occ;
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst >= SQ_A8 ? Move::make<PROMOTION>(dst - 8, dst) : Move(dst - 8, dst));
		dsts = arch::blsr(dsts);
	}
	dsts = ((pieces & ~FileABits) << 7) & pos.piece_boards[OCC(BLACK)];
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst >= SQ_A8 ? Move::make<PROMOTION>(dst - 7, dst) : Move(dst - 7, dst));
		dsts = arch::blsr(dsts);
	}
	dsts = ((pieces & ~FileHBits) << 9) & pos.piece_boards[OCC(BLACK)];
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst >= SQ_A8 ? Move::make<PROMOTION>(dst - 9, dst) : Move(dst - 9, dst));
		dsts = arch::blsr(dsts);
	}
}

void black_pawn_moves(const Position &pos, pzstd::vector<Move> &moves) {
	Bitboard pieces = pos.piece_boards[PAWN] & pos.piece_boards[OCC(BLACK)];
	Bitboard occ = pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)];
	Bitboard dsts = (pieces >> 8) & ~occ;
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst <= SQ_H1 ? Move::make<PROMOTION>(dst + 8, dst) : Move(dst + 8, dst));
		dsts = arch::blsr(dsts);
	}
	dsts = ((pieces & ~FileHBits) >> 7) & pos.piece_boards[OCC(WHITE)];
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst <= SQ_H1 ? Move::make<PROMOTION>(dst + 7, dst) : Move(dst + 7, dst));
		dsts = arch::blsr(dsts);
	}
	dsts = ((pieces & ~FileABits) >> 9) & pos.piece_boards[OCC(WHITE)];
	while (dsts) {
		int dst = arch::tzcnt(dsts);
		moves.push_back(dst <= SQ_H1 ? Move::make<PROMOTION>(dst + 9, dst) : Move(dst + 9, dst));
		dsts = arch::blsr(dsts);
	}
}

void pawn_moves(const Position &pos, pzstd::vector<Move> &moves) {
	pos.side == WHITE ? white_pawn_moves(pos, moves) : black_pawn_moves(pos, moves);
}

static void leaper_moves(const Position &pos, pzstd::vector<Move> &moves, PieceType type, Bitboard table[64]) {
	Bitboard pieces = pos.piece_boards[type] & pos.piece_boards[OCC(pos.side)];
	while (pieces) {
		int src = arch::tzcnt(pieces);
		Bitboard dsts = table[src] & ~pos.piece_boards[OCC(pos.side)];
		while (dsts) {
			int dst = arch::tzcnt(dsts);
			moves.push_back(Move(src, dst));
			dsts = arch::blsr(dsts);
		}
		pieces = arch::blsr(pieces);
	}
}

void knight_moves(const Position &pos, pzstd::vector<Move> &moves) { leaper_moves(pos, moves, KNIGHT, knight_movetable); }
void alfil_moves(const Position &pos, pzstd::vector<Move> &moves) { leaper_moves(pos, moves, ALFIL, alfil_movetable); }
void ferz_moves(const Position &pos, pzstd::vector<Move> &moves) { leaper_moves(pos, moves, FERZ, ferz_movetable); }
void king_moves(const Position &pos, pzstd::vector<Move> &moves) { leaper_moves(pos, moves, KING, king_movetable); }

void rook_moves(const Position &pos, pzstd::vector<Move> &moves) {
	Bitboard pieces = pos.piece_boards[ROOK] & pos.piece_boards[OCC(pos.side)];
	Bitboard occ = pos.piece_boards[OCC(WHITE)] | pos.piece_boards[OCC(BLACK)];
	while (pieces) {
		int src = arch::tzcnt(pieces);
		Bitboard dsts = rook_attacks(Square(src), occ) & ~pos.piece_boards[OCC(pos.side)];
		while (dsts) {
			int dst = arch::tzcnt(dsts);
			moves.push_back(Move(src, dst));
			dsts = arch::blsr(dsts);
		}
		pieces = arch::blsr(pieces);
	}
}

void Position::legal_moves(pzstd::vector<Move> &moves) const {
	rook_moves(*this, moves);
	alfil_moves(*this, moves);
	ferz_moves(*this, moves);
	knight_moves(*this, moves);
	pawn_moves(*this, moves);
	king_moves(*this, moves);
}

bool Position::has_legal_move() const {
	pzstd::vector<Move> moves;
	legal_moves(moves);
	for (Move move : moves) if (is_legal(move)) return true;
	return false;
}

void Position::captures(pzstd::vector<Move> &moves) const {
	pzstd::vector<Move> all;
	legal_moves(all);
	for (Move move : all)
		if (is_capture(move) || move.type() == PROMOTION) moves.push_back(move);
}

void Position::update_control() {
	memset(side_control, 0, sizeof(side_control));
	memset(pinned, 0, sizeof(pinned));
	memset(pinners, 0, sizeof(pinners));
	memset(checkers, 0, sizeof(checkers));
	Bitboard occ = piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)];
	Square king_sq[2] = {
		Square(arch::tzcnt(piece_boards[KING] & piece_boards[OCC(WHITE)])),
		Square(arch::tzcnt(piece_boards[KING] & piece_boards[OCC(BLACK)]))
	};
	for (int color = WHITE; color <= BLACK; color++) {
		int enemy = !color;
		Bitboard enemy_king = piece_boards[KING] & piece_boards[OCC(enemy)];
		Bitboard pieces = piece_boards[ROOK] & piece_boards[OCC(color)];
		while (pieces) {
			Square src = Square(arch::tzcnt(pieces));
			Bitboard attacks = rook_attacks(src, occ ^ enemy_king);
			side_control[color] |= attacks;
			if (attacks & enemy_king) checkers[enemy] |= square_bits(src);
			Bitboard between = rook_blockers_pure[src][king_sq[enemy]] & occ;
			if (arch::popcnt(between) == 1 && (between & piece_boards[OCC(enemy)])) {
				pinned[enemy] |= between;
				pinners[enemy] |= square_bits(src);
			}
			pieces = arch::blsr(pieces);
		}

		struct Leaper { PieceType type; Bitboard (*attacks)(Square); } leapers[] = {
			{ALFIL, alfil_attacks}, {FERZ, ferz_attacks}, {KNIGHT, knight_attacks}, {KING, king_attacks}
		};
		for (auto leaper : leapers) {
			pieces = piece_boards[leaper.type] & piece_boards[OCC(color)];
			while (pieces) {
				Square src = Square(arch::tzcnt(pieces));
				Bitboard attacks = leaper.attacks(src);
				side_control[color] |= attacks;
				if (attacks & enemy_king) checkers[enemy] |= square_bits(src);
				pieces = arch::blsr(pieces);
			}
		}
		pieces = piece_boards[PAWN] & piece_boards[OCC(color)];
		while (pieces) {
			Square src = Square(arch::tzcnt(pieces));
			Bitboard attacks = pawn_attacks(src, color);
			side_control[color] |= attacks;
			if (attacks & enemy_king) checkers[enemy] |= square_bits(src);
			pieces = arch::blsr(pieces);
		}
	}
}

bool Position::control(int sq, bool color) const { return side_control[color] & square_bits(Square(sq)); }

Bitboard Position::lva_(Square sq, int color, PieceType &piece, Bitboard occ) const {
	Bitboard ours = piece_boards[OCC(color)] & occ;
	Bitboard attackers = pawn_attacks(sq, !color) & piece_boards[PAWN] & ours;
	if (attackers) { piece = PAWN; return arch::blsi(attackers); }
	attackers = alfil_attacks(sq) & piece_boards[ALFIL] & ours;
	if (attackers) { piece = ALFIL; return arch::blsi(attackers); }
	attackers = ferz_attacks(sq) & piece_boards[FERZ] & ours;
	if (attackers) { piece = FERZ; return arch::blsi(attackers); }
	attackers = knight_attacks(sq) & piece_boards[KNIGHT] & ours;
	if (attackers) { piece = KNIGHT; return arch::blsi(attackers); }
	attackers = rook_attacks(sq, occ) & piece_boards[ROOK] & ours;
	if (attackers) { piece = ROOK; return arch::blsi(attackers); }
	attackers = king_attacks(sq) & piece_boards[KING] & ours;
	if (attackers) { piece = KING; return attackers; }
	piece = NO_PIECETYPE;
	return 0;
}

static int gain(const Position &pos, Move move) {
	int value = PieceValue[pos.mailbox[move.dst()] & 7];
	if (move.type() == PROMOTION) value += PieceValue[FERZ] - PawnValue;
	return value;
}

bool Position::see(Move move, int threshold) {
	Square src = move.src(), dst = move.dst();
	PieceType attacker = PieceType(mailbox[src] & 7);
	int score = gain(*this, move) - threshold;
	if (score < 0) return false;
	PieceType next = move.type() == PROMOTION ? FERZ : attacker;
	score -= PieceValue[next];
	if (score >= 0) return true;
	int color = mailbox[src] >> 3;
	Bitboard occ = (piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)]) ^ square_bits(src);
	color ^= 1;
	while (Bitboard attackers = lva_(dst, color, next, occ)) {
		occ ^= arch::blsi(attackers);
		score = -score - 1 - PieceValue[next];
		color ^= 1;
		if (score >= 0) {
			if (next == KING && lva_(dst, color, next, occ)) color ^= 1;
			break;
		}
	}
	return color != side;
}

bool Position::is_pseudolegal(Move move) const {
	if (move == NullMove || move.src() >= SQ_NONE || move.dst() >= SQ_NONE) return false;
	if ((mailbox[move.src()] >> 3) != side) return false;
	if (piece_boards[OCC(side)] & square_bits(move.dst())) return false;
	PieceType piece = PieceType(mailbox[move.src()] & 7);
	if (move.type() == PROMOTION && piece != PAWN) return false;
	switch (piece) {
	case FERZ: return ferz_attacks(move.src()) & square_bits(move.dst());
	case ROOK: return !(rook_blockers[move.src()][move.dst()] & (piece_boards[OCC(WHITE)] | piece_boards[OCC(BLACK)]));
	case ALFIL: return alfil_attacks(move.src()) & square_bits(move.dst());
	case KNIGHT: return knight_attacks(move.src()) & square_bits(move.dst());
	case KING: return king_attacks(move.src()) & square_bits(move.dst());
	case PAWN:
		if (move.type() == PROMOTION) {
			if ((side == WHITE && move.dst() < SQ_A8) || (side == BLACK && move.dst() > SQ_H1)) return false;
		} else if ((side == WHITE && move.dst() >= SQ_A8) || (side == BLACK && move.dst() <= SQ_H1)) return false;
		if (side == WHITE) {
			if (move.dst() - move.src() == 8) return mailbox[move.dst()] == NO_PIECE;
			if ((move.src() & 7) != FILE_A && move.dst() - move.src() == 7) return piece_boards[OCC(BLACK)] & square_bits(move.dst());
			if ((move.src() & 7) != FILE_H && move.dst() - move.src() == 9) return piece_boards[OCC(BLACK)] & square_bits(move.dst());
		} else {
			if (move.src() - move.dst() == 8) return mailbox[move.dst()] == NO_PIECE;
			if ((move.src() & 7) != FILE_A && move.src() - move.dst() == 9) return piece_boards[OCC(WHITE)] & square_bits(move.dst());
			if ((move.src() & 7) != FILE_H && move.src() - move.dst() == 7) return piece_boards[OCC(WHITE)] & square_bits(move.dst());
		}
		return false;
	default: return false;
	}
}

bool Position::is_legal(Move move) const {
	Square king_sq = Square(arch::tzcnt(piece_boards[KING] & piece_boards[OCC(side)]));
	if ((mailbox[move.src()] & 7) == KING)
		return !(side_control[!side] & square_bits(move.dst()));
	if (arch::popcnt(checkers[side]) > 1) return false;
	if (checkers[side]) {
		Square checker = Square(arch::tzcnt(checkers[side]));
		Bitboard between = (piece_boards[ROOK] & square_bits(checker)) ? rook_blockers_pure[checker][king_sq] : 0;
		if (!(between & square_bits(move.dst())) && move.dst() != checker) return false;
	}
	if (pinned[side] & square_bits(move.src())) {
		Bitboard p = pinners[side];
		while (p) {
			Square pinner = Square(arch::tzcnt(p));
			Bitboard ray = arch::blsi(p) | rook_blockers_pure[pinner][king_sq];
			if (square_bits(move.src()) & ray) return square_bits(move.dst()) & ray;
			p = arch::blsr(p);
		}
	}
	return true;
}
