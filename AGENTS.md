# AGENTS.md — Instructions for AI Assistants

This project is a **solo-developer, AI-assisted** native Android (arm64) port of Rigs of Rods. You (the AI agent) are the force multiplier; the human handles what AI is weakest at (GPU driver triage, numerical stability, threading races, netcode/NAT debugging). This file tells you how to work here.

**Read first, in order:** this file → [ROADMAP.md](ROADMAP.md) (current phase + task lists) → [DECISIONS.md](DECISIONS.md) (what's locked, what's open) → [Project Plan.md](Project%20Plan.md) (full research & rationale, when you need the "why").

---

## Project snapshot

- **Project name:** **Trussline** (D-011). Never ship or display the "Rigs of Rods" name or logo — attribution only.
- **What:** Fork of `github.com/RigsOfRods/rigs-of-rods` (C++ / CMake / AngelScript), ported natively to Android arm64-v8a with Vulkan rendering via OGRE 14.x, packaged as a GameActivity (AGDK) app.
- **Upstream base:** master, which pins **OGRE 1.11.6.1**. The 1.11→14.x upgrade is *our* work under the hands-off posture (D-012) and is sequenced **desktop-first** (ROADMAP § 2.0).
- **Reference device:** a single **mid-to-low range** Android phone (D-013) — a harder target than the original plan's midrange baseline. Entry-tier budget is 30 fps at 720p-class. The emulator is for iteration only and produces no valid Gate 1 evidence.
- **Current phase:** Phase 0 complete; Phase 1 underway. **Spike A passed** — OGRE 14.5.2 renders RTSS-shaded geometry via Vulkan on Android (V-8–V-11). Spike B correctness fixes landed (Steps 1–2); solver extraction and the hardware benchmark remain. Gate 1 waits only on the entry-tier phone. *(Update this line as phases complete.)*
- **Crown jewel:** the node/beam soft-body solver (`Actor` physics) — plain C++ mass-spring-damper, fixed 0.0005 s (2 kHz) Euler integration, architecturally separated from graphics (`GfxActor`). It survives any strategy pivot; everything else is replaceable.

## Hard constraints — never violate, never "temporarily" work around

1. **GPLv3 everywhere.** The fork stays GPLv3 with complete corresponding source published. Never add a dependency without a license check (record it in the dependency license manifest). Never vendor code of unknown license.
2. **No community mod content in the repo or in shipped packages.** RoR community vehicles/terrains/sounds are individually licensed by their creators and generally cannot be redistributed. Only RoR's GPLv3 base content or explicitly-permitted content. When in doubt, it's un-shippable.
3. **arm64-v8a + Vulkan only.** arm64-v8a is the sole *shipping* ABI. x86_64 is permitted for emulator development and is currently the primary working target (D-013) — never a release artifact. Do not add armeabi-v7a, a GLES render path, or any Cg.
   - **Cg is a licensing violation, not just a porting nuisance.** Linking a GPLv3 binary against proprietary Cg breaches the GPL unless the system-library exception applies — and Cg is not a system library on Android, where it does not exist at all. So Cg must be *removed*, never worked around, shimmed, or stubbed-but-linked. If you find Cg-dependent code, eliminating it is the task.
4. **Do not alter physics semantics.** The 2 kHz fixed timestep (`PHYSICS_DT 0.0005f`, `SimConstants.h:20`), the **symplectic (semi-implicit) Euler** integrator (`ActorForcesEuler.cpp:1636-1639`), and force-accumulation order are load-bearing (variable timestep historically caused instability). Optimizations (NEON, threading, batching) must be **tolerance-verified** against the reference behavior, with benchmarks — *not* bit-compared: `-ffast-math` is on in every release config and aarch64 has unconditional FMA, so bit-exact x86↔ARM parity is impossible by construction (R-17). Never "improve" the integrator as a side effect of another task.
   - **Related known hazards, do not reintroduce:** `ApproxMath.h`'s float/int type punning is strict-aliasing UB (R-16) — use `memcpy`, never `*(float*)&i`. The project builds at `-std=c++11`, so `std::bit_cast` is unavailable.
5. **The solver stays behind its C-style API.** No OGRE types, globals, or platform types leaking into the solver boundary. This isolation is the project's pivot insurance — treat boundary erosion as a bug.
6. **Desktop build stays green.** Android work must not break the desktop target. Platform divergence goes behind the platform abstraction layer, not ad-hoc `#ifdef`s scattered through logic.
7. **Distinct branding.** Never introduce "Rigs of Rods" name/logo into fork-facing surfaces (app name, store text, UI). Upstream attribution yes; upstream branding no.
8. **No iOS / Apple App Store work.** GPLv3-incompatible. Don't scaffold for it "just in case."
9. **TBDR rules in rendering code:** no classic deferred/G-buffers, minimize render-target switches, `DONT_CARE` load/store on transient attachments, no SSR/SSAO/DOF-class post.

## Phase discipline

- Work only on the current phase's tasks (plus cross-cutting tracks) unless the human explicitly asks otherwise. In particular: **no multiplayer work before Phase 7, no mod-support work before Phase 8** — the ordering is deliberate risk management.
- Gates are pre-committed. Do not soften exit criteria or renegotiate thresholds after seeing results; report the evidence straight and let the human make the gate call (especially Gate 1's Godot pivot).
- When you finish a roadmap task, check it off in ROADMAP.md. When you discover new work, add it under the right phase/workstream rather than doing it opportunistically.

## Division of labor — know your lane

**You are strong at (do these proactively):** CMake/superbuild/CI authoring, JNI/GameActivity glue, `#ifdef`-layer refactoring, dependency porting, shader translation (Cg→GLSL-ES/RTSS), UI layout code, asset-pipeline scripting, documentation, license manifest upkeep.

**You are weak at (flag for the human, don't fake it):**
- GPU driver bugs and per-device rendering anomalies — you can't see the screen or the device. Produce the diagnostic setup (RenderDoc captures, minimal repros, logging); let the human observe and decide.
- Physics numerical stability judgments — propose changes with verification harnesses attached; never assert stability from reading code.
- Threading/race conditions — write the code defensively, request TSAN runs and human review on every sim→render handoff change.
- Control feel, playtesting, thermal behavior, real-network (LTE/CGNAT) behavior — on-device human territory.

When a task hits your weak zone, say so explicitly and hand over a *prepared* experiment rather than a guess.

## Verification rules

- **Don't trust the plan's unverified facts.** Project Plan.md flags several: upstream OGRE-14/Cg completion status, exact RoRnet send rate, presence of SSE intrinsics in the solver, all performance numbers. Check current reality (upstream repo, actual source) before building on any of them, and record findings in DECISIONS.md.
- **Never invent performance numbers.** Framerate/thermal/CPU figures come from on-device measurement only. If you don't have a measurement, write "unmeasured."
- **Benchmark before and after every optimization.** An optimization without numbers is a refactor with risk.
- **Cite sources when updating docs** — upstream issue/PR numbers, commit hashes, device model + driver version for any device-specific claim.

## Engineering conventions

- **Language/build:** C++ matching upstream's standard and style (naming, formatting, comment density — read neighboring code first). CMake for everything; the Android dependency superbuild is the single source of truth for dependency versions; NDK version is pinned (see DECISIONS.md).
- **Platform abstraction:** desktop/Android divergence lives in dedicated abstraction interfaces (input, filesystem/asset access, lifecycle, audio backend). OIS is compiled out on Android behind the input abstraction — never call OIS from shared code.
- **Upstreamability — optional, not obligatory (D-012):** the fork is hands-off toward upstream; nothing you write needs to be mergeable there, and no task waits on upstream. Still keep Android-specific code structurally separate from engine/dependency work: it costs nothing, keeps a future rebase onto upstream cheap, and preserves the option. Reading and borrowing from upstream's public GPLv3 PRs (#3418, #3380, #3431) is encouraged — hands-off means we don't invest in upstreaming, not that we ignore free reference material.
- **Assets:** every asset transformation (mesh re-encode, ASTC compression, LOD gen) goes through the scripted pipeline — no manually-converted binaries committed without their pipeline recipe.
- **Dependencies:** adding one requires (a) license check into the manifest, (b) superbuild integration with pinned version, (c) an entry in DECISIONS.md if it's architecturally significant.
- **Commits/PRs:** small, single-purpose, desktop-CI-green. Note the roadmap task ID/phase in the message body.

## Documentation duties (every session)

- ROADMAP.md checkboxes reflect reality.
- DECISIONS.md gets an entry for anything locked, reopened, or discovered-to-be-wrong.
- RISKS.md updated when a risk materializes, retires, or changes shape.
- Driver-bug log / device matrix / material audit updated when touched.
- Keep `BUILDING-ANDROID.md` in sync with build changes — the build must be reproducible from docs alone.

## Quick reference — the stack (as decided; see DECISIONS.md for status)

| Concern | Choice |
|---|---|
| Renderer | OGRE 14.x, Vulkan RenderSystem (ogre-next = Gate 1 alternative). Upgrade from upstream's 1.11.6.1 is our own work — desktop first |
| Reference device | One mid-to-low range Android phone (model TBD) + x86_64 emulator for iteration only |
| App shell | GameActivity (AGDK), SurfaceView |
| Frame pacing / thermal | Swappy + ADPF |
| Audio | OpenAL Soft mixing → Oboe/AAudio backend sink |
| Input | GameActivity input buffer + Game Controller library (OIS removed) |
| UI | Hybrid: RmlUi *or* native views for front-end (open) + touch-adapted Dear ImGui in-sim |
| Assets | Play Asset Delivery (Play flavor) / first-run download (FOSS flavor); ASTC + ETC2 fallback |
| Multiplayer (Phase 7) | RoRnet message model over ENet or GameNetworkingSockets (open); dedicated relay servers |
| Min platform | Android 8.0+ (ideally 10+), Vulkan-capable, arm64-v8a |
| Distribution | Google Play + F-Droid + direct APK; **never** Apple App Store |
