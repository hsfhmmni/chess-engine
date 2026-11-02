# Design notes

Why the engine is built the way it is, and which alternatives were rejected. Each
section is added when its phase lands.

## Source layout

| File | What it does |
|---|---|
| `src/types.h` | Square, Piece, Color, the 16-bit `Move`, `MoveList` |
| `src/bitboard.h/.cpp` | bit helpers, leaper tables, magic bitboards for sliders |
| `src/prng.h` | xorshift64* generator for magics and Zobrist keys |
| `src/position.h/.cpp` | board state, FEN in/out, make/unmake, Zobrist hash, attack queries |
| `src/movegen.h/.cpp` | pseudo-legal and legal generation, UCI move parsing, `perft` |
| `src/main.cpp` | command loop, plus `./engine perft N [fen]` |
| `tests/perft_test.cpp` | the perft and consistency suite, registered with CTest |

CMake builds everything except `main.cpp` into an `engine_core` library, which both
`engine` and `perft_test` link against.

## Phase 1: board and move generation

### Sliding piece attacks: magic bitboards

| Option | Trade-off |
|---|---|
| Ray loops | Simple, but a branch per square and slow |
| Kogge-Stone fills | Branch-free and table-free, but more instructions per lookup |
| **Magic bitboards (chosen)** | Mask the relevant blockers, multiply by a magic number, shift, look up: one multiply and one load |
| PEXT (BMI2) | Same idea in hardware; fast on Intel and AMD Zen 3+, slow on older AMD, not portable |

The magics are searched for at startup with a fixed seed (a few milliseconds) instead of
being pasted in as 128 constants, and the test suite checks every table against a ray
walk. PEXT may be tried later, and kept only if perft speed shows it helps.

### Legality: make, test, unmake

Moves are generated pseudo-legally, then each is made and kept only if our own king is not
attacked. A pin-aware legal generator is about 2 to 3 times faster, but has many more ways
to be subtly wrong (pins, double check, en passant). The simple version came first; it will
be replaced only if profiling shows movegen matters, and perft will catch any regression.
The cost: in search, each move is made and unmade twice.

### Undoing moves: a state stack

`make_move` pushes a `StateInfo` (castling rights, en passant square, 50-move counter,
captured piece, hash) and `unmake_move` pops it. Copy-make is simpler, but copies the
whole board every ply, and that cost grows as the board struct grows.

### En passant square only when a capture is possible

Many FENs record e3 after every e2e4. Storing it blindly would give identical positions
different hashes, and repetition detection in Phase 2 would quietly miss draws. Side
effect: `fen()` may print `-` where the input FEN had a square.

### Zobrist hashing from the start

Phase 1 doesn't need it, but it is cheap to add while make/unmake is fresh, and checking
it now is much easier than after search is built on top.

### Pitfalls handled

- **En passant discovered check:** an en passant capture removes two pawns from one rank
  and can expose the king to a rook. Make/test/unmake catches this for free.
- **Castling:** not out of check, through an attacked square, or into one. On the queen
  side, b1 must be empty but may be attacked.
- **Castling rights** are lost when the king or rook moves, or when a rook is captured on
  its home square: `rights &= mask[from] & mask[to]`.
- **FEN castling rights without the pieces** are stripped.
- **Shift wrap-around:** shifting h4 east would land on a5 unless the h-file is masked first.
- **Underpromotions:** all four promotion pieces, with and without capture.

### Known limits

- No Chess960.
- No captures-only generator yet; quiescence search in Phase 3 will need one.
- `parse_uci_move` generates the full legal list to find one move. Fine for UCI input,
  not for use inside search.
