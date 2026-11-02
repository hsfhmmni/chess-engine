# chess-engine

A chess engine written from scratch in C++, aiming for 2000+ on the
[CCRL](https://computerchess.org.uk/ccrl/) rating list.

- Bitboard board representation
- UCI protocol, so it runs in any standard chess GUI
- Every change measured with self-play matches, one feature at a time

## Roadmap

1. Board representation and move generation, verified with perft
2. UCI support and basic search
3. Core search: alpha-beta, iterative deepening, move ordering, quiescence, transposition table
4. Evaluation tuning and time management
5. Public release on GitHub and Lichess, then CCRL submission

## Building

Requires CMake 3.16+ and a C++20 compiler.

```sh
cmake -B build
cmake --build build
./build/engine
```

Pass `-DNATIVE=OFF` to build a portable binary without `-march=native`.

## Testing the move generator

```sh
./build/perft_test           # quick suite, a few seconds
./build/perft_test --deep    # deeper counts, about a minute
./build/engine perft 5       # per-move breakdown from the start position
./build/engine perft 3 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
```

The engine also reads commands on stdin: `position startpos moves e2e4`,
`position fen <fen>`, `go perft <depth>`, `d` (show board) and `quit`.
