#include "bitboard.h"

#include <array>

#include "prng.h"

Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
Bitboard KnightAttacks[SQUARE_NB];
Bitboard KingAttacks[SQUARE_NB];
Magic BishopMagics[SQUARE_NB];
Magic RookMagics[SQUARE_NB];

namespace {

// Total table sizes: the sum over all squares of 2^(relevant bits).
// Rooks have 10-12 relevant bits per square, bishops 5-9.
Bitboard RookTable[102400];
Bitboard BishopTable[5248];

constexpr int RookDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int BishopDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

// Walk each ray square by square until the edge or the first blocker
// (the blocker itself is included: it may be an enemy piece to capture).
Bitboard slide(const int dirs[4][2], Square s, Bitboard occupied) {
    Bitboard attacks = 0;
    for (int d = 0; d < 4; ++d) {
        int f = file_of(s) + dirs[d][0];
        int r = rank_of(s) + dirs[d][1];
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            Bitboard b = square_bb(make_square(f, r));
            attacks |= b;
            if (occupied & b)
                break;
            f += dirs[d][0];
            r += dirs[d][1];
        }
    }
    return attacks;
}

void init_magics(Magic magics[], Bitboard table[], const int dirs[4][2], PRNG& rng) {
    // Scratch space for one square: every subset of its mask, and the true
    // attack set for that subset. 4096 = 2^12, the most relevant bits any
    // rook square has.
    static std::array<Bitboard, 4096> occupancy, reference;
    static std::array<int, 4096> epoch{};  // which attempt last wrote each slot
    int attempt = 0;
    Bitboard* next_slice = table;

    for (int sq = A1; sq <= H8; ++sq) {
        Square s = Square(sq);
        Magic& m = magics[s];

        // Edge squares on the far side of a ray never block anything further.
        // Exclude the board edges, except the rank/file the piece is on.
        Bitboard edges = ((RANK_1_BB | RANK_8_BB) & ~rank_bb(s)) |
                         ((FILE_A_BB | FILE_H_BB) & ~file_bb(s));
        m.mask = slide(dirs, s, 0) & ~edges;
        m.shift = unsigned(64 - popcount(m.mask));
        m.attacks = next_slice;

        // Enumerate every subset of the mask ("Carry-Rippler" trick):
        // b = (b - mask) & mask steps through all subsets and returns to 0.
        int size = 0;
        Bitboard b = 0;
        do {
            occupancy[size] = b;
            reference[size] = slide(dirs, s, b);
            ++size;
            b = (b - m.mask) & m.mask;
        } while (b);
        next_slice += size;

        // Try random sparse candidates until one maps every subset without a
        // harmful collision. Two subsets may share an index only if they give
        // the same attack set (a "constructive" collision).
        for (;;) {
            m.magic = rng.sparse();
            // Cheap filter: a good magic spreads the mask's bits into the
            // top byte of the product. Skip candidates that clearly don't.
            if (popcount((m.mask * m.magic) >> 56) < 6)
                continue;

            ++attempt;
            bool ok = true;
            for (int i = 0; i < size; ++i) {
                unsigned idx = m.index(occupancy[i]);
                if (epoch[idx] < attempt) {
                    epoch[idx] = attempt;
                    m.attacks[idx] = reference[i];
                } else if (m.attacks[idx] != reference[i]) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                break;
        }
    }
}

}  // namespace

Bitboard slow_bishop_attacks(Square s, Bitboard occupied) { return slide(BishopDirs, s, occupied); }
Bitboard slow_rook_attacks(Square s, Bitboard occupied) { return slide(RookDirs, s, occupied); }

void init_bitboards() {
    for (int sq = A1; sq <= H8; ++sq) {
        Square s = Square(sq);
        Bitboard b = square_bb(s);

        PawnAttacks[WHITE][s] = shift<7>(b) | shift<9>(b);
        PawnAttacks[BLACK][s] = shift<-7>(b) | shift<-9>(b);

        // Knight jumps, with masks so jumps near the edge don't wrap round.
        KnightAttacks[s] = ((b << 17) & ~FILE_A_BB) | ((b << 15) & ~FILE_H_BB) |
                           ((b << 10) & ~(FILE_A_BB | FILE_B_BB)) |
                           ((b << 6) & ~(FILE_G_BB | FILE_H_BB)) |
                           ((b >> 17) & ~FILE_H_BB) | ((b >> 15) & ~FILE_A_BB) |
                           ((b >> 10) & ~(FILE_G_BB | FILE_H_BB)) |
                           ((b >> 6) & ~(FILE_A_BB | FILE_B_BB));

        KingAttacks[s] = shift<8>(b) | shift<-8>(b) | shift<1>(b) | shift<-1>(b) |
                         shift<9>(b) | shift<7>(b) | shift<-7>(b) | shift<-9>(b);
    }

    // Fixed seed: the same magics every run, so behaviour is reproducible.
    PRNG rng(0x5EED1234ABCDULL);
    init_magics(RookMagics, RookTable, RookDirs, rng);
    init_magics(BishopMagics, BishopTable, BishopDirs, rng);
}

std::string bitboard_to_string(Bitboard b) {
    std::string s;
    for (int r = 7; r >= 0; --r) {
        s += char('1' + r);
        s += ' ';
        for (int f = 0; f < 8; ++f) {
            s += (b & square_bb(make_square(f, r))) ? 'X' : '.';
            s += ' ';
        }
        s += '\n';
    }
    s += "  a b c d e f g h\n";
    return s;
}
