// Move generator test suite.
//
//   ./perft_test          quick run (a few seconds in a Release build)
//   ./perft_test --deep   deeper counts (a minute or more)
//
// The reference counts are the standard positions from the Chess
// Programming Wiki "Perft Results" page. Each position targets bugs the
// start position never reaches: castling through check, en passant
// discovered checks, promotions with capture, and so on.

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "movegen.h"
#include "position.h"

namespace {

struct PerftCase {
    const char* name;
    const char* fen;
    std::vector<std::uint64_t> counts;  // counts[d - 1] = perft(d)
    int quick_depth;                    // depth used without --deep
};

const std::vector<PerftCase> Cases = {
    {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {20, 400, 8902, 197281, 4865609, 119060324}, 5},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {48, 2039, 97862, 4085603, 193690690}, 4},
    {"position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {14, 191, 2812, 43238, 674624, 11030083, 178633661}, 5},
    {"position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {6, 264, 9467, 422333, 15833292}, 4},
    {"position 4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
     {6, 264, 9467, 422333, 15833292}, 4},
    {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {44, 1486, 62379, 2103487, 89941194}, 4},
    {"position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {46, 2079, 89890, 3894594, 164075551}, 4},
};

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

// Walk the tree and, at every node, check that the incrementally updated
// hash matches one computed from scratch, and that unmake restores the
// position exactly. Perft counts alone can't catch hash bugs.
void walk_consistency(Position& pos, int depth, int& errors) {
    if (pos.key() != pos.compute_key())
        ++errors;
    if (depth == 0)
        return;
    MoveList list;
    generate_legal(pos, list);
    for (Move m : list) {
        const std::string before = pos.fen();
        const Key key_before = pos.key();
        pos.make_move(m);
        walk_consistency(pos, depth - 1, errors);
        pos.unmake_move(m);
        if (pos.fen() != before || pos.key() != key_before)
            ++errors;
    }
}

void test_magics() {
    // Compare magic lookups against the slow ray walk on random occupancies.
    std::uint64_t s = 0x9E3779B97F4A7C15ULL;
    int bad = 0;
    for (int i = 0; i < 20000; ++i) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        Bitboard occ = s & (s >> 3);  // roughly a quarter of squares filled
        Square sq = Square(i & 63);
        bad += bishop_attacks(sq, occ) != slow_bishop_attacks(sq, occ);
        bad += rook_attacks(sq, occ) != slow_rook_attacks(sq, occ);
    }
    check(bad == 0, "magic lookups disagree with ray walk (" + std::to_string(bad) + ")");
}

void test_fen_round_trip() {
    Position pos;
    for (const auto& c : Cases) {
        check(pos.set_fen(c.fen), std::string("set_fen failed: ") + c.fen);
        check(pos.fen() == c.fen, std::string("FEN round trip: ") + c.fen + " -> " + pos.fen());
    }
    // Malformed or illegal positions must be rejected.
    for (const char* bad : {
             "", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",          // 7 ranks
             "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",     // 9 files
             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w kq - 0 1",       // no white king
             "4k3/8/8/8/8/8/8/4K2R x K - 0 1",                               // bad side
             "4k3/8/8/8/8/8/8/R3K2R w KX - 0 1",                             // bad castling
             "4k3/8/8/8/8/8/8/4K2P w - - 0 1",                               // pawn on rank 1
             "4k3/8/8/8/8/8/8/4R1K1 w - - 0 1",                              // black in check, white to move
         })
        check(!pos.set_fen(bad), std::string("accepted bad FEN: ") + bad);
}

}  // namespace

int main(int argc, char* argv[]) {
    const bool deep = argc > 1 && std::strcmp(argv[1], "--deep") == 0;

    init_bitboards();
    Position::init();

    std::cout << "magic tables\n";
    test_magics();
    std::cout << "FEN parsing\n";
    test_fen_round_trip();

    Position pos;
    std::uint64_t total_nodes = 0;
    auto start = std::chrono::steady_clock::now();

    for (const auto& c : Cases) {
        pos.set_fen(c.fen);

        int errors = 0;
        walk_consistency(pos, 3, errors);
        check(errors == 0, std::string(c.name) + ": hash/unmake mismatches: " + std::to_string(errors));

        int max_depth = deep ? int(c.counts.size()) : c.quick_depth;
        std::cout << c.name << "\n";
        for (int d = 1; d <= max_depth; ++d) {
            std::uint64_t got = perft(pos, d);
            std::uint64_t want = c.counts[d - 1];
            total_nodes += got;
            std::cout << "  depth " << d << ": " << got << (got == want ? "" : "  <-- MISMATCH") << "\n";
            check(got == want, std::string(c.name) + " depth " + std::to_string(d) + ": expected " +
                                   std::to_string(want));
            if (got != want)
                break;
        }
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    std::cout << "\n" << total_nodes << " nodes in " << ms << " ms";
    if (ms > 0)
        std::cout << " (" << total_nodes / std::uint64_t(ms) / 1000 << " Mnps)";
    std::cout << "\n";

    if (failures) {
        std::cout << failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}
