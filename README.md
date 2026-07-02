# PZShatranjBot

![donate btc](https://img.shields.io/badge/donate%20btc-31pma4U314hJHSxXBECWxYFPBgL7n9BoCC-blue)

<img src="logo.png" alt="PZShatranjBot Logo" width="1024"/>

PZShatranjBot is a UCI shatranj engine derived from PZChessBot.

## Installation

Clone the repository and build it locally:

1. Clone the repository:

```bash
git clone https://github.com/kevlu8/PZShatranjBot.git
```

2. Build the engine:

```bash
make -j
```

## Logistics & Features

PZShatranjBot is a basic negamax engine.

### Search

- Basic alpha-beta pruning
- Quiescence search
- Principal-Variation Search
- Late-move reductions
- Late-move pruning
- Transposition tables
- Null-move pruning
- Move ordering using MVV+CaptHist, killer moves, butterfly history, and continuation history
- Aspiration windows and iterative deepening
- Internal iterative reductions
- Reverse futility pruning
- Razoring
- Singular extensions
- Multi-cut pruning
- Negative extensions
- History pruning
- Static exchange evaluation pruning
- QS futility pruning
- Static evaluation correction history (pawn, non-pawn, major, minor, continuation)
- TT-corrected evaluation
- Mate-distance pruning
- Improving heuristic
- Multithreading with Lazy SMP
- ProbCut

### Moves and board representation

- Hybrid bitboard + mailbox representation
- PEXT bitboards for lightning fast move generation

### Evaluation

- Shatranj piece-square evaluation adapted from Prolix's PRF evaluator
- Includes material, piece placement, and a side-to-move tempo bonus

### Special Thanks

- The [Stockfish Discord Server](https://discord.gg/XUyHyT5ap9), specifically `#engines-dev` for their help!
- [Prolix](https://github.com/sscg13/Prolix) for the initial shatranj PRF parameters
- The [ChessProgramming Wiki](https://chessprogramming.org/) for their clear albeit outdated explanations
- [OpenBench](https://github.com/AndyGrant/OpenBench) for providing an excellent testing GUI
- [sscg13](https://github.com/sscg13) for having shared an OpenBench instance with me and helping me with a lot of miscellaneous stuff
- [Jonathan Hallström](https://github.com/JonathanHallstrom) for donating hardware and giving lots of advice
- Lastly, YOU for checking out this project!
