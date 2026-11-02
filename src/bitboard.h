#pragma once

// Bitboard helpers and precomputed attack tables.
//
// Leapers (pawn captures, knights, kings) do not depend on other pieces, so
// their attacks are a plain 64-entry lookup.
//
// Sliders (bishops, rooks, queens) stop at the first blocker, so their attacks
// depend on the occupancy along their rays. We use "fancy" magic bitboards:
//   1. Keep only the occupancy bits that can matter (the "relevant mask": the
//      ray squares, minus the board edge, since a blocker on the last square
//      of a ray changes nothing).
//   2. Multiply by a "magic" constant and shift right. A good magic maps every
//      possible relevant occupancy to an index with no harmful collisions.
//   3. Look the attack set up in a table filled at startup.
// Magics are searched for at startup with a fixed random seed (see
// bitboard.cpp), so there is no table of unexplained constants to trust.

#include <bit>
#include <cassert>
#include <string>

#include "types.h"

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = FILE_A_BB << 1;
constexpr Bitboard FILE_G_BB = FILE_A_BB << 6;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;

constexpr Bitboard RANK_1_BB = 0xFFULL;
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_3_BB = RANK_1_BB << 16;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_6_BB = RANK_1_BB << 40;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

constexpr Bitboard square_bb(Square s) { return 1ULL << s; }
constexpr Bitboard file_bb(Square s) { return FILE_A_BB << file_of(s); }
constexpr Bitboard rank_bb(Square s) { return RANK_1_BB << (8 * rank_of(s)); }

inline int popcount(Bitboard b) { return std::popcount(b); }

inline Square lsb(Bitboard b) {
    assert(b);
    return Square(std::countr_zero(b));
}

// Return the lowest set square and clear it. b & (b - 1) clears the lowest bit.
inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Shift a whole bitboard one step in direction D (in square-index units).
// The file masks stop pieces on the a- or h-file from wrapping round to the
// other side of the board: without them, h4 shifted "east" lands on a5.
template <int D>
constexpr Bitboard shift(Bitboard b) {
    if constexpr (D == 8)
        return b << 8;
    else if constexpr (D == -8)
        return b >> 8;
    else if constexpr (D == 1)
        return (b & ~FILE_H_BB) << 1;
    else if constexpr (D == -1)
        return (b & ~FILE_A_BB) >> 1;
    else if constexpr (D == 9)
        return (b & ~FILE_H_BB) << 9;
    else if constexpr (D == 7)
        return (b & ~FILE_A_BB) << 7;
    else if constexpr (D == -7)
        return (b & ~FILE_H_BB) >> 7;
    else if constexpr (D == -9)
        return (b & ~FILE_A_BB) >> 9;
    else
        static_assert(D == 8, "unsupported shift direction");
}

struct Magic {
    Bitboard mask = 0;        // relevant occupancy squares
    Bitboard magic = 0;       // the multiplier
    Bitboard* attacks = nullptr;  // this square's slice of the shared table
    unsigned shift = 0;       // 64 - popcount(mask)

    unsigned index(Bitboard occupied) const {
        return unsigned(((occupied & mask) * magic) >> shift);
    }
};

extern Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
extern Bitboard KnightAttacks[SQUARE_NB];
extern Bitboard KingAttacks[SQUARE_NB];
extern Magic BishopMagics[SQUARE_NB];
extern Magic RookMagics[SQUARE_NB];

// Must be called once before anything else uses the tables.
void init_bitboards();

inline Bitboard bishop_attacks(Square s, Bitboard occupied) {
    const Magic& m = BishopMagics[s];
    return m.attacks[m.index(occupied)];
}

inline Bitboard rook_attacks(Square s, Bitboard occupied) {
    const Magic& m = RookMagics[s];
    return m.attacks[m.index(occupied)];
}

inline Bitboard queen_attacks(Square s, Bitboard occupied) {
    return bishop_attacks(s, occupied) | rook_attacks(s, occupied);
}

// Slow ray-walking reference, used to build the magic tables and in tests.
Bitboard slow_bishop_attacks(Square s, Bitboard occupied);
Bitboard slow_rook_attacks(Square s, Bitboard occupied);

// 8x8 picture of a bitboard, rank 8 at the top, for debugging.
std::string bitboard_to_string(Bitboard b);
