/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//Definition of input features Full_Threats of NNUE evaluation function

#include "threats.hpp"
#include "../bitboard.hpp"
#include "../includes.hpp"

//lookup array for indexing threats
void Full_Threats::init_threat_offsets() {
    int pieceoffset = 0;
    PieceType piecetbl[16] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECETYPE, NO_PIECETYPE,
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECETYPE, NO_PIECETYPE};
    for (int piece = 0; piece < 16; piece++) {
        if (piecetbl[piece] != NO_PIECETYPE) {
            threatoffsets[piece][65] = pieceoffset;
            int squareoffset = 0;
            for (int from = SQ_A1; from <= SQ_H8; from++) {
                threatoffsets[piece][from] = squareoffset;
                if (piecetbl[piece] != PAWN) {
                    Bitboard attacks = attacks_bb(piecetbl[piece], Square(from), 0ULL);
                    squareoffset += _mm_popcnt_u64(attacks);
                }
                else if (from >= SQ_A2 && from <= SQ_H7) {
                    Bitboard attacks = pawn_attacks_bb(piece/8, Square(from));
                    squareoffset += _mm_popcnt_u64(attacks);
                }
            }
            threatoffsets[piece][64] = squareoffset;
            pieceoffset += numvalidtargets[piecetbl[piece]]*squareoffset;
        }
    }
}
template<bool Perspective> int Full_Threats::make_psq_index(Piece pc, Square sq, Square ksq) {
    return 79856 + (color_of(pc) != Perspective) * 64 * 6 + (pc & 7) * 64 + (int(sq) ^ OrientTBL[Perspective][ksq]);
}
// Index of a feature for a given king position and another piece on some square
template<bool Perspective> std::optional<int> Full_Threats::make_threat_index(Piece attkr, Square from, Square to, Piece attkd, Square ksq) {
    bool enemy = ((attkr ^ attkd) & 8);
    from = (Square)(int(from) ^ OrientTBL[Perspective][ksq]);
    to = (Square)(int(to) ^ OrientTBL[Perspective][ksq]);
    if (Perspective == BLACK) {
        attkr = Piece((int)attkr^8);
        attkd = Piece((int)attkd^8);
    }
    if ((map[type_of(attkr)][type_of(attkd)] < 0) || (type_of(attkr) == type_of(attkd) && (enemy || type_of(attkr) != PAWN) && from < to)) {
        return std::nullopt;
    }
    Bitboard attacks = (type_of(attkr) == PAWN) ? pawn_attacks_bb(color_of(attkr), from) : attacks_bb(type_of(attkr), from, 0ULL);
    return threatoffsets[attkr][65] + 
        ((attkd/8)*(numvalidtargets[type_of(attkr)]/2)+map[type_of(attkr)][type_of(attkd)])*threatoffsets[attkr][64]
    + threatoffsets[attkr][from] + _mm_popcnt_u64((square_bits(to)-1) & attacks);
}

// Get a list of indices for active features in ascending order
template<bool Perspective> void Full_Threats::append_active_threats(const Bitboard *pieceBB, const Piece *mailbox, IndexList<96>& active) {
    Square ksq = Square(_tzcnt_u64(pieceBB[6+Perspective] & pieceBB[KING]));
    bool order[2][2] = {{WHITE, BLACK}, {BLACK, WHITE}};
    Bitboard occupied = pieceBB[6+WHITE] | pieceBB[6+BLACK];
    IndexList<16> indices;

    for (int i = WHITE; i <= BLACK; i++) {
        for (int j = PAWN; j <= KING; j++) {
            bool c = order[Perspective][i];
            PieceType pt = PieceType(j);
            Piece attkr = make_piece(c, pt);
            Bitboard bb  = pieceBB[6+c] & pieceBB[pt];
            indices.clear();
            if (pt == PAWN) {
                auto right = (c == WHITE) ? 9 : -9;
                auto left = (c == WHITE) ? 7 : -7;
                auto attacks_left = ((c == WHITE) ? shift<9>(bb) : shift<-9>(bb)) & occupied;
                auto attacks_right = ((c == WHITE) ? shift<7>(bb) : shift<-7>(bb)) & occupied;
                while (attacks_left) {
                    Square to = pop_lsb(attacks_left);
                    Square from = Square(to - right);
                    Piece attkd = mailbox[to];
                    std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                    if (index.has_value()) {
                        indices.push_back(index.value());
                    }
                }
                while (attacks_right) {
                    Square to = pop_lsb(attacks_right);
                    Square from = Square(to - left);
                    Piece attkd = mailbox[to];
                    std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                    if (index.has_value()) {
                        indices.push_back(index.value());
                    }
                }
            }
            else {
                while (bb)
                {
                    Square from = pop_lsb(bb);
                    Bitboard attacks = (attacks_bb(pt, from, occupied)) & occupied;
                    while (attacks) {
                        Square to = pop_lsb(attacks);
                        Piece attkd = mailbox[to];
                        std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                        if (index.has_value()) {
                            indices.push_back(index.value());
                        }   
                    }
                }
            }
            std::sort(indices.begin(), indices.end());
            for (auto threat : indices) {
                active.push_back(threat);
            }
        }
    }
}

template<bool Perspective> void Full_Threats::append_active_psq(const Board& board, IndexList<32>& active) {
    Square   ksq = Square(_tzcnt_u64(board.piece_boards[6+Perspective] & board.piece_boards[KING]));
    Bitboard bb  = (board.piece_boards[6+WHITE] | board.piece_boards[6+BLACK]);
    while (bb)
    {
        Square s = pop_lsb(bb);
        Piece pc = board.mailbox[s];
        active.push_back(make_psq_index<Perspective>(pc, s, ksq));
    }
}

template<bool Perspective> void Full_Threats::append_active_features(const Bitboard *pieceBB, const Piece *mailbox, IndexList<32>& psq, IndexList<96>& threats) {
    Square ksq = Square(_tzcnt_u64(pieceBB[6+Perspective] & pieceBB[KING]));
    bool order[2][2] = {{WHITE, BLACK}, {BLACK, WHITE}};
    Bitboard occupied = pieceBB[6+WHITE] | pieceBB[6+BLACK];
    IndexList<16> indices;
    for (int i = WHITE; i <= BLACK; i++) {
        for (int j = PAWN; j <= KING; j++) {
            bool c = order[Perspective][i];
            PieceType pt = PieceType(j);
            Piece attkr = make_piece(c, pt);
            Bitboard bb  = pieceBB[6+c] & pieceBB[pt];
            indices.clear();
            if (pt == PAWN) {
                int right = (c == WHITE) ? 9 : -9;
                int left = (c == WHITE) ? 7 : -7;
                Bitboard attacks_left = ((c == WHITE) ? shift<9>(bb) : shift<-9>(bb)) & occupied;
                Bitboard attacks_right = ((c == WHITE) ? shift<7>(bb) : shift<-7>(bb)) & occupied;
                while (attacks_left) {
                    Square to = pop_lsb(attacks_left);
                    Square from = Square(to - right);
                    Piece attkd = mailbox[to];
                    std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                    if (index.has_value()) {
                        indices.push_back(index.value());
                    }
                }
                while (attacks_right) {
                    Square to = pop_lsb(attacks_right);
                    Square from = Square(to - left);
                    Piece attkd = mailbox[to];
                    std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                    if (index.has_value()) {
                        indices.push_back(index.value());
                    }
                }
                while (bb) {
                    Square from = pop_lsb(bb);
                    psq.push_back(make_psq_index<Perspective>(attkr, from, ksq));
                }
            }
            else {
                while (bb)
                {
                    Square from = pop_lsb(bb);
                    psq.push_back(make_psq_index<Perspective>(attkr, from, ksq));
                    Bitboard attacks = (attacks_bb(pt, from, occupied)) & occupied;
                    while (attacks) {
                        Square to = pop_lsb(attacks);
                        Piece attkd = mailbox[to];
                        std::optional<int> index = make_threat_index<Perspective>(attkr, from, to, attkd, ksq);
                        if (index.has_value()) {
                            indices.push_back(index.value());
                        }
                    }
                }
            }
            std::sort(indices.begin(), indices.end());
            for (auto threat : indices) {
                threats.push_back(threat);
            }
        }
    }
}

// Explicit template instantiations
template void Full_Threats::append_active_threats<WHITE>(const Bitboard *pieceBB, const Piece *mailbox, IndexList<96>& active);
template void Full_Threats::append_active_threats<BLACK>(const Bitboard *pieceBB, const Piece *mailbox, IndexList<96>& active);
template void Full_Threats::append_active_psq<WHITE>(const Board& board, IndexList<32>& active);
template void Full_Threats::append_active_psq<BLACK>(const Board& board, IndexList<32>& active);
template void Full_Threats::append_active_features<WHITE>(const Bitboard *pieceBB, const Piece *mailbox, IndexList<32>& psq, IndexList<96>& active);
template void Full_Threats::append_active_features<BLACK>(const Bitboard *pieceBB, const Piece *mailbox, IndexList<32>& psq, IndexList<96>& active);
template int Full_Threats::make_psq_index<WHITE>(Piece pc, Square sq, Square ksq);
template int Full_Threats::make_psq_index<BLACK>(Piece pc, Square sq, Square ksq);
template std::optional<int> Full_Threats::make_threat_index<WHITE>(Piece attkr, Square from, Square to, Piece attkd, Square ksq);
template std::optional<int> Full_Threats::make_threat_index<BLACK>(Piece attkr, Square from, Square to, Piece attkd, Square ksq);

