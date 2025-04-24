#include "../includes.hpp"
#include "../bitboard.hpp"
#include "../movegen.hpp"

template<std::size_t MaxSize>
class IndexList {

   public:
    std::size_t size() const { return size_; }
    void        push_back(const T& value) { values_[size_++] = value; }
    void        clear() { size_ = 0ULL; }
    int*          begin() { return values_; }
    int*          end() { return values_ + size_; }
    const int&    operator[](int index) const { return values_[index]; }

   private:
    int           values_[MaxSize];
    std::size_t size_;
};

PieceType type_of(Piece pc) {
    return PieceType(pc & 7);
}

bool color_of(Piece pc) {
    return (pc ^ 8);
}

Piece make_piece(bool c, PieceType pt) {
    return Piece(8*c + pt);
}

template<int D>
constexpr Bitboard shift(Bitboard b) {
    return D == 9     ? (b & ~FileHBits) << 9
         : D == 7     ? (b & ~FileABits) << 7
         : D == -7    ? (b & ~FileHBits) >> 7
         : D == -9    ? (b & ~FileABits) >> 9
                              : 0;
}

inline Square pop_lsb(Bitboard& b) {
    const Square s = Square(_tzcnt_u64(b));
    b &= b - 1;
    return s;
}

Bitboard attacks_bb(PieceType pt, Square sq, Bitboard occ) {
    switch(pt) {
        case KNIGHT:
            return knight_attacks(sq);
        case BISHOP:
            return bishop_attacks(sq, occ);
        case ROOK:
            return rook_attacks(sq, occ);
        case QUEEN:
            return queen_attacks(sq, occ);
        case KING:
            return king_attacks(sq);
        default:
            return 0;
    }
}

Bitboard pawn_attacks_bb(bool color, Square sq) {
    return pawn_attacks(sq, color);
}

void write_difference(const IndexList<96>& a1, const IndexList<96>& b1, IndexList<96>& a2, IndexList<96>& b2) {
    unsigned long long a = 0;
    unsigned long long b = 0;
    while (a < a1.size() && b < b1.size()) {
        if (a1[a] < b1[b]) {
            a2.push_back(a1[a]);
            a++;
        }
        else if (b1[b] < a1[a]) {
            b2.push_back(b1[b]);
            b++;
        }
        else {
            a++;
            b++;
        }
    }
    while (a < a1.size()) {
        a2.push_back(a1[a]);
        a++;
    }
    while (b < b1.size()) {
        b2.push_back(b1[b]);
        b++;
    }
}