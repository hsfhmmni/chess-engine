# Roadmap

The goal is a classical engine (alpha-beta search plus a hand-written, tuned
evaluation) rated 2000+ on the CCRL by about September 2027.

Every stage ends with a **gate**: a check that must pass before the next stage
starts. If a stage overruns, every later date moves back. Gates are never
skipped to stay on schedule.

**Assumptions behind the dates:** about 5 to 8 hours a week in term time; a pause
from April to June 2027 for exams; one ordinary 4+ core machine for overnight
test runs; no NNUE and no multithreading, because neither is needed for 2000.

| Stage | Target | Version tag | Status |
|---|---|---|---|
| 0. Setup and tooling | Nov 2026 | — | In progress |
| 1. Board and move generation | Nov 2026 | `v0.1` | In review |
| Testing rig | Dec 2026 | — | Not started |
| 2. UCI and basic search | Dec 2026 | `v0.2` | Not started |
| 3. Core search | Jan–Mar 2027 | `v0.3` | Not started |
| Exam pause | Apr–Jun 2027 | — | — |
| 4. Evaluation tuning and time management | Jul–Aug 2027 | `v1.0-rc` | Not started |
| 5. Release and CCRL | Sep 2027 | `v1.0` | Not started |

---

## Stage 0: Setup and tooling

- [x] CMake build, C++20, warnings on, Release by default
- [ ] Builds and passes tests on my own machine with one command each
- [ ] `-DSANITIZE=ON` CMake option for AddressSanitizer and UBSan
- [ ] GitHub Actions: build and run the quick perft suite on every push
- [ ] Install [Cute Chess](https://github.com/cutechess/cutechess) and [fastchess](https://github.com/Disservin/fastchess)

**Gate:** a fresh clone builds and passes the tests locally and in CI.

## Stage 1: Board and move generation (`v0.1`)

- [x] Bitboards, magic bitboards for sliders, leaper attack tables
- [x] FEN in/out with validation
- [x] Make/unmake with a state stack, incremental Zobrist hash
- [x] Pseudo-legal generation, legality by make/test/unmake
- [x] Perft suite: all 7 standard positions, magic check, hash and unmake checks
- [ ] Code reviewed and understood line by line
- [ ] Committed and tagged `v0.1`

**Gate:** all perft tests pass, and I can explain without notes:
1. how a magic lookup turns the blocker squares into an attack set;
2. why en passant can expose your own king, and how make/test/unmake catches it;
3. what `rights &= mask[from] & mask[to]` does for castling rights.

Known limits carried forward: no Chess960; no captures-only generator yet
(Phase 3 needs one for quiescence search).

## Testing rig (before Phase 2 ends)

See [TESTING.md](TESTING.md) for how each tool is used.

- [ ] `bench` command: fixed positions, fixed depth, prints a node count
- [ ] fastchess SPRT script: new build against old build, run overnight
- [ ] Opening book for varied, balanced starting positions
- [ ] Gauntlet of 3 to 5 engines with known CCRL ratings around 1800 to 2200

**Gate:** one command runs an SPRT between two builds overnight, and `bench` is reproducible.

## Stage 2: UCI and basic search (`v0.2`)

- [ ] UCI loop: `uci`, `isready`, `ucinewgame`, `position`, `go` (`wtime btime winc binc movestogo depth movetime infinite`), `stop`, `quit`
- [ ] Decide how `stop` works (polling for input, or a search thread) before writing search
- [ ] Negamax alpha-beta at fixed depth; mate scored as −(MATE − ply), stalemate as 0
- [ ] Threefold repetition and 50-move rule
- [ ] Starter evaluation: material plus tapered PeSTO piece-square tables
- [ ] Simple time limit, about 1/30 of the remaining time per move

**Why the evaluation starts here and not in Phase 4:** with material only, the engine
plays aimless moves, and search features tested on top of it give misleading results.
Real tuning still waits for Phase 4.

**Gate:** 1,000 self-play games with no illegal moves, crashes or time losses; beats a
random-move player 100–0.

## Stage 3: Core search (`v0.3`)

Most of the rating comes from this stage. One feature per branch, in this order, each
kept only if it passes its SPRT. Later features depend on earlier ones.

| # | Feature | Rough gain (Elo) |
|---|---|---|
| 1 | Quiescence search (captures only, stand-pat) | +150 to 300 |
| 2 | MVV-LVA capture ordering | +50 to 100 |
| 3 | Iterative deepening with soft and hard time limits | needed for 4–9 |
| 4 | Transposition table, hash move first | +100 to 200 |
| 5 | Killer moves and history heuristic | +40 to 80 |
| 6 | Principal variation search | +10 to 30 |
| 7 | Null-move pruning (off in king-and-pawn endgames) | +50 to 100 |
| 8 | Late move reductions | +80 to 150 |
| 9 | Check extension, reverse futility pruning, aspiration windows | +10 to 40 each |

The gains are rough figures from other engines, not promises. A well-known feature
that fails its SPRT usually has a bug, so look for one before dropping the idea.

**Gate:** a gauntlet puts the engine at about 1800 to 2000 CCRL.

## Stage 4: Evaluation tuning and time management (`v1.0-rc`)

Evaluation terms, one SPRT each: passed pawns by rank; doubled and isolated pawns;
bishop pair; mobility; king safety (attackers and pawn shield); rooks on open and
half-open files.

**Texel tuning:** fit the weights to about a million quiet positions with known results
R ∈ {1, ½, 0} by minimising

E(w) = (1/N) Σ (Rᵢ − σ(K·qᵢ(w)))², where σ(x) = 1 / (1 + 10^(−x/400)).

Fit K first with the weights fixed, then the weights by gradient descent. Start with a
public labelled set, then generate positions from self-play.

**Time management:** soft limit about time/25 + 0.75 × increment (don't start a new
iteration after it); hard limit about time/5 (abort mid-iteration); later, stop early when
the best move is stable; always keep about 50 ms in reserve.

**Gate:** zero time losses in 2,000 games at 10s+0.1s and at 1min+0; a gauntlet at 2000+ CCRL.

## Stage 5: Release and CCRL (`v1.0`)

- [ ] Choose a real name for the engine
- [ ] Choose a licence (MIT or GPL-3.0; GPL is required if any GPL code or tables are copied)
- [ ] Tester-friendly: `Hash` and `Threads` options, `go` with no time arguments, nothing but UCI on stdout, no crashes on unusual FENs
- [ ] GitHub Release with Windows and Linux binaries (portable x86-64 and AVX2)
- [ ] Lichess BOT account running [lichess-bot](https://github.com/lichess-bot-devs/lichess-bot)
- [ ] Announce where CCRL testers look, and check the current process at [computerchess.org.uk/ccrl](https://computerchess.org.uk/ccrl/)

**Gate:** the engine appears on the CCRL list.

---

## Parked until after the CCRL listing

NNUE evaluation, multithreaded search, an own opening book, Chess960, a pin-aware
legal move generator (only if profiling shows movegen matters).

## Risks

| Risk | What to do about it |
|---|---|
| Committing code I don't understand | Explain each change before committing it |
| Testing changes by eye | Every strength change goes through an SPRT |
| Scope creep | Park ideas in the list above |
| Exams | Pause April to June, with notes written before the pause |
| Self-play inflating Elo | Run a gauntlet against rated engines at each gate |
