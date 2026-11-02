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
