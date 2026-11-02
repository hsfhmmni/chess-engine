#pragma once

#include <cstdint>

// xorshift64* pseudo-random generator: tiny, fast, and good enough for
// Zobrist keys and magic-number search. std::mt19937_64 would also work, but
// this is deterministic across compilers and standard libraries.
class PRNG {
public:
    explicit PRNG(std::uint64_t seed) : s_(seed) {}

    std::uint64_t next() {
        s_ ^= s_ >> 12;
        s_ ^= s_ << 25;
        s_ ^= s_ >> 27;
        return s_ * 2685821657736338717ULL;
    }

    // Few bits set (about 1 in 8). Good magics tend to be sparse.
    std::uint64_t sparse() { return next() & next() & next(); }

private:
    std::uint64_t s_;
};
