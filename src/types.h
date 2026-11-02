#pragma once

// Core types shared by every part of the engine.
//
// Square numbering is "little-endian rank-file": a1 = 0, b1 = 1, ..., h1 = 7,
// a2 = 8, ..., h8 = 63. So square = rank * 8 + file, and bit N of a Bitboard
// stands for square N.

#include <cstdint>
#include <string>

using Bitboard = std::uint64_t;
using Key = std::uint64_t;

enum Color : int { WHITE, BLACK, COLOR_NB = 2 };

constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PIECE_TYPE_NB = 6 };

// Piece = color * 6 + type, so a piece fits a small array index.
enum Piece : int {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    NO_PIECE, PIECE_NB = 12
};

constexpr Piece make_piece(Color c, PieceType pt) { return Piece(c * 6 + pt); }
constexpr Color color_of(Piece p) { return Color(p / 6); }
constexpr PieceType type_of(Piece p) { return PieceType(p % 6); }

// clang-format off
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQUARE, SQUARE_NB = 64
};
// clang-format on

constexpr int file_of(Square s) { return s & 7; }
constexpr int rank_of(Square s) { return s >> 3; }
constexpr Square make_square(int file, int rank) { return Square(rank * 8 + file); }

inline std::string square_name(Square s) {
    return {char('a' + file_of(s)), char('1' + rank_of(s))};
}

// Castling rights are four independent bits.
enum CastlingRight : int {
    NO_CASTLING = 0,
    WHITE_OO = 1,
    WHITE_OOO = 2,
    BLACK_OO = 4,
    BLACK_OOO = 8,
    ALL_CASTLING = 15
};

// A move packed into 16 bits:
//   bits 0-5   from square
//   bits 6-11  to square
//   bits 12-15 flag (the scheme from the Chess Programming Wiki)
//
// Flag values:
//   0 quiet          4 capture
//   1 double push    5 en passant capture
//   2 king castle    8-11  promotion to N, B, R, Q
//   3 queen castle   12-15 promotion with capture to N, B, R, Q
//
// Bit 2 of the flag (value 4) means "capture" and bit 3 (value 8) means
// "promotion", so both questions are a single AND.
enum MoveFlag : int {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    PROMOTION = 8,
    PROMO_CAPTURE = 12
};

class Move {
public:
    constexpr Move() = default;
    constexpr Move(Square from, Square to, int flag)
        : data_(std::uint16_t(from | (to << 6) | (flag << 12))) {}

    // Promotion flag for promoting to `pt` (KNIGHT..QUEEN), optionally capturing.
    static constexpr int promo_flag(PieceType pt, bool capture) {
        return (capture ? PROMO_CAPTURE : PROMOTION) | (pt - KNIGHT);
    }

    constexpr Square from() const { return Square(data_ & 63); }
    constexpr Square to() const { return Square((data_ >> 6) & 63); }
    constexpr int flag() const { return data_ >> 12; }

    constexpr bool is_capture() const { return flag() & CAPTURE; }
    constexpr bool is_promotion() const { return flag() & PROMOTION; }
    constexpr bool is_castle() const { return flag() == KING_CASTLE || flag() == QUEEN_CASTLE; }
    constexpr PieceType promotion_type() const { return PieceType(KNIGHT + (flag() & 3)); }

    constexpr bool is_null() const { return data_ == 0; }
    constexpr bool operator==(const Move&) const = default;

    // Long algebraic notation as used by UCI: e2e4, e7e8q, e1g1 (castling).
    std::string to_uci() const {
        if (is_null())
            return "0000";
        std::string s = square_name(from()) + square_name(to());
        if (is_promotion())
            s += "nbrq"[promotion_type() - KNIGHT];
        return s;
    }

private:
    std::uint16_t data_ = 0;
};

// Fixed-capacity list: no heap allocation inside the search's hot path.
// 218 is the most legal moves known in any reachable position, and
// pseudo-legal counts stay comfortably below 256 in practice.
struct MoveList {
    Move moves[256];
    int count = 0;

    void add(Move m) { moves[count++] = m; }
    int size() const { return count; }
    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const { return moves + count; }
};
