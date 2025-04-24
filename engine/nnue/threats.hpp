#pragma once

#include <cstdint>
#include <optional>

#include "../includes.hpp"
#include "../bitboard.hpp"
#include "misc.hpp"

// Threat Inputs: Combination of the position of pieces and all
// active piece-to-piece attacks. Position mirrored such that king is always on abcd files.
class Full_Threats {

    
    int threatoffsets[16][66];
    void init_threat_offsets();
   public:

    // Number of feature dimensions
    static constexpr int InputSize = 80624;

    // clang-format off
    // Orient a square according to perspective (rotates by 180 for black)
    static constexpr int OrientTBL[2][64] = {
      { SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1,
        SQ_A1, SQ_A1, SQ_A1, SQ_A1, SQ_H1, SQ_H1, SQ_H1, SQ_H1 },
      { SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8,
        SQ_A8, SQ_A8, SQ_A8, SQ_A8, SQ_H8, SQ_H8, SQ_H8, SQ_H8 }
    };
    static constexpr int numvalidtargets[6] = {6, 12, 10, 10, 12, 8};
    static constexpr int map[6][6] = {
      {0, 1, -1, 2, -1, -1},
      {0, 1, 2, 3, 4, 5},
      {0, 1, 2, 3, -1, 4},
      {0, 1, 2, 3, -1, 4},
      {0, 1, 2, 3, 4, 5},
      {0, 1, 2, 3, -1, -1}
    };
    // clang-format on


    Full_Threats() { init_threat_offsets(); };

    // Index of a feature for a given king position and another piece on some square
    template<bool Perspective> int make_psq_index(Piece pc, Square sq, Square ksq);
    
    template<bool Perspective> std::optional<int> make_threat_index(Piece attkr, Square from, Square to, Piece attkd, Square ksq);

    // Get a list of indices for active features
    template<bool Perspective> void append_active_threats(const Bitboard *pieceBB, const Piece *mailbox, IndexList<96>& active);

    template<bool Perspective> void append_active_psq(const Board& board, IndexList<32>& active);

    template<bool Perspective> void append_active_features(const Bitboard *pieceBB, const Piece *mailbox, IndexList<32>& psq, IndexList<96>& threats);
};
