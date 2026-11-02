# chess-engine

A UCI chess engine written from scratch in C++20, built one tested feature at a
time with the goal of a **2000+ rating on the [CCRL](https://computerchess.org.uk/ccrl/)**
(Computer Chess Rating Lists), the standard independent rating list for engines.

> **Status: Phase 1 (move generation) is in review.** The board representation and
> move generator are written and pass every standard perft test, but the engine
> cannot play a game yet: there is no search and no UCI `go` command. Phase 1 is
> finished when it is tagged `v0.1`. See the [roadmap](docs/ROADMAP.md).

## What's in it so far

- **Bitboards**: the board is stored as 64-bit integers, one per piece type and one
  per colour. ANDing them together gives, for example, all white knights.
- **Magic bitboards** for rook, bishop and queen attacks. The magic numbers are
  searched for at startup with a fixed seed, so none of them are pasted in unexplained.
- **Make/unmake** with a state stack, and **Zobrist hashing** updated incrementally.
- **FEN** input and output, with malformed or illegal positions rejected.
- **Perft** (move path enumeration) tests against the 7 standard positions from the
  [Chess Programming Wiki](https://www.chessprogramming.org/Perft_Results), all
  matching, at roughly 25 to 30 million nodes per second.

Why it is built this way, and the alternatives that were rejected, is in
[docs/DESIGN.md](docs/DESIGN.md).

## Building

Requires CMake 3.16+ and a C++20 compiler (GCC 12+, Clang 15+ or recent MSVC).
On Windows, WSL (Ubuntu) or MSYS2 is the easiest route.

```sh
cmake -B build
cmake --build build
```

This builds two programs: `build/engine` and the test suite `build/perft_test`.
Pass `-DNATIVE=OFF` for a portable binary without `-march=native`.

## Testing

```sh
./build/perft_test           # quick suite, about 1 second
./build/perft_test --deep    # full suite, about 30 seconds (805 million nodes)
cd build && ctest          # the quick suite through CTest
```

The suite checks perft counts for all 7 standard positions, compares every magic
lookup with a slow ray walk, checks that the incremental hash and unmake are exact
at every node, and checks that bad FENs are rejected.
[docs/TESTING.md](docs/TESTING.md) explains this and the self-play testing
that every later strength change has to pass.

## Using the engine

Today the engine understands a small set of commands, a subset of UCI plus
two debugging extras:

```text
position startpos moves e2e4 e7e5
position fen <fen> [moves ...]
go perft <depth>      # count positions per move ("divide"), total, and speed
d                     # print the board and its FEN
quit
```

It can also be run once from the shell:

```sh
./build/engine perft 5
./build/engine perft 3 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
```

Full UCI support, so it can play in GUIs such as Cute Chess or Arena, arrives in Phase 2.

## Roadmap

| Phase | What it adds | Target | Status |
|---|---|---|---|
| 1 | Bitboard board, move generation, perft | Nov 2026 | In review |
| — | Testing rig: `bench`, fastchess + SPRT, gauntlet | Dec 2026 | Not started |
| 2 | UCI, alpha-beta search, draw rules, starter evaluation | Dec 2026 | Not started |
| 3 | Quiescence, move ordering, iterative deepening, transposition table, pruning | Jan–Mar 2027 | Not started |
| 4 | Evaluation terms, Texel tuning, time management | Jul–Aug 2027 | Not started |
| 5 | Release, Lichess bot, CCRL submission | Sep 2027 | Not started |

April to June 2027 is a planned pause for exams. Each phase ends with a gate that
must pass before the next one starts. The full plan, with the gates, is in
[docs/ROADMAP.md](docs/ROADMAP.md).

## How changes are accepted

- One feature per branch.
- Every change that is meant to affect strength must pass an SPRT test in self-play.
  Watching a few games doesn't count.
- `main` always builds and passes perft.
- Test positions and reference numbers are copied from their source, never typed from memory.

## Licence

No licence has been chosen yet, so for now all rights are reserved. A licence (MIT or GPL-3.0)
will be added before the public release in Phase 5.
