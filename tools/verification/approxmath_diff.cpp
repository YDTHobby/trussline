// Differential test: old (strict-aliasing-violating) ApproxMath vs new (memcpy).
//
// Verifies the R-16 fix is semantics-preserving AS COMPILED BY THIS TOOLCHAIN.
// The old form is UB, so this cannot prove equivalence in general - that is the
// entire point of removing it. What it does prove is that the reference desktop
// build's behaviour is unchanged, which is what ROADMAP 1.2 Step 1 requires.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

// ---------------- OLD: verbatim from upstream, type punning ----------------
static int mirand_old = 1;

inline float old_frand() {
    unsigned int a; mirand_old *= 16807;
    a = (mirand_old & 0x007fffff) | 0x40000000;
    return (*((float*)&a) - 2.0f) * 0.5f;
}
inline float old_frand_02() {
    unsigned int a; mirand_old *= 16807;
    a = (mirand_old & 0x007fffff) | 0x40000000;
    return (*((float*)&a) - 2.0f);
}
inline float old_frand_11() {
    unsigned int a; mirand_old *= 16807;
    a = (mirand_old & 0x007fffff) | 0x40000000;
    return (*((float*)&a) - 3.0f);
}
inline float old_approx_exp(const float x) {
    if (x < -15) return 0.f; else if (x > 88) return 1e38f;
    else { int i = 12102203 * x + 1064652319; return *(float*)&i; }
}
inline float old_approx_pow2(const float x) {
    int i = 8388608 * x + 1065353216; return *(float*)&i;
}
inline float old_approx_pow(const float x, const float y) {
    float v = x; int i = y * ((*(int*)&v) - 1065353216) + 1065353216; return *(float*)&i;
}
inline float old_approx_sqrt(const float y) {
    float f = y; int i = (((*(int*)&f) - 1065353216) >> 1) + 1065353216; return *(float*)&i;
}
inline float old_approx_invSqrt(const float y) {
    float f = y; int i = 0x5f3759df - ((*(int*)&f) >> 1); return *(float*)&i;
}
inline float old_fast_invSqrt(const float v) {
    float y = v; int i = 0x5f3759df - ((*(int*)&y) >> 1); y = *(float*)&i;
    y *= (1.5f - (0.5f * v * y * y)); return y;
}

// ---------------- NEW: memcpy-based, as committed ----------------
static int mirand_new = 1;

inline float bit_cast_to_float(const int32_t bits) { float f; std::memcpy(&f, &bits, sizeof(f)); return f; }
inline float bit_cast_to_float(const uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof(f)); return f; }
inline int32_t bit_cast_to_int(const float f) { int32_t b; std::memcpy(&b, &f, sizeof(b)); return b; }

inline float new_frand() {
    mirand_new *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_new) & 0x007fffffu) | 0x40000000u;
    return (bit_cast_to_float(a) - 2.0f) * 0.5f;
}
inline float new_frand_02() {
    mirand_new *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_new) & 0x007fffffu) | 0x40000000u;
    return bit_cast_to_float(a) - 2.0f;
}
inline float new_frand_11() {
    mirand_new *= 16807;
    const uint32_t a = (static_cast<uint32_t>(mirand_new) & 0x007fffffu) | 0x40000000u;
    return bit_cast_to_float(a) - 3.0f;
}
inline float new_approx_exp(const float x) {
    if (x < -15) return 0.f; else if (x > 88) return 1e38f;
    else { int i = 12102203 * x + 1064652319; return bit_cast_to_float(static_cast<int32_t>(i)); }
}
inline float new_approx_pow2(const float x) {
    int i = 8388608 * x + 1065353216; return bit_cast_to_float(static_cast<int32_t>(i));
}
inline float new_approx_pow(const float x, const float y) {
    int i = y * (bit_cast_to_int(x) - 1065353216) + 1065353216;
    return bit_cast_to_float(static_cast<int32_t>(i));
}
inline float new_approx_sqrt(const float y) {
    int i = ((bit_cast_to_int(y) - 1065353216) >> 1) + 1065353216;
    return bit_cast_to_float(static_cast<int32_t>(i));
}
inline float new_approx_invSqrt(const float y) {
    int i = 0x5f3759df - (bit_cast_to_int(y) >> 1);
    return bit_cast_to_float(static_cast<int32_t>(i));
}
inline float new_fast_invSqrt(const float v) {
    int i = 0x5f3759df - (bit_cast_to_int(v) >> 1);
    float y = bit_cast_to_float(static_cast<int32_t>(i));
    y *= (1.5f - (0.5f * v * y * y)); return y;
}

// ---------------- harness ----------------
static long long g_checked = 0, g_mismatch = 0;

static bool bitsEqual(float a, float b) {
    uint32_t x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4); return x == y;
}

static void check(const char* fn, float in, float a, float b) {
    ++g_checked;
    if (!bitsEqual(a, b)) {
        if (++g_mismatch <= 10) {
            uint32_t x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4);
            std::printf("  MISMATCH %-18s in=%.9g  old=%.9g (0x%08X)  new=%.9g (0x%08X)\n",
                        fn, in, a, x, b, y);
        }
    }
}

int main() {
    // Sweep positive magnitudes across many decades - the solver feeds these
    // squared lengths, so the interesting domain is (0, large].
    for (double e = -20.0; e <= 20.0; e += 0.05) {
        const float x = static_cast<float>(std::pow(10.0, e));
        check("fast_invSqrt",   x, old_fast_invSqrt(x),   new_fast_invSqrt(x));
        check("approx_invSqrt", x, old_approx_invSqrt(x), new_approx_invSqrt(x));
        check("approx_sqrt",    x, old_approx_sqrt(x),    new_approx_sqrt(x));
        check("approx_pow2",    x, old_approx_pow2(x),    new_approx_pow2(x));
    }
    // Dense linear sweep near unity, where beam forces actually live.
    for (double v = 0.0009765625; v < 4096.0; v *= 1.0009) {
        const float x = static_cast<float>(v);
        check("fast_invSqrt",   x, old_fast_invSqrt(x),   new_fast_invSqrt(x));
        check("approx_invSqrt", x, old_approx_invSqrt(x), new_approx_invSqrt(x));
        check("approx_sqrt",    x, old_approx_sqrt(x),    new_approx_sqrt(x));
    }
    // approx_exp over its guarded domain, including both clamp branches.
    for (double x = -30.0; x <= 100.0; x += 0.001) {
        const float f = static_cast<float>(x);
        check("approx_exp", f, old_approx_exp(f), new_approx_exp(f));
    }
    // approx_pow over a grid of bases and exponents.
    for (double b = 0.01; b < 1000.0; b *= 1.05) {
        for (double p = -8.0; p <= 8.0; p += 0.05) {
            const float bf = static_cast<float>(b), pf = static_cast<float>(p);
            check("approx_pow", bf, old_approx_pow(bf, pf), new_approx_pow(bf, pf));
        }
    }
    // PRNG sequences must match step for step, from the same seed.
    mirand_old = 1; mirand_new = 1;
    for (int i = 0; i < 2000000; ++i) check("frand",    0, old_frand(),    new_frand());
    mirand_old = 1; mirand_new = 1;
    for (int i = 0; i < 2000000; ++i) check("frand_02", 0, old_frand_02(), new_frand_02());
    mirand_old = 1; mirand_new = 1;
    for (int i = 0; i < 2000000; ++i) check("frand_11", 0, old_frand_11(), new_frand_11());

    std::printf("checked=%lld mismatches=%lld\n", g_checked, g_mismatch);
    std::printf("%s\n", g_mismatch == 0 ? "RESULT: BIT-IDENTICAL" : "RESULT: DIVERGENCE DETECTED");
    return g_mismatch == 0 ? 0 : 1;
}
