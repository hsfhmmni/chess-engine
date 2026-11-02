#include "movegen.h"

namespace {

// Add one move per target square, with the given offset back to the origin.
// Used for pawns, where every target in a set came from the same direction.
inline void add_pawn_moves(MoveList& list, Bitboard targets, int offset, int flag) {
    while (targets) {
        Square to = pop_lsb(targets);
        list.add(Move(Square(to - offset), to, flag));
    }
}

inline void add_promotions(MoveList& list, Bitboard targets, int offset, bool capture) {
    while (targets) {
        Square to = pop_lsb(targets);
        Square from = Square(to - offset);
        // Queen first: it is almost always best, which will help move
        // ordering later. Underpromotions still matter for correctness.
        for (PieceType pt : {QUEEN, KNIGHT, ROOK, BISHOP})
            list.add(Move(from, to, Move::promo_flag(pt, capture)));
    }
}

// Templated on colour so the directions are compile-time constants and the
// compiler can pick the right shift without branching.
template <Color Us>
void generate_pawn_moves(const Position& pos, MoveList& list) {
    constexpr Color Them = Us == WHITE ? BLACK : WHITE;
    constexpr int Up = Us == WHITE ? 8 : -8;
    constexpr int UpLeft = Us == WHITE ? 7 : -9;   // capture towards the a-file
    constexpr int UpRight = Us == WHITE ? 9 : -7;  // capture towards the h-file
    constexpr Bitboard Rank3 = Us == WHITE ? RANK_3_BB : RANK_6_BB;  // after one push
    constexpr Bitboard Rank7 = Us == WHITE ? RANK_7_BB : RANK_2_BB;  // about to promote

    const Bitboard pawns = pos.pieces(Us, PAWN);
    const Bitboard empty = ~pos.pieces();
    const Bitboard enemies = pos.pieces(Them);
    const Bitboard promoting = pawns & Rank7;
    const Bitboard others = pawns & ~Rank7;

    // Pushes, all pawns at once. A double push needs both squares empty, so
    // shift the single-push result again rather than the pawns themselves.
    Bitboard single = shift<Up>(others) & empty;
    Bitboard dbl = shift<Up>(single & Rank3) & empty;
    add_pawn_moves(list, single, Up, QUIET);
    add_pawn_moves(list, dbl, 2 * Up, DOUBLE_PUSH);

    add_pawn_moves(list, shift<UpLeft>(others) & enemies, UpLeft, CAPTURE);
    add_pawn_moves(list, shift<UpRight>(others) & enemies, UpRight, CAPTURE);

    if (promoting) {
        add_promotions(list, shift<Up>(promoting) & empty, Up, false);
        add_promotions(list, shift<UpLeft>(promoting) & enemies, UpLeft, true);
        add_promotions(list, shift<UpRight>(promoting) & enemies, UpRight, true);
    }

    // En passant: our pawns that attack the ep square. Reverse lookup again:
    // squares a Them-pawn on ep would attack are where our capturers stand.
    if (pos.ep_square() != NO_SQUARE) {
        Bitboard capturers = PawnAttacks[Them][pos.ep_square()] & pawns;
        while (capturers)
            list.add(Move(pop_lsb(capturers), pos.ep_square(), EP_CAPTURE));
    }
}

// Knights, bishops, rooks, queens and king: attacks minus our own pieces.
void generate_piece_moves(const Position& pos, MoveList& list, Color us) {
    const Bitboard occ = pos.pieces();
    const Bitboard own = pos.pieces(us);
    const Bitboard enemies = pos.pieces(~us);

    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
        Bitboard pieces = pos.pieces(us, pt);
        while (pieces) {
            Square from = pop_lsb(pieces);
            Bitboard targets;
            switch (pt) {
            case KNIGHT: targets = KnightAttacks[from]; break;
            case BISHOP: targets = bishop_attacks(from, occ); break;
            case ROOK: targets = rook_attacks(from, occ); break;
            case QUEEN: targets = queen_attacks(from, occ); break;
            default: targets = KingAttacks[from]; break;
            }
            targets &= ~own;
            while (targets) {
                Square to = pop_lsb(targets);
                list.add(Move(from, to, (enemies & square_bb(to)) ? CAPTURE : QUIET));
            }
        }
    }
}

void generate_castling(const Position& pos, MoveList& list, Color us) {
    const int rights = pos.castling_rights();
    const Color them = ~us;
    const Bitboard occ = pos.pieces();

    // Can't castle out of check.
    if (!(rights & (us == WHITE ? (WHITE_OO | WHITE_OOO) : (BLACK_OO | BLACK_OOO))) ||
        pos.in_check())
        return;

    // Squares between king and rook must be empty. The squares the king
    // crosses and lands on must not be attacked. Note the asymmetry on the
    // queen side: b1 must be empty, but may be attacked.
    if (us == WHITE) {
        if ((rights & WHITE_OO) && !(occ & (square_bb(F1) | square_bb(G1))) &&
            !pos.is_attacked(F1, them) && !pos.is_attacked(G1, them))
            list.add(Move(E1, G1, KING_CASTLE));
        if ((rights & WHITE_OOO) && !(occ & (square_bb(B1) | square_bb(C1) | square_bb(D1))) &&
            !pos.is_attacked(D1, them) && !pos.is_attacked(C1, them))
            list.add(Move(E1, C1, QUEEN_CASTLE));
    } else {
        if ((rights & BLACK_OO) && !(occ & (square_bb(F8) | square_bb(G8))) &&
            !pos.is_attacked(F8, them) && !pos.is_attacked(G8, them))
            list.add(Move(E8, G8, KING_CASTLE));
        if ((rights & BLACK_OOO) && !(occ & (square_bb(B8) | square_bb(C8) | square_bb(D8))) &&
            !pos.is_attacked(D8, them) && !pos.is_attacked(C8, them))
            list.add(Move(E8, C8, QUEEN_CASTLE));
    }
}

}  // namespace

void generate_pseudo_legal(const Position& pos, MoveList& list) {
    const Color us = pos.side_to_move();
    if (us == WHITE)
        generate_pawn_moves<WHITE>(pos, list);
    else
        generate_pawn_moves<BLACK>(pos, list);
    generate_piece_moves(pos, list, us);
    generate_castling(pos, list, us);
}

void generate_legal(Position& pos, MoveList& list) {
    MoveList pseudo;
    generate_pseudo_legal(pos, pseudo);
    const Color us = pos.side_to_move();
    for (Move m : pseudo) {
        pos.make_move(m);
        // After the move it is the opponent's turn: is our king attacked?
        // This one test covers pins, moving into check, and the tricky en
        // passant case where removing two pawns from a rank exposes the king.
        if (!pos.is_attacked(pos.king_square(us), ~us))
            list.add(m);
        pos.unmake_move(m);
    }
}

Move parse_uci_move(Position& pos, const std::string& text) {
    MoveList list;
    generate_legal(pos, list);
    for (Move m : list)
        if (m.to_uci() == text)
            return m;
    return Move();
}

std::uint64_t perft(Position& pos, int depth) {
    if (depth == 0)
        return 1;
    MoveList list;
    generate_legal(pos, list);
    // "Bulk counting": at the last ply, the number of legal moves is the
    // number of leaves, so there's no need to make each one.
    if (depth == 1)
        return std::uint64_t(list.size());
    std::uint64_t nodes = 0;
    for (Move m : list) {
        pos.make_move(m);
        nodes += perft(pos, depth - 1);
        pos.unmake_move(m);
    }
    return nodes;
}
