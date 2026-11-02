// A small text interface for Phase 1. The commands follow UCI syntax where
// UCI has one, so Phase 2 can grow this loop into a full UCI handler.
//
//   position startpos [moves e2e4 e7e5 ...]
//   position fen <fen> [moves ...]
//   go perft <depth>     per-move breakdown ("divide"), total and speed
//   d                    print the board, FEN and hash key
//   quit
//
// It also runs one command from the shell:  ./engine perft 5 [fen]

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"
#include "movegen.h"
#include "position.h"

namespace {

// Print the node count under each root move. When a count disagrees with a
// reference engine, play that move and divide again one ply shallower: in a
// few steps you reach the exact position where your generator is wrong.
void divide(Position& pos, int depth) {
    auto start = std::chrono::steady_clock::now();
    std::uint64_t total = 0;

    MoveList list;
    generate_legal(pos, list);
    for (Move m : list) {
        pos.make_move(m);
        std::uint64_t n = depth > 1 ? perft(pos, depth - 1) : 1;
        pos.unmake_move(m);
        total += n;
        std::cout << m.to_uci() << ": " << n << "\n";
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    std::cout << "\nNodes searched: " << total << "\n";
    std::cout << "Time: " << ms << " ms";
    if (ms > 0)
        std::cout << " (" << total / std::uint64_t(ms) / 1000 << " Mnps)";
    std::cout << "\n" << std::endl;
}

void cmd_position(Position& pos, std::istringstream& in) {
    std::string token, fen;
    in >> token;
    if (token == "startpos") {
        fen = Position::StartFEN;
        in >> token;  // "moves", if present
    } else if (token == "fen") {
        while (in >> token && token != "moves")
            fen += token + " ";
    } else {
        std::cout << "info string expected 'startpos' or 'fen'\n";
        return;
    }

    if (!pos.set_fen(fen)) {
        std::cout << "info string invalid FEN, resetting to start position\n";
        pos.set_fen(Position::StartFEN);
        return;
    }

    while (in >> token) {
        Move m = parse_uci_move(pos, token);
        if (m.is_null()) {
            std::cout << "info string illegal move '" << token << "', ignoring the rest\n";
            return;
        }
        pos.make_move(m);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    init_bitboards();
    Position::init();

    Position pos;
    pos.set_fen(Position::StartFEN);

    if (argc >= 3 && std::string(argv[1]) == "perft") {
        if (argc >= 4) {
            std::string fen;
            for (int i = 3; i < argc; ++i)
                fen += std::string(argv[i]) + " ";
            if (!pos.set_fen(fen)) {
                std::cerr << "invalid FEN\n";
                return 1;
            }
        }
        divide(pos, std::stoi(argv[2]));
        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "quit") {
            break;
        } else if (cmd == "position") {
            cmd_position(pos, in);
        } else if (cmd == "go") {
            std::string sub;
            int depth = 0;
            if (in >> sub >> depth && sub == "perft" && depth > 0)
                divide(pos, depth);
            else
                std::cout << "info string usage: go perft <depth>\n";
        } else if (cmd == "d") {
            std::cout << pos.to_string() << std::endl;
        } else if (!cmd.empty()) {
            std::cout << "info string unknown command '" << cmd << "'\n";
        }
    }
    return 0;
}
