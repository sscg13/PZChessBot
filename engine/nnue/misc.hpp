#pragma once

#include "../includes.hpp"
#include "../bitboard.hpp"
#include "../movegen.hpp"

template<std::size_t MaxSize>
class IndexList {

   public:
    std::size_t size() const { return size_; }
    void        push_back(const int value) { values_[size_++] = value; }
    void        clear() { size_ = 0ULL; }
    int*          begin() { return values_; }
    int*          end() { return values_ + size_; }
    const int&    operator[](int index) const { return values_[index]; }

   private:
    int           values_[MaxSize];
    std::size_t size_;
};

PieceType type_of(Piece pc);

bool color_of(Piece pc);

Piece make_piece(bool c, PieceType pt);

template<int D> Bitboard shift(Bitboard b);
Square pop_lsb(Bitboard& b);

Bitboard attacks_bb(PieceType pt, Square sq, Bitboard occ);

Bitboard pawn_attacks_bb(bool color, Square sq);

void write_difference(const IndexList<96>& a1, const IndexList<96>& b1, IndexList<96>& a2, IndexList<96>& b2);