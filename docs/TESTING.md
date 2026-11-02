# Testing

Two kinds of testing protect this engine. **Correctness tests** catch bugs: perft now,
and `bench` once search exists. **Strength tests** decide whether a change makes the
engine stronger. Strength is never judged by watching a few games.

## Perft (available now)

Perft counts every legal move sequence to a fixed depth. The counts for well-known
positions are published, so any mismatch means a move generation bug.

```sh
./build/perft_test           # quick suite, about 1 second, also run by CTest
./build/perft_test --deep    # every position to full depth, about 30 seconds
```

The suite also:

- compares the magic attack tables with a slow ray walk on 20,000 random occupancies;
- checks at every node to depth 3 that the incremental hash equals a from-scratch hash,
  and that unmake restores the exact FEN;
- checks FEN round-trips, and that 7 kinds of malformed or illegal FEN are rejected.

**Finding a perft bug.** Run `./build/engine perft N "<fen>"` on the failing position. It prints
the count for each first move. Compare those counts with a trusted engine (Stockfish's
`go perft N` prints the same breakdown), play the move whose count differs, and repeat one
ply deeper until a single position shows the bug.

**Reference data** (FENs and expected counts) is copied from the
[Chess Programming Wiki](https://www.chessprogramming.org/Perft_Results), never typed from memory.

## Bench (Testing rig stage)

`bench` will search about 50 fixed positions to a fixed depth and print the total node count.
The same code always gives the same number. Each commit message records it. If it changes in a
commit that shouldn't affect search, that commit has a bug.

## SPRT self-play (Testing rig stage)

Every change meant to affect strength plays the new build against the old one with
[fastchess](https://github.com/Disservin/fastchess), starting from a book of balanced openings.

SPRT (Sequential Probability Ratio Test) keeps a running log-likelihood ratio between two
hypotheses, such as "the gain is 0 Elo" against "the gain is 5 Elo", and stops as soon as it
crosses an upper or lower bound. With α = β = 0.05 it needs far fewer games on average than
a fixed-length match.

| Change | Bounds (Elo) | Time control |
|---|---|---|
| New feature | [0, 5] | 8s + 0.08s |
| Simplification (removing code) | [−5, 0] | 8s + 0.08s |
| Confirming a large gain | [0, 5] | 40s + 0.4s |

## Gauntlet (at each phase gate)

Self-play exaggerates gains: +30 against the parent build can be +15 against other engines.
At each gate, the engine plays 3 to 5 engines with known CCRL ratings between about 1800 and
2200, which gives an honest estimate of its own rating.
