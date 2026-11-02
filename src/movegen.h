#pragma once

#include <cstdint>
#include <string>

#include "position.h"
#include "types.h"

// Pseudo-legal moves obey how each piece moves but may leave our own king in
// check. Castling is the exception: its "not in, through or into check"
// conditions are checked here, because they are not about the final position.
void generate_pseudo_legal(const Position& pos, MoveList& list);

// Fully legal moves. Implemented as: generate pseudo-legal, then make each
// move and drop it if our king is attacked. Simple and hard to get wrong;
// a faster pin-aware generator can replace it later, with perft as the check.
void generate_legal(Position& pos, MoveList& list);

// Find the legal move written in UCI notation (e.g. "e2e4", "e7e8q"), or a
// null Move if there is none.
Move parse_uci_move(Position& pos, const std::string& text);

// Count leaf nodes of the legal move tree to `depth` plies.
std::uint64_t perft(Position& pos, int depth);
