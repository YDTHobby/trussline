# tools/verification

One-off differential tests that back specific claims in [DECISIONS.md](../../DECISIONS.md) and [RISKS.md](../../RISKS.md). These are not part of the build and are not run by CI; they exist so a claim like "this change is semantics-preserving" is reproducible rather than asserted.

## `approxmath_diff.cpp` — R-16, the strict-aliasing fix

`source/main/physics/ApproxMath.h` used to reinterpret bit patterns with `*(float*)&i` casts, on the solver's hottest path (`ActorForcesEuler.cpp:1220,1479,1647`, `ActorManager.cpp:1664`). That is a strict-aliasing violation — undefined behaviour. MSVC effectively never exploits strict aliasing, so it was harmless in practice on the desktop build; GCC and Clang on aarch64 exploit it aggressively, which made it a latent miscompile waiting for the Android port.

The fix replaces the casts with `memcpy` (`std::bit_cast` would be the modern spelling, but the project builds as C++11). This test compiles **both** implementations into one binary and compares them bit-for-bit.

Build and run it from a Developer Command Prompt:

```bash
cl /nologo /O2 /fp:fast /EHsc approxmath_diff.cpp /Fe:diff_o2.exe && diff_o2.exe
```

Coverage: `fast_invSqrt`, `approx_invSqrt`, `approx_sqrt`, `approx_pow2`, `approx_exp` (including both clamp branches), `approx_pow` over a base/exponent grid, and 2,000,000 iterations of each of the three `frand` variants seeded identically.

**Result on the reference toolchain (MSVC 19.44, 2026-07-31): 6,259,812 comparisons, 0 mismatches, at both `/O2 /fp:fast` and `/Od`.**

### What this does and does not prove

It proves the desktop reference build's behaviour is unchanged by the fix — which is what ROADMAP § 1.2 Step 1 requires, and why the fix is done on x86 *before* ARM exists to confound results.

It cannot prove equivalence in general, because the old code was undefined behaviour: a compiler is entitled to do something different tomorrow, on another target, or at another optimisation level. That is precisely the reason to remove it rather than rely on it continuing to work.
