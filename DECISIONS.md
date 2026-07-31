# DECISIONS.md — Decision Log

Single source of truth for what is **locked**, what is **open**, and what was **verified**. Every gate outcome, strategy choice, and plan-changing discovery gets an entry. Never delete entries — supersede them (mark the old one `SUPERSEDED by D-xxx`).

**Entry format:**

```
## D-NNN: <title>
- Status: LOCKED | OPEN | SUPERSEDED | VERIFIED
- Phase: <when decided / to be decided>
- Decision: <what>
- Rationale: <why — link Project Plan.md section if applicable>
- Revisit trigger: <what evidence would reopen this>
```

---

# Locked decisions

## D-001: Native port (not engine swap, not rewrite)
- Status: LOCKED
- Phase: pre-0 (planning)
- Decision: Port RoR natively (NDK/CMake fork) rather than rebuilding in Godot/Unity or rewriting from scratch.
- Rationale: The goal is *Rigs of Rods* on Android, not a new game; the solver and content formats are the value. Full analysis in Project Plan §3.
- Revisit trigger: **Gate 1 failure** (Spike A can't hold ~30 fps at 1080p-class on midrange within a bounded effort) → pivot to Godot 4 + native solver GDExtension. Also revisit if visual-modernization goals keep fighting OGRE 1.x-era architecture.

## D-002: Renderer target — OGRE 14.x with Vulkan RenderSystem
- Status: LOCKED (pending Gate 1 confirmation)
- Phase: pre-0
- Decision: OGRE 14.x Vulkan is the primary target; ogre-next 2.3+/Vulkan is the evaluated alternative during Spike A only.
- Rationale: Smallest diff from RoR's 1.x API (faster to first pixel), aligned with upstream's own committed path; ogre-next would force a rewrite of all rendering/material code. GLES is explicitly rejected (unmaintained on Android, "horrendous" driver bugs per OGRE team; Android is Vulkan-first going forward).
- Revisit trigger: Spike A shows OGRE 14.x Vulkan/mobile performance inadequate while ogre-next is adequate.

## D-003: arm64-v8a only; Android 8.0+ (ideally 10+); Vulkan-capable devices
- Status: LOCKED
- Phase: pre-0
- Decision: Single ABI (plus optional x86_64 for emulator dev builds). No 32-bit, no GLES fallback — fallback is feature-scaling within Vulkan + device allow/deny list.
- Rationale: Play requires 64-bit since 2019; Vulkan 1.1 required on new 64-bit devices since Android 10; performance floor (midrange 7-series) is 64-bit.
- Revisit trigger: Vulkan driver failures exceeding a small fraction of target devices → reconsider ANGLE/GLES fallback despite OGRE's warnings.

## D-004: AGDK stack for all Android plumbing
- Status: LOCKED
- Phase: pre-0
- Decision: GameActivity (not NativeActivity/hand-rolled JNI), Swappy frame pacing, ADPF thermal/perf hints, Game Controller library, Game Text Input, Memory Advice API, Oboe audio output.
- Rationale: Highest-leverage path for a solo dev; Google strongly recommends GameActivity for C++ games; hand-rolling lifecycle/pacing/thermal is exactly the human-debugging time the project can't afford.
- Revisit trigger: none foreseen.

## D-005: Audio — keep OpenAL Soft, route output through Oboe
- Status: LOCKED
- Phase: pre-0 (implemented Phase 4)
- Decision: Preserve RoR's OpenAL-based mixing/3D-audio layer; replace only the device backend with an Oboe/AAudio sink. Normal low-latency path, not exclusive MMAP.
- Rationale: OpenAL Soft's OpenSL backend is known-problematic on Android; rewriting RoR's layered RPM-crossfade audio would be far more work than wiring a backend. Engine audio isn't rhythm-game latency-critical.
- Revisit trigger: backend integration proves harder than expected, or unacceptable latency/glitches on target devices.

## D-006: Input — remove OIS entirely on Android
- Status: LOCKED
- Phase: pre-0 (implemented Phases 2/5)
- Decision: OIS compiled out behind an input abstraction; GameActivity input buffer + Game Controller library on Android.
- Rationale: OIS is desktop-centric (keyboard/mouse/joystick); touch and Android gamepad handling need platform-native paths.
- Revisit trigger: none.

## D-007: Multiplayer — keep RoRnet message model, replace TCP transport with reliable-UDP; relay-server topology
- Status: LOCKED (transport library choice still OPEN, see D-102)
- Phase: pre-0 (implemented Phase 7)
- Decision: Positions/state as unreliable-unordered, control/events reliable; small dedicated relay servers (8–16 players); no P2P as primary; no desktop RoRnet 2.44/2.45 compatibility.
- Rationale: TCP head-of-line blocking is wrong for lossy mobile links; CGNAT defeats hole-punching often enough that relays are needed anyway; upstream lead has expressed intent to move to ENet, so this aligns. Quadratic server upload keeps populations small.
- Revisit trigger: none for the model; library choice at D-102.

## D-008: Distribution — Google Play + F-Droid + direct APK; never Apple App Store
- Status: LOCKED
- Phase: pre-0 (implemented Phase 9)
- Decision: Android-only distribution; separate Play flavor (Play Core/PAD) and FOSS flavor (no proprietary deps); full source published per release.
- Rationale: GPLv3 is publishable on Play but conflicts with App Store DRM/ToS (FSF position); F-Droid requires the FOSS flavor.
- Revisit trigger: none while the codebase is GPLv3 (i.e., effectively never).

## D-009: Solver isolated behind a C-style API from day one
- Status: LOCKED
- Phase: 1 (task 1.3)
- Decision: The node/beam solver is consumed only through a clean C API; no OGRE/platform types cross the boundary.
- Rationale: Pivot insurance — the solver is the one asset preserved across any strategy change (OGRE / ogre-next / Godot).
- Revisit trigger: none.

## D-010: Content policy — GPLv3 base content only; no community mods shipped
- Status: LOCKED
- Phase: all
- Decision: Ship a small license-clean base content set; community content only via user-side mod support (Phase 8) and only served with redistribution rights.
- Rationale: RoR community content is individually licensed and generally non-redistributable — the project's biggest legal trap.
- Revisit trigger: explicit written per-creator permission for specific content.

## D-011: Fork name — **Trussline** *(was D-101)*
- Status: LOCKED
- Phase: 0.2 (2026-07-31)
- Decision: The fork is named **Trussline**. Name references the triangulated-truss structure the beam model requires. No use of "Rigs of Rods" name or logo anywhere; upstream credited by attribution only.
- Rationale / availability evidence (checked 2026-07-31): GitHub org/user `trussline` returns 404 (available). No trademark records found for "Trussline"; existing `TRUSS` marks belong to [Truss App, Inc.](https://trademark.justia.com/owners/truss-app-inc-4358733) (private social networking) and Truss Holdings (real estate) — different goods/services classes, and "Trussline" is a distinct composite mark.
- Outstanding: reserve the GitHub org, decide the Play package id (`org.trussline.*` suggested — **hard to change after first Play listing**), and have a proper USPTO/EUIPO search done before any store listing. Preliminary signals are clean but this is not legal advice.
- Revisit trigger: a formal trademark search turning up a conflict in software/game classes.

## D-012: Upstream posture — hands-off; fork master, own the OGRE upgrade
- Status: LOCKED
- Phase: 0.1 (2026-07-31)
- Decision: Fork from upstream `master` (OGRE 1.11.6.1) and perform the OGRE 1.11→14.x upgrade **in-fork**. No commitment to contribute work back upstream, no dependency on [PR #3418](https://github.com/RigsOfRods/rigs-of-rods/pull/3418) landing. Upstream PRs remain readable public GPLv3 code and may be **mined as reference implementations** — hands-off means we don't invest in upstreaming, not that we ignore their published work.
- Rationale: maximum control and independence from upstream's schedule; no blocking on a third-party branch that may be force-pushed or rewritten.
- Consequences (accepted): the 1.11→14 upgrade is now owned work (see ROADMAP § 2.0, desktop-first); permanent fork maintenance burden is higher; risk of duplicated effort rises (RISKS.md R-13 raised to High, new R-15 added).
- Revisit trigger: if the in-fork OGRE upgrade drags badly, reconsider rebasing onto #3418's merged result once it lands upstream.

## D-013: Reference device tier — mid-to-low range (harder than the original baseline)
- Status: LOCKED (device model pending identification)
- Phase: 0.3 (2026-07-31)
- Decision: The primary reference device is a **mid-to-low range** Android phone, not the Snapdragon 7-series "midrange" the original plan assumed. Performance budgets are recalibrated accordingly (ROADMAP § Cross-cutting E): entry tier targets 30 fps at **720p-class** render resolution with reduced feature set; the original 1080p/30 midrange target moves up a tier. The x86_64 emulator is available as a secondary target for build/iteration only.
- Rationale: designing against the actual hardware in hand. A tougher floor is strategically useful — it forces honest budgets early and widens the shippable device range — but it makes Gate 1's Spike A a harsher test than originally scoped.
- Caveat: **the emulator cannot validate Vulkan driver behavior, thermals, or frame pacing.** Gate 1 evidence must come from physical hardware.
- **Amended 2026-07-31: emulation-first for now.** The reference phone is plugged into another machine and unavailable, so the **x86_64 emulator is the primary development target** until it returns. Installed images: `android-33/35/36/36.1` (x86_64); AVD `Pixel_7_Auto`. The superbuild accepts `TL_ANDROID_ABI=x86_64` with a loud non-shippable warning rather than failing.
- **What this does and does not unblock.** The emulator validates the *functional* half of Spike A — that OGRE 14 builds, initializes its Vulkan RenderSystem, compiles shaders, renders correctly, and survives the Android lifecycle. That is real de-risking and worth doing now. It validates **none** of the performance half: no Vulkan driver quirks on real GPUs, no thermal throttling, no frame pacing, no sustained framerate. **Gate 1 cannot be closed on emulator evidence** — the framerate thresholds are meaningless against a host GPU.
- Outstanding: identify the exact device (SoC, GPU, Android version, Vulkan driver) once it is back and ADB connects, and record it in the device matrix.
- Revisit trigger: acquisition of a higher-tier device, or measurements showing the entry tier is not viable at all (then the floor rises and the device becomes a deny-list case study).

## D-014: Toolchain pins *(was D-104)*
- Status: LOCKED (for the pinned items); two prerequisites still missing
- Phase: 0.3 (2026-07-31)
- **NDK: `27.3.13750724`** — the latest patch of the **r27 LTS** line. Installed and verified at `%LOCALAPPDATA%\Android\Sdk\ndk\27.3.13750724`, toolchain file present.
  - Rationale: r28/r29 are stable and r30 is in RC, but the superbuild compiles several older vendored libraries (Caelum, PagedGeometry, zziplib, the OGRE-coupled forks) where newer toolchains break things more often than they fix them. Pinning a mature LTS is the low-risk default. Moving to a newer LTS is cheap **before** Phase 2 locks the dependency set — revisit then, not later.
  - ⚠️ The CI workflow uses `nttld/setup-ndk`, which takes `rNNx`-style names. The mapping from `27.3.13750724` to its r-name is **unconfirmed** — verify before the Android CI job goes blocking (Phase 2.4).
- **Conan: 2.31.1** — installed via pip (user site).
- **Already present and adequate:** git 2.51.2 · CMake 3.29.2 (meets the superbuild's 3.24 floor) · Ninja · Python 3.13.5 · Android Studio · Android SDK with platforms 30/35/36, build-tools to 36.1.0, cmdline-tools `latest` · ADB (note: two copies exist, `C:\platform-tools` and the SDK's — harmless, but pin one on PATH).
- **JDK: `17.0.20` (Temurin)** — installed 2026-07-31. ⚠️ The installer left `JAVA_HOME` pointing at the old **JRE 8**; corrected to `C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot` at User scope. Both runtimes remain on disk — if Gradle ever reports a Java 8 toolchain, this regressed.
- **MSVC: Visual Studio Build Tools 2022, v17.14.37516.0** — verified to carry the `VC.Tools.x86.x64` component. Two other installs exist and should **not** be used: Build Tools 2019 (16.11, also has VC++ but is older than upstream's CI baseline) and Visual Studio Community 2026 (18.7, which does *not* carry the VC++ toolset). Pin 2022 so local and CI toolsets match — upstream's own CI builds on MSVC 2022.
- **Environment variables set (User scope, 2026-07-31):** `JAVA_HOME` → JDK 17 · `ANDROID_HOME` → `%LOCALAPPDATA%\Android\Sdk` · `ANDROID_NDK_HOME` → the pinned NDK. New shells pick these up; existing ones need a restart.
- **Still outstanding:** ADB cannot see the phone (see ROADMAP § 0.3) — this is the only remaining Phase 0 blocker, and it gates all Gate 1 evidence.

---

# Open decisions (lock by the phase noted)

## D-101: Fork name & branding
- Status: **LOCKED → see [D-011](#d-011-fork-name--trussline-was-d-101) (Trussline, 2026-07-31)**

## D-102: Multiplayer transport library — ENet vs GameNetworkingSockets
- Status: OPEN — lock at Phase 7.1 start
- **Lean flipped to GameNetworkingSockets on 2026-07-31** (was ENet). Reason: the V-3 audit shows a real-time actor packet exceeds a typical ~1200-byte UDP MTU at roughly **170+ chassis nodes** — well within normal vehicle sizes. **ENet cannot fragment unreliable packets** (`ENET_PACKET_FLAG_UNSEQUENCED` is incompatible with fragmentation), so large actors would have to go reliable — reintroducing exactly the head-of-line blocking the transport swap exists to eliminate. GNS fragments unreliable messages natively up to 512 KB, and brings encryption and ICE/NAT traversal as bonuses.
- ENet remains viable **only** if vehicle complexity caps (Phase 4/8) land low enough to keep actors under MTU — in which case its simplicity wins. Decide with the actual caps in hand.

## D-103: Front-end UI stack — RmlUi vs native Android views (hybrid with ImGui in-sim either way)
- Status: OPEN — lock at Phase 5.3 start
- Trade: RmlUi = skinnable, stays in the C++/render world, consistent across surfaces. Native views = platform feel, system IME, accessibility, scroll physics; awkward for in-game.

## D-104: NDK version pin
- Status: **LOCKED → see [D-014](#d-014-toolchain-pins-was-d-104) (NDK 27.3.13750724, 2026-07-31)**

## D-105: Godot pivot criteria interpretation
- Status: PRE-COMMITTED (thresholds), outcome OPEN until Gate 1
- Thresholds are written in ROADMAP.md Gate 1 and must not be renegotiated post-hoc.

## D-106: First-release scope — ship v1 single-player-only?
- Status: OPEN — decide by end of Phase 6
- A single-player v1 before Phases 7/8 complete is a legitimate option; record the call and its rationale here.

## D-107: Monetization model
- Status: OPEN — lock in Phase 9
- **Clarified 2026-07-31 by [LEGAL.md](LEGAL.md):** selling the app is **unambiguously permitted** — GPLv3 expressly allows commercial sale, and RoR has been GPLv3 since its first publication (8 Feb 2009). The constraint is not "may you charge" but "what stays exclusive": anyone who receives a binary may redistribute both it and the source, so paid-app models leak by design. Value has to live in content, convenience, and service — not code secrecy.
- **Permanently foreclosed:** any closed-source or proprietary-relicensed model. There is no CLA/DCO, the AUTHORS list is admittedly incomplete, and the SVN→Mercurial→Git migration makes authorship unreconstructable — relicensing would need agreement from hundreds of untraceable contributors. If the business model ever requires closed source, the answer is **NO-GO**, not "renegotiate."
- Also binding: Play's **repetitive-content policy** forbids reuploading someone else's app without adding original value. Mobile UI, curated original content, and mobile optimization are a defensible answer; a near-verbatim reupload is not.
- Candidates: free + donations · owned original content · paid hosted servers · paid app with source published.

---

# Verification ledger (facts the plan flags as unconfirmed — fill in as checked)

| # | Claim to verify | Where to check | Status | Finding |
|---|---|---|---|---|
| V-1 | Upstream OGRE-14 upgrade / Cg removal completion status | `RigsOfRods/rigs-of-rods` master, issue #1412, recent PRs | ☑ **verified 2026-07-31** | **Not landed, but actively in progress.** Master (2026.01, active) pins `ogre3d/1.11.6.1` via Conan — upstream is on OGRE **1.11**, not 1.9 as the plan assumed (smaller gap to 14). **[PR #3418](https://github.com/RigsOfRods/rigs-of-rods/pull/3418) "Minimal viable OGRE upgrade to 14.5 (no RTSS, same old Cg, same old PSSM)"** opened Apr 2026, **updated Jul 25 2026** — live right now. Older `ogre-13`/`ogre-14` branches are dormant reference material (last real work Sep 2022; 45 ahead / 742 behind master). Cg **toolkit** left the build deps long ago ([#1412](https://github.com/RigsOfRods/rigs-of-rods/issues/1412) closed *completed* 2018-11-13; no Cg in conanfile or DEPENDENCIES.md) — but PR #3418's "same old Cg" confirms **Cg shader content is still in use**, so the fork's material audit + Cg elimination (ROADMAP 3.2) remains required for ARM. Related: [PR #3380](https://github.com/RigsOfRods/rigs-of-rods/pull/3380) replaces Caelum with SkyX explicitly to "ditch Cg"; [PR #3431](https://github.com/RigsOfRods/rigs-of-rods/pull/3431) explores SDL (input — relevant to our OIS removal). **Plan impact:** track/assist #3418 instead of doing a cold 1.11→14 port in-fork (upstream-first principle); re-check at every phase boundary. |
| V-2 | Solver contains no SSE intrinsics (portable scalar C++) | `Actor.cpp` + force-calculation loops | ☑ **verified 2026-07-31** | **CONFIRMED — zero SIMD intrinsics repo-wide.** No `xmmintrin`/`immintrin`/`__m128`/`_mm_`/`__builtin_ia32`, no inline asm, no `_controlfp`, no `__rdtsc`, no `#ifdef _WIN32` inside `source/main/physics/` at all. `Ogre::Vector3` is three bare floats (OGRE's own SSE lives in renderer-side `OgreOptimisedUtilSSE.cpp`, never touched by the solver). **ARM port of the math is a recompile; NEON is pure upside, not a prerequisite.** Timestep confirmed: `PHYSICS_DT 0.0005f` at `SimConstants.h:20` — a `#define` used at ~70 sites. Integrator is **symplectic (semi-implicit) Euler**, `ActorForcesEuler.cpp:1636-1639`. **But three portability hazards were found — see R-16/R-17 and ROADMAP § 1.2:** (a) `ApproxMath.h:101-132` implements `fast_invSqrt`/`approx_sqrt`/`approx_pow` via strict-aliasing-violating `*(float*)&i` punning, called on the hot path (`ActorForcesEuler.cpp:1220,1479,1647`); (b) `-ffast-math` is on in **every** release config (`CMakeLists.txt:89,114-116`) with no `-ffp-contract` setting, and aarch64 has unconditional FMA — so ARM results will be bit-different from x86 by construction; (c) `ApproxMath.h:28` `static int mirand` is a header-scoped mutable global mutated from inside the parallelized per-actor task — a pre-existing data race, plus signed-overflow UB at `:35`. Also noted: project builds at `-std=c++11` (no `std::bit_cast`), and a POD `RoR::Vec3` already exists at `source/main/utils/Vec3.h` as the starting point for the C-API vector shim. |
| V-3 | RoRnet network send rate (Hz) and per-node byte encoding | source, `RoRnet.h` / developer.rigsofrods.org | ☑ **verified 2026-07-31** | **Send rate CONFIRMED: 10 Hz, hard-coded.** `Actor.cpp:2030-2033` early-returns if `< 100` ms since last update — no CVar, no accumulator; actual rate is `min(framerate, 10)` and jitters by a frame period. Characters likewise (`Character.cpp:449`). **Node encoding is NOT float32 triplets — it's a hybrid:** node 0 absolute as 3×float32 (12 B), every other chassis node as 3× **IEEE binary16 half-float deltas relative to node 0** (6 B/node, `Actor.cpp:2128-2137`), wheels 4 B each (rotation only — wheel/rim nodes are never transmitted, they're reconstructed from axis nodes), plus `ceil(K/8)` prop-anim bits. Formula at `ActorManager.cpp:293-307`. Per packet: `16 (header) + 40 + 12 + 6(N-1) + 4W + ceil(K/8)`. Worked: 100 nodes/6 wheels = 670 B/packet = 6.7 KB/s ≈ 54 kbit/s at 10 Hz — consistent with the documented 64 kbit/s per-client budget, so the quadratic upload math in the plan holds. **Corrections to the plan:** `VehicleState` is **40 bytes, not ~44** (`RoRnet.h:207-219`, 10 × 4-byte fields under `pack(1)`); `RORNET_MAX_MESSAGE_LENGTH` 8192 is header+payload, so max payload is 8176 → ceiling ≈1350 chassis nodes; **`RORNET_LAN_BROADCAST_PORT` is dead code** — defined but referenced nowhere in the client, so there is no UDP LAN discovery to port (server discovery is HTTP against the master server); there is **no default game port** (client CVar has no default; server picks randomly in 12000–12500). Also found: the size guard at `Actor.cpp:2036` omits the 16-byte header and calls `exit(126)` on failure — fix both when rewriting. |
| V-4 | State of RoR forks of Caelum / PagedGeometry / MyGUI (OGRE version lock) | respective repos | ☑ **verified 2026-07-31** | Conan pins on master: `ogre3d-caelum/0.6.3.1`, `ogre3d-pagedgeometry/1.2.0`, `mygui/3.4.0` (AnotherFoxGuy recipes), `socketw/3.11.0`, `ois/1.4.1`, `angelscript/2.35.1`, `openal-soft/1.24.3` — all built against OGRE 1.11.6.1. Org fork `ogre-caelum` stale (last touched Jan 2020; upstream plans to drop it for SkyX per PR #3380 — aligns with our Phase 6 sky replacement); `ogre-pagedgeometry` updated Jul 2024; no org MyGUI fork (Conan recipe only). **Bonus find:** [`ror-dependencies`](https://github.com/RigsOfRods/ror-dependencies) — upstream's CMake meta-project for building all deps, actively maintained (May 2026) — strong candidate base for the Android superbuild (ROADMAP 2.1). |
| V-5 | All performance/thermal targets | on-device measurement (Spikes A/B, Phase 4/6) | ☐ unmeasured | — |
| V-7 | Runtime render configuration of the shipping build | `redist/plugins.cfg` from our own green desktop build | ☑ **verified 2026-07-31** | Read from the built artifact, not source. Loaded plugins: `Codec_FreeImage`, **`RenderSystem_Direct3D9`**, `Plugin_ParticleFX`, `Plugin_OctreeSceneManager`, **`Plugin_CgProgramManager`**, `Caelum`. **Two findings.** (1) Cg is confirmed a *runtime* dependency of the shipping build, not merely a build-time one — closing the loop on V-6 with direct evidence. (2) **The shipping desktop build renders through Direct3D 9.** `RenderSystem_GL` and `GL3Plus` are present but **commented out**, and D3D11 too. This is more consequential than it looks: the Android port is not "swap the GL backend for Vulkan" — there is no exercised GL path in the shipping configuration either, so the render backend is being replaced wholesale rather than migrated. It raises, not lowers, the value of Spike A as the project's make-or-break test, and it means desktop-vs-Android visual comparisons will differ by backend as well as by API. Also absent: any RTSS initialization, consistent with V-6. |
| V-6 | Cg/material inventory and Cg-elimination scope | `resources/**`, terrain + Hydrax generators | ☑ **verified 2026-07-31** | **Two corrections to the Project Plan.** (1) **RTSS is NOT in use.** The plan says RoR "ships two shader sets… and OGRE's RTSS" — in fact `resources/rtshader/` is dormant dead content: `ContentManager::ResourcePack::RTSHADER` is declared but never passed to `AddResourcePack()`, there is no `ShaderGenerator::initialize()` or `addSceneManager()` call anywhere, and `plugins.cfg.in` loads `Plugin_CgProgramManager` — **Cg is a hard runtime dependency today**. Good news in disguise: there is no legacy RTSS integration to unwind, so RTSS can be adopted cleanly from modern OGRE. (2) **The `.cg` file count understates the work badly.** 24 `.cg` files exist, but 9 are deletable RTSS library copies and 2 are near-duplicate nicemetal twins (~13 real files, ~68 declared Cg programs) — while **~229 KB of C++ generates shader source as strings and never appears in any file count**: `OgreTerrainPSSMMaterialGenerator.cpp` (62 KB) and `gfx/hydrax/MaterialManager.cpp` (167 KB). **Favorable dependency structure:** one file, `managed_materials/shadows/pssm/on/pssm.cg`, gates ~350 otherwise-fixed-function material definitions (nearly everything inherits `RoR/Managed_Mats/Base` → `Shadows/managed/base_receiver`), and the codebase **already ships the disable path** at `shadows/pssm/off/` with empty passes. That enables the staged port in ROADMAP § 3.2. **SkyX is already pure GLSL** (14 shaders, 63 program declarations, zero Cg) needing only GLSL 1.x → ES 3.x modernization — which makes the Phase 6 sky replacement much cheaper and aligns with upstream PR #3380. Base content is a git submodule (`RigsOfRods/content`) containing just 3 `.material` files and no shaders. |
