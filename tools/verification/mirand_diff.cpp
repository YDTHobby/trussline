// Differential test: signed vs unsigned PRNG state in ApproxMath.h (R-16 step 2).
//
// `mirand *= 16807` overflows by design. As a signed int that is undefined
// behaviour; as uint32_t it is defined wraparound. The claim being tested is
// that switching the type leaves the generated sequence bit-for-bit unchanged,
// so the only behavioural difference from the commit is the removal of the
// data race - not a different stream of random numbers.
//
// Build from a Developer Command Prompt:
//   cl /nologo /O2 /fp:fast /EHsc mirand_diff.cpp /Fe:mirand_diff.exe && mirand_diff.exe

#include <cstdint>
#include <cstdio>
#include <cstring>

static int      mirand_signed   = 1;   // old
static uint32_t mirand_unsigned = 1;   // new

static inline float bits_to_float(uint32_t b) { float f; std::memcpy(&f, &b, sizeof f); return f; }

// --- old form (signed state, as upstream had it) ---
static inline float old_frand() {
    mirand_signed *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_signed) & 0x007fffffu) | 0x40000000u;
    return (bits_to_float(a) - 2.0f) * 0.5f;
}
static inline float old_frand_02() {
    mirand_signed *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_signed) & 0x007fffffu) | 0x40000000u;
    return bits_to_float(a) - 2.0f;
}
static inline float old_frand_11() {
    mirand_signed *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_signed) & 0x007fffffu) | 0x40000000u;
    return bits_to_float(a) - 3.0f;
}

// --- new form (unsigned state, as committed) ---
static inline float new_frand() {
    mirand_unsigned *= 16807;
    const uint32_t a = (mirand_unsigned & 0x007fffffu) | 0x40000000u;
    return (bits_to_float(a) - 2.0f) * 0.5f;
}
static inline float new_frand_02() {
    mirand_unsigned *= 16807;
    const uint32_t a = (mirand_unsigned & 0x007fffffu) | 0x40000000u;
    return bits_to_float(a) - 2.0f;
}
static inline float new_frand_11() {
    mirand_unsigned *= 16807;
    const uint32_t a = (mirand_unsigned & 0x007fffffu) | 0x40000000u;
    return bits_to_float(a) - 3.0f;
}

static bool bitsEqual(float a, float b) {
    uint32_t x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4); return x == y;
}

int main() {
    const int N = 20000000;
    long long checked = 0, mismatch = 0;

    struct Case { const char* name; float (*oldf)(); float (*newf)(); };
    const Case cases[] = {
        { "frand",    &old_frand,    &new_frand    },
        { "frand_02", &old_frand_02, &new_frand_02 },
        { "frand_11", &old_frand_11, &new_frand_11 },
    };

    for (const Case& c : cases) {
        mirand_signed = 1; mirand_unsigned = 1;
        for (int i = 0; i < N; ++i) {
            const float a = c.oldf(), b = c.newf();
            ++checked;
            if (!bitsEqual(a, b)) {
                if (++mismatch <= 5) {
                    uint32_t x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4);
                    std::printf("  MISMATCH %s at i=%d: signed=%.9g (0x%08X) unsigned=%.9g (0x%08X)\n",
                                c.name, i, a, x, b, y);
                }
            }
        }
        // The raw state must also agree, not just the derived float.
        if (static_cast<uint32_t>(mirand_signed) != mirand_unsigned) {
            std::printf("  STATE DIVERGED after %s: signed=0x%08X unsigned=0x%08X\n",
                        c.name, static_cast<uint32_t>(mirand_signed), mirand_unsigned);
            ++mismatch;
        }
    }

    std::printf("checked=%lld mismatches=%lld\n", checked, mismatch);
    std::printf("%s\n", mismatch == 0 ? "RESULT: SEQUENCE IDENTICAL" : "RESULT: DIVERGENCE DETECTED");
    return mismatch == 0 ? 0 : 1;
}
