#include "position.h"

#include <sstream>

#include "prng.h"

namespace Zobrist {
// A position's key is the XOR of one random number per feature present.
// XOR is its own inverse, so make_move updates the key by XOR-ing features
// out and in, instead of rehashing the whole board.
Key piece_square[PIECE_NB][SQUARE_NB];
Key castling[16];
Key ep_file[8];
Key side;
}  // namespace Zobrist

namespace {

// castling &= CastlingMask[from] & CastlingMask[to] after every move.
// Moving the king or a rook from its home square, or capturing a rook on its
// home square, clears the matching rights. Every other square keeps all 15.
int CastlingMask[SQUARE_NB];

constexpr const char* PieceChars = "PNBRQKpnbrqk";

}  // namespace

void Position::init() {
    PRNG rng(0xC0FFEE1234567ULL);
    for (auto& per_piece : Zobrist::piece_square)
        for (Key& k : per_piece)
            k = rng.next();
    for (Key& k : Zobrist::castling)
        k = rng.next();
    for (Key& k : Zobrist::ep_file)
        k = rng.next();
    Zobrist::side = rng.next();

    for (int& m : CastlingMask)
        m = ALL_CASTLING;
    CastlingMask[E1] &= ~(WHITE_OO | WHITE_OOO);
    CastlingMask[H1] &= ~WHITE_OO;
    CastlingMask[A1] &= ~WHITE_OOO;
    CastlingMask[E8] &= ~(BLACK_OO | BLACK_OOO);
    CastlingMask[H8] &= ~BLACK_OO;
    CastlingMask[A8] &= ~BLACK_OOO;
}

void Position::put_piece(Piece p, Square s) {
    Bitboard b = square_bb(s);
    board_[s] = p;
    by_type_[type_of(p)] |= b;
    by_color_[color_of(p)] |= b;
}

void Position::remove_piece(Square s) {
    Piece p = board_[s];
    Bitboard b = square_bb(s);
    by_type_[type_of(p)] ^= b;
    by_color_[color_of(p)] ^= b;
    board_[s] = NO_PIECE;
}

void Position::move_piece(Square from, Square to) {
    Piece p = board_[from];
    Bitboard b = square_bb(from) | square_bb(to);
    by_type_[type_of(p)] ^= b;
    by_color_[color_of(p)] ^= b;
    board_[from] = NO_PIECE;
    board_[to] = p;
}

bool Position::set_fen(const std::string& fen) {
    for (auto& b : by_type_)
        b = 0;
    for (auto& b : by_color_)
        b = 0;
    for (auto& p : board_)
        p = NO_PIECE;
    states_.clear();
    states_.emplace_back();
    StateInfo& st = states_.back();

    std::istringstream in(fen);
    std::string placement, side, castling, ep;
    if (!(in >> placement >> side >> castling >> ep))
        return false;
    // Half-move and full-move counters are optional: many test FENs omit them.
    int rule50 = 0, fullmove = 1;
    in >> rule50 >> fullmove;

    // 1. Piece placement, from a8 across and down to h1.
    int file = 0, rank = 7;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8)
                return false;
            file = 0;
            if (--rank < 0)
                return false;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8)
                return false;
        } else {
            const char* p = std::char_traits<char>::find(PieceChars, 12, c);
            if (!p || file > 7)
                return false;
            put_piece(Piece(p - PieceChars), make_square(file, rank));
            ++file;
        }
    }
    if (rank != 0 || file != 8)
        return false;

    // 2. Side to move.
    if (side == "w")
        side_to_move_ = WHITE;
    else if (side == "b")
        side_to_move_ = BLACK;
    else
        return false;

    // 3. Castling rights. Drop any right whose king or rook is not on its
    // home square: otherwise the generator would happily castle with a rook
    // that isn't there.
    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
            case 'K': st.castling |= WHITE_OO; break;
            case 'Q': st.castling |= WHITE_OOO; break;
            case 'k': st.castling |= BLACK_OO; break;
            case 'q': st.castling |= BLACK_OOO; break;
            default: return false;
            }
        }
    }
    if (board_[E1] != W_KING)
        st.castling &= ~(WHITE_OO | WHITE_OOO);
    if (board_[H1] != W_ROOK)
        st.castling &= ~WHITE_OO;
    if (board_[A1] != W_ROOK)
        st.castling &= ~WHITE_OOO;
    if (board_[E8] != B_KING)
        st.castling &= ~(BLACK_OO | BLACK_OOO);
    if (board_[H8] != B_ROOK)
        st.castling &= ~BLACK_OO;
    if (board_[A8] != B_ROOK)
        st.castling &= ~BLACK_OOO;

    // 4. En passant square. We keep it only if a pawn can actually capture
    // there. FENs often list it after every double push, and an unusable ep
    // square would make two identical positions hash differently, which
    // breaks repetition detection later.
    if (ep != "-") {
        if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || (ep[1] != '3' && ep[1] != '6'))
            return false;
        Square s = make_square(ep[0] - 'a', ep[1] - '1');
        Color us = side_to_move_;
        if (PawnAttacks[~us][s] & pieces(us, PAWN))
            st.ep_square = s;
    }

    st.rule50 = rule50;
    fullmove_ = fullmove < 1 ? 1 : fullmove;

    // Sanity checks: exactly one king each, no pawns on the back ranks, and
    // the side that just moved must not be in check (its king could be
    // "captured", which the legality test in movegen does not expect).
    if (popcount(pieces(WHITE, KING)) != 1 || popcount(pieces(BLACK, KING)) != 1)
        return false;
    if (pieces(PAWN) & (RANK_1_BB | RANK_8_BB))
        return false;
    if (is_attacked(king_square(~side_to_move_), side_to_move_))
        return false;

    st.key = compute_key();
    return true;
}

std::string Position::fen() const {
    std::string s;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            Piece p = board_[make_square(f, r)];
            if (p == NO_PIECE) {
                ++empty;
                continue;
            }
            if (empty)
                s += char('0' + empty);
            empty = 0;
            s += PieceChars[p];
        }
        if (empty)
            s += char('0' + empty);
        if (r)
            s += '/';
    }
    s += side_to_move_ == WHITE ? " w " : " b ";

    int cr = castling_rights();
    if (!cr)
        s += '-';
    if (cr & WHITE_OO) s += 'K';
    if (cr & WHITE_OOO) s += 'Q';
    if (cr & BLACK_OO) s += 'k';
    if (cr & BLACK_OOO) s += 'q';

    s += ' ';
    s += ep_square() == NO_SQUARE ? "-" : square_name(ep_square());
    s += ' ' + std::to_string(rule50()) + ' ' + std::to_string(fullmove_);
    return s;
}

std::string Position::to_string() const {
    std::ostringstream out;
    for (int r = 7; r >= 0; --r) {
        out << " +---+---+---+---+---+---+---+---+\n " << r + 1;
        for (int f = 0; f < 8; ++f) {
            Piece p = board_[make_square(f, r)];
            out << "| " << (p == NO_PIECE ? ' ' : PieceChars[p]) << ' ';
        }
        out << "|\n";
    }
    out << " +---+---+---+---+---+---+---+---+\n";
    out << "   a   b   c   d   e   f   g   h\n\n";
    out << "Fen: " << fen() << "\n";
    out << "Key: " << std::hex << std::uppercase << key() << std::dec << "\n";
    return out.str();
}

Key Position::compute_key() const {
    Key k = 0;
    for (Bitboard b = pieces(); b;) {
        Square s = pop_lsb(b);
        k ^= Zobrist::piece_square[board_[s]][s];
    }
    k ^= Zobrist::castling[castling_rights()];
    if (ep_square() != NO_SQUARE)
        k ^= Zobrist::ep_file[file_of(ep_square())];
    if (side_to_move_ == BLACK)
        k ^= Zobrist::side;
    return k;
}

Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
    // Symmetry trick: a white pawn attacks s exactly when a black pawn on s
    // would attack the white pawn's square. Same idea for every piece type.
    return (PawnAttacks[BLACK][s] & pieces(WHITE, PAWN)) |
           (PawnAttacks[WHITE][s] & pieces(BLACK, PAWN)) |
           (KnightAttacks[s] & pieces(KNIGHT)) |
           (bishop_attacks(s, occupied) & (pieces(BISHOP) | pieces(QUEEN))) |
           (rook_attacks(s, occupied) & (pieces(ROOK) | pieces(QUEEN))) |
           (KingAttacks[s] & pieces(KING));
}

bool Position::is_attacked(Square s, Color by) const {
    Bitboard them = pieces(by);
    Bitboard occ = pieces();
    return (PawnAttacks[~by][s] & pieces(by, PAWN)) ||
           (KnightAttacks[s] & them & pieces(KNIGHT)) ||
           (KingAttacks[s] & them & pieces(KING)) ||
           (bishop_attacks(s, occ) & them & (pieces(BISHOP) | pieces(QUEEN))) ||
           (rook_attacks(s, occ) & them & (pieces(ROOK) | pieces(QUEEN)));
}

void Position::make_move(Move m) {
    // Copy the current state, then edit the copy. unmake_move just pops it.
    states_.push_back(states_.back());
    StateInfo& st = states_.back();

    const Color us = side_to_move_;
    const Color them = ~us;
    const Square from = m.from();
    const Square to = m.to();
    const Piece pc = board_[from];
    const int flag = m.flag();

    st.captured = NO_PIECE;
    ++st.rule50;

    // The old en passant chance expires after any move.
    if (st.ep_square != NO_SQUARE) {
        st.key ^= Zobrist::ep_file[file_of(st.ep_square)];
        st.ep_square = NO_SQUARE;
    }

    // Remove the captured piece first, so move_piece lands on an empty square.
    if (flag == EP_CAPTURE) {
        // The captured pawn is beside the moving pawn, not on `to`.
        Square cap_sq = Square(us == WHITE ? to - 8 : to + 8);
        st.captured = board_[cap_sq];
        st.key ^= Zobrist::piece_square[st.captured][cap_sq];
        remove_piece(cap_sq);
    } else if (m.is_capture()) {
        st.captured = board_[to];
        st.key ^= Zobrist::piece_square[st.captured][to];
        remove_piece(to);
    }
    if (st.captured != NO_PIECE || type_of(pc) == PAWN)
        st.rule50 = 0;

    move_piece(from, to);
    st.key ^= Zobrist::piece_square[pc][from] ^ Zobrist::piece_square[pc][to];

    if (m.is_promotion()) {
        Piece promoted = make_piece(us, m.promotion_type());
        remove_piece(to);
        put_piece(promoted, to);
        st.key ^= Zobrist::piece_square[pc][to] ^ Zobrist::piece_square[promoted][to];
    } else if (m.is_castle()) {
        // The king has moved two squares; now move the rook over it.
        Square rook_from = flag == KING_CASTLE ? Square(to + 1) : Square(to - 2);
        Square rook_to = flag == KING_CASTLE ? Square(to - 1) : Square(to + 1);
        Piece rook = board_[rook_from];
        move_piece(rook_from, rook_to);
        st.key ^= Zobrist::piece_square[rook][rook_from] ^ Zobrist::piece_square[rook][rook_to];
    } else if (flag == DOUBLE_PUSH) {
        // Set the ep square only if an enemy pawn could really capture there
        // (same reasoning as in set_fen).
        Square ep = Square((from + to) / 2);
        if (PawnAttacks[us][ep] & pieces(them, PAWN)) {
            st.ep_square = ep;
            st.key ^= Zobrist::ep_file[file_of(ep)];
        }
    }

    int new_castling = st.castling & CastlingMask[from] & CastlingMask[to];
    if (new_castling != st.castling) {
        st.key ^= Zobrist::castling[st.castling] ^ Zobrist::castling[new_castling];
        st.castling = new_castling;
    }

    if (us == BLACK)
        ++fullmove_;
    side_to_move_ = them;
    st.key ^= Zobrist::side;
}

void Position::unmake_move(Move m) {
    // Undo in the reverse order of make_move.
    side_to_move_ = ~side_to_move_;
    const Color us = side_to_move_;
    const Square from = m.from();
    const Square to = m.to();
    const int flag = m.flag();

    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    } else if (m.is_castle()) {
        Square rook_from = flag == KING_CASTLE ? Square(to + 1) : Square(to - 2);
        Square rook_to = flag == KING_CASTLE ? Square(to - 1) : Square(to + 1);
        move_piece(rook_to, rook_from);
    }

    move_piece(to, from);

    Piece captured = states_.back().captured;
    if (captured != NO_PIECE) {
        Square cap_sq = to;
        if (flag == EP_CAPTURE)
            cap_sq = Square(us == WHITE ? to - 8 : to + 8);
        put_piece(captured, cap_sq);
    }

    if (us == BLACK)
        --fullmove_;
    states_.pop_back();
}
