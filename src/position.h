#pragma once

#include <string>
#include <vector>

#include "bitboard.h"
#include "types.h"

// Everything make_move changes that unmake_move cannot recompute from the
// move alone. One of these is pushed per move and popped on unmake.
struct StateInfo {
    int castling = NO_CASTLING;
    Square ep_square = NO_SQUARE;
    int rule50 = 0;               // half-moves since the last capture or pawn move
    Piece captured = NO_PIECE;    // what the move that led here captured
    Key key = 0;                  // Zobrist hash of the position
};

class Position {
public:
    static constexpr const char* StartFEN =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // Fill the Zobrist key tables. Call once at startup, after init_bitboards().
    static void init();

    // An empty board. Call set_fen() before using it. (The constructor
    // deliberately doesn't parse StartFEN itself: that needs the attack
    // tables, and a global Position would be built before main() runs init.)
    Position() { states_.emplace_back(); }

    // Returns false (and leaves the position unspecified) if the FEN is
    // malformed or describes an illegal position.
    bool set_fen(const std::string& fen);
    std::string fen() const;
    std::string to_string() const;

    // The move must be pseudo-legal in this position. make_move does not
    // check legality: the move generator filters out moves that leave the
    // king in check.
    void make_move(Move m);
    void unmake_move(Move m);

    Color side_to_move() const { return side_to_move_; }
    Piece piece_on(Square s) const { return board_[s]; }
    Square ep_square() const { return state().ep_square; }
    int castling_rights() const { return state().castling; }
    int rule50() const { return state().rule50; }
    Key key() const { return state().key; }

    Bitboard pieces() const { return by_color_[WHITE] | by_color_[BLACK]; }
    Bitboard pieces(Color c) const { return by_color_[c]; }
    Bitboard pieces(PieceType pt) const { return by_type_[pt]; }
    Bitboard pieces(Color c, PieceType pt) const { return by_color_[c] & by_type_[pt]; }
    Square king_square(Color c) const { return lsb(pieces(c, KING)); }

    // Is square s attacked by any piece of colour `by`?
    bool is_attacked(Square s, Color by) const;
    // All pieces (both colours) attacking s, given an occupancy.
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    bool in_check() const { return is_attacked(king_square(side_to_move_), ~side_to_move_); }

    // Hash computed from scratch. Tests compare it with the incrementally
    // updated key() to catch make/unmake bugs.
    Key compute_key() const;

private:
    const StateInfo& state() const { return states_.back(); }

    void put_piece(Piece p, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);

    Bitboard by_type_[PIECE_TYPE_NB] = {};
    Bitboard by_color_[COLOR_NB] = {};
    Piece board_[SQUARE_NB] = {};      // mailbox copy: "what is on square s?" in O(1)
    Color side_to_move_ = WHITE;
    int fullmove_ = 1;
    std::vector<StateInfo> states_;   // states_.back() is the current state
};
