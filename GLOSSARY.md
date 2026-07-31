# GLOSSARY.md

Shared vocabulary for the project docs. RoR = Rigs of Rods.

## RoR architecture & formats

- **Node/beam solver** — RoR's soft-body physics core: vehicles are networks of *nodes* (dimensionless mass points) joined by *beams* (spring-dampers with a ball-joint model, no angular resistance — structures must be triangulated). Fixed Euler integration at 0.0005 s (2 kHz).
- **Actor / GfxActor** — the physics representation of a vehicle (`Actor`) vs its graphics representation (`GfxActor`); their clean separation is what makes threaded simulation (and any renderer swap) possible.
- **Truck file** — text vehicle definition (`.truck`, `.load`, `.airplane`, `.boat`, `.trailer`); renderer-agnostic, parses unchanged on mobile.
- **terrn2 / OTC** — RoR terrain definition (`.terrn2`) and terrain config (`.otc`); text, renderer-agnostic.
- **Managed materials** — RoR's classic truck shader feature (Nicemetal/PSSM Cg shaders); must be rewritten for the port.
- **RoRnet** — RoR's multiplayer protocol (current version `RoRnet_2.45`): TCP relay model, **10 Hz** update rate, a **40-byte** `VehicleState` control struct + a variable node-position buffer per update, 8192-byte message cap (8176 payload), 64-peer hard cap (≤16 recommended).
- **Half-float node encoding** — how RoRnet compresses vehicle geometry: node 0 is sent as an absolute 3×float32 anchor (12 bytes), every other chassis node as a 3× IEEE binary16 *delta* from node 0 (6 bytes each). Wheel and rim nodes aren't sent at all — the receiver reconstructs them from axis nodes plus one float per wheel.
- **Discardable stream data** — RoRnet's `MSG2_STREAM_DATA_DISCARDABLE` (1045), the message type carrying real-time actor state. The natural unreliable-unsequenced channel in a UDP transport. Note that plain `MSG2_STREAM_DATA` (1044) is *not* safe to treat the same way — it also carries discrete state transitions that must arrive.
- **AngelScript** — embedded scripting language used for RoR mods; portable interpreted bytecode (ARM-fine); sandboxing is the mobile concern.

## Rendering

- **OGRE** — the 3D engine RoR uses. *OGRE 1.x/14.x* keeps the classic API (RoR's target); *ogre-next* (2.3+) is the rearchitected branch (HLMS/PBR, better mobile ceiling, big migration cost).
- **Cg** — NVIDIA's legacy shader language/toolkit (dead since 2012, x86-only, proprietary). Doesn't exist on ARM — every Cg shader must be eliminated. The port's original blocker.
- **RTSS** — OGRE's Real Time Shader System: generates shaders (per-pixel lighting, PSSM shadows) instead of hand-written Cg.
- **PSSM** — Parallel-Split Shadow Maps; cascade count is a major mobile cost knob (1–2 splits on mobile).
- **TBDR** — Tile-Based Deferred Rendering; how mobile GPUs work. Render-target switches flush tiles to main memory (expensive), so: forward rendering, minimal RT switches, `DONT_CARE` load/store, minimal overdraw, no SSR/heavy post.
- **HLMS** — ogre-next's High Level Material System (its PBR material pipeline).
- **ASTC / ETC2** — mobile texture compression formats; ASTC preferred, ETC2 as the universal fallback.
- **ANGLE** — GL-over-Vulkan translation layer; how Android runs OpenGL on newer devices. Only relevant here as a rejected fallback.

## Android

- **NDK / AGDK** — Native Development Kit (C++ toolchain) / Android Game Development Kit (the game-specific libraries below).
- **GameActivity** — AGDK's activity class for C++ games (successor to NativeActivity): SurfaceView rendering, Jetpack-compatible, native input buffer.
- **Swappy** — AGDK Frame Pacing library: vsync alignment and pacing, VRR support.
- **ADPF** — Android Dynamic Performance Framework: thermal-headroom signals + CPU performance hints (`PerformanceHintManager`), used to step quality down before throttling.
- **Oboe / AAudio** — Android low-latency audio libraries; Oboe picks AAudio (8.1+) or OpenSL automatically. Target output for OpenAL Soft's backend.
- **PAD** — Play Asset Delivery: how large game content ships on Play (install-time / fast-follow / on-demand packs; replaces OBB); includes **TCFT** (Texture Compression Format Targeting — per-device ASTC/ETC2 delivery).
- **AAssetManager** — NDK API for reading assets bundled inside the APK.
- **Scoped storage** — Android's restriction on reading arbitrary user directories; mods must live in app-specific storage or come via the Storage Access Framework (SAF).
- **big.LITTLE** — heterogeneous CPU cores (fast "big" + efficient "LITTLE"); the 2 kHz physics thread must stay on big cores.
- **CGNAT** — Carrier-Grade NAT; mobile carriers' shared NAT, frequently symmetric → defeats UDP hole-punching → relay servers instead of P2P.
- **ABI / arm64-v8a** — the 64-bit ARM binary target; this project's only shipping ABI.

## Networking

- **ENet** — lightweight reliable-UDP library; the lean transport candidate.
- **GameNetworkingSockets (GNS)** — Valve's transport: reliable-UDP + encryption + fragmentation + ICE/NAT traversal.
- **Head-of-line blocking** — TCP stalling all subsequent data behind one lost packet; why TCP is wrong for real-time state on lossy mobile links.

## Licensing

- **GPLv3** — RoR's license; fork must stay GPLv3 with complete corresponding source. Publishable on Google Play, **not** the Apple App Store (anti-tivoization vs App Store DRM/ToS).
- **LGPL** — "lesser" GPL on some deps (OpenAL Soft, SocketW, Caelum); satisfiable here but tracked until each dep is removed or verified.
- **Complete corresponding source** — GPLv3's requirement to publish the exact source (including build scripts) for every distributed binary.

## Project shorthand

- **Spike A / Spike B** — Phase 1 throwaway experiments: A = OGRE-on-Android rendering, B = solver-on-arm64 benchmark.
- **Gate 1** — the go/pivot decision after the spikes (native port vs Godot 4 + solver GDExtension).
- **The pivot** — fallback strategy: Godot 4 shell + node/beam solver as a GDExtension behind its C API.
- **Superbuild** — the CMake project that builds every dependency for arm64 with pinned versions.
- **Play flavor / FOSS flavor** — the two build variants: with Play Core/PAD vs fully-FOSS (F-Droid-compliant).
