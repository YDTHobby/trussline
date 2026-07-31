# Forking Rigs of Rods to Android: A Technical Porting and Project Plan

## TL;DR
- **A native NDK/CMake fork of Rigs of Rods (RoR) to arm64 Android is technically feasible but gated on one hard prerequisite the RoR team itself has identified: the OGRE 1.9→OGRE 14 upgrade and removal of the legacy NVIDIA Cg toolkit.** Until that is done (upstream or in your fork) there is no supported Android render path, because OGRE 1.9 has no maintained GLES3/Vulkan Android backend and Cg does not exist on ARM. This is the single highest-risk unknown and must be de-risked first.
- **The soft-body "node/beam" solver is the crown jewel and the most portable part** — it is plain C++ math (mass-spring-damper, fixed 2 kHz Euler integration), cleanly separated from rendering (`Actor` physics vs. `GfxActor` graphics), and will run on ARM after recompilation; the real work is performance tuning (NEON, big.LITTLE threading, vehicle complexity budgets), not a rewrite. Rendering, UI (MyGUI), input (OIS), and the shader/material content are where most of the porting effort and visual-modernization work actually live.
- **Do the port in dependency-ordered phases, de-risking rendering and physics framerate with tiny spike apps before committing.** Mobile-only multiplayer should come late and almost certainly needs replacing RoRnet's TCP/SocketW transport with reliable-UDP (ENet or GameNetworkingSockets); GPLv3 is fine on Google Play but blocks the Apple App Store, so plan Android-first distribution via Play + F-Droid/direct APK.

## Key Findings

1. **Codebase**: RoR is C++ (with AngelScript for mods), built with CMake, dependencies managed via Conan. Latest stable is **2026.01 (released 3 January 2026** — per the official announcement, "three long years in the making"). Active maintainers include Petr Ohlidal (ohlidalp), CuriousMike56, AnotherFoxGuy, MarkROR. It still uses **OGRE 1.x (historically 1.9, with in-progress work toward OGRE 14)** plus OIS, MyGUI, Caelum, PagedGeometry, OpenAL Soft, AngelScript, MySocketW/SocketW, curl. The official position, from the locked Rigs of Rods Community thread "Is it possible to play on Android?" (Mar 2025), is verbatim: *"No, it's not currently possible to bring any RoR version to another platform. The possibility will only open up once the OGRE 14 upgrade is complete (and we ditch Cg shaders), however this likely won't happen any time soon."*
2. **OGRE on Android**: OGRE 1.9 (what RoR uses) has no maintained mobile backend. Two viable targets exist: **OGRE 1.x (14.x) with the backported Vulkan RenderSystem (since OGRE 13.2)**, or **ogre-next (OGRE 2.3+) with Vulkan + preliminary Android support**. Both require Vulkan-capable devices, Android 8.0+ strongly recommended due to driver bugs. GLES3 on Android is explicitly de-prioritized by the OGRE team ("the amount of driver bugs in Android was HORRENDOUS").
3. **Physics**: node/beam solver is mass-spring-damper with fixed Euler integration at a 0.0005 s (2 kHz) internal timestep since ~0.4.6; historically "single-and-half-threaded" (mostly single-threaded with some parallelized force loops). Physics/graphics are architecturally separated, enabling threaded simulation.
4. **Multiplayer (RoRnet)**: TCP via SocketW, relay-server model, 64-peer hard cap (10–16 recommended), per-client budget ~64 kbit/s with **quadratic upload growth** at the server. Each actor update = ~44-byte control struct + variable node-position buffer capped at 8192 bytes/packet. TCP over lossy mobile links is the core problem.
5. **Licensing**: Code is GPLv3. **GPLv3 is publishable on Google Play but not the Apple App Store** (anti-tivoization vs. App Store DRM/ToS). RoR *content* (vehicles/terrains) is separately licensed by individual creators and mostly cannot be redistributed.

## Details

### 1. Current State of the Rigs of Rods Codebase

**Repository & language.** Main repo: `github.com/RigsOfRods/rigs-of-rods` (~1.2k stars, ~204 forks). Primary language C++; AngelScript for in-game scripting/mods. Build system is **CMake**, with dependencies fetched via **Conan** (the buildsystem overhaul happened in PR #2774; Conan V2 migration in PR #2965). There is a `DEPENDENCIES.md` and per-platform Compile wiki pages. Stable release **2026.01** shipped 3 January 2026; the project is actively developed (recent PRs on tuning UI, terrn2 AI presets, OGRE script bindings). Active contributors: Petr Ohlidal (ohlidalp, lead), CuriousMike56, AnotherFoxGuy (build/deps), MarkROR.

**Core dependencies and their licenses** (from `DEPENDENCIES.md`):

| Library | Purpose | License | Android status |
|---|---|---|---|
| **OGRE** | 3D rendering | MIT | 1.9 unsupported on Android; needs 14.x (Vulkan) or ogre-next |
| **OIS** | input | zlib/libpng | Desktop-centric (keyboard/mouse/joystick); replace with touch/GameActivity input |
| **MyGUI** | GUI | MIT | Builds on OGRE; not touch-first; likely replace/augment |
| **Caelum** | sky | LGPLv3 | OGRE 1.x plugin; LGPLv3 has implications (see licensing) |
| **PagedGeometry** | vegetation | zlib/libpng | OGRE 1.x plugin; RoR maintains its own fork (updated to 1.2.2) |
| **OpenAL Soft** | 3D audio | LGPLv2 | Builds on Android but has known latency/quirk issues; Oboe backend requested |
| **AngelScript** | scripting | zlib | Portable C++; compiles for ARM (used deferred) |
| **MySocketW/SocketW** | sockets | LGPLv2.1 | TCP stream sockets; portable but transport is the wrong choice for mobile |
| **curl** | online services | MIT | First-class Android support |
| **mofilereader** | i18n | MIT | Portable |
| **UTFCpp / RapidJSON** | utility | BSL-1.0 / MIT | Portable |

RoR historically depended on the **legacy NVIDIA Cg Toolkit** (last updated 2012, proprietary, x86-only). This is a hard blocker for ARM/Android: the terrain system (`OgreTerrain`) and the "managedmaterials" truck shader feature use generated Cg shaders. Upstream tracks this in issue #1412 ("Remove nvidia-cg-toolkit"). RoR ships two shader sets: the classic **PSSM/Nicemetal.cg** managed materials, and OGRE's **RTSS (RealTimeShaderSystem)**. The lead dev has stated that migrating to a modern renderer would grant per-pixel lighting and PSSM3 shadows "for free" via RTSS, and that **the real visual bottleneck is assets (materials, shaders, textures), not the engine**.

**The soft-body physics core.** Vehicles are networks of dimensionless **nodes** (mass points) connected by **beams** (spring-dampers with a ball-joint model — no angular resistance, so structures must be triangulated). Forces (beams + gravity + aero + buoyancy + user input) are accumulated and applied in two phases per step; simulation uses **fixed Euler integration at 0.0005 s (2 kHz)** since ~v0.4.6 (variable timestep before that caused instability). Architecturally, **`Actor` (physics) is cleanly separated from `GfxActor` (graphics)** — the key enabler for running physics on its own thread. Per the lead dev, the codebase is "single-and-half-threaded" today (single-threaded with selectively parallelized force calculations); the stated goal is "physics running in parallel with rendering… single threadpool, physics tasks inserted at fixed rate," copying sim→render state at frame boundaries. There is no confirmation of heavy SSE/SSE2 intrinsic use in the solver — it appears to be portable scalar/float C++, which means **ARM porting is a recompile, and NEON vectorization is an optimization opportunity rather than a prerequisite**. (Marketing text historically mentioned "dual-core… multithreading and CUDA support," but CUDA is not a load-bearing dependency of the current sim.)

**Prior port attempts / lessons.** There is no successful native mobile port. RoR community consensus (forum threads "Is it possible to play on Android?", locked Mar 2025) is that a port is blocked until the OGRE 14 upgrade + Cg removal land. A "Rigs of Rods – Forums" Android app exists but is just a forum reader ("not the game itself… we are working on that"). Community suggestions to date are workarounds (Winlator/Mobox x86 emulation, or PC game-streaming via Moonlight/Sunshine), not ports. A macOS port attempt (issue #2981) got as far as compiling against OGRE 13 but segfaulted at startup and is tagged "Requires OGRE upgrade (version 14+)." **Lesson: the dependency modernization is the real project; the "port" is downstream of it.** BeamNG.drive, founded by ex-RoR developers, is a proprietary from-scratch commercial engine and is not a reusable reference.

### 2. Recommended Native Android Porting Path

**Toolchain & app shell.** Use **Android Studio + NDK + CMake** (RoR is already CMake, so the `externalNativeBuild` path is natural). For the app shell, use **GameActivity** (from AGDK/Jetpack) rather than bare NativeActivity or a hand-rolled Java+JNI shell. Google "strongly recommends GameActivity for new games and C/C++ intensive applications"; it renders into a `SurfaceView` (easy to overlay Android UI), derives from `AppCompatActivity` (Jetpack components available), integrates GameTextInput, and ships as a static library (v1.2.2+) via Prefab. This is the best fit because you will want native Android views/IME for menus and text entry over the GL/Vulkan surface.

**AGDK components worth adopting:**
- **GameActivity** — app lifecycle, input buffer, surface.
- **Android Frame Pacing (Swappy)** — correct frame pacing / VSync alignment; essential for smooth presentation and reduced jank on variable-refresh displays.
- **ADPF (Android Dynamic Performance Framework)** — thermal headroom hints + CPU performance hints (`PerformanceHintManager`); lets you scale quality down before thermal throttling wrecks framerate. Directly relevant to your midrange-to-flagship target.
- **Game Controller library** — Bluetooth/USB gamepad support with hotplug/reconnect handling.
- **Game Text Input** — software keyboard from C++.
- **Memory Advice API** — proactively avoid the low-memory killer on midrange devices.
- **Oboe** — audio (see below).

**ABI targeting.** Build **arm64-v8a** as the primary and effectively only target. armeabi-v7a (32-bit) is not worth the effort for a new 2026 title: Google Play has required 64-bit since 2019, Vulkan 1.1 is required on new 64-bit devices since Android 10, and your performance floor (midrange 7-series) is 64-bit. Skip x86/x86_64 unless you specifically want emulator/Chromebook testing (an x86_64 build is handy for the Android emulator during development).

**Building the dependency stack.** RoR's Conan setup targets desktop; Android cross-compiles are not currently provided (the RoR Conan maintainer noted Android support as a "would like to" future item). Practical approach:
- Use **OGRE's own Android build path** as the template. ogre-next provides an official Android guide (`ogre-next-deps`, NDK toolchain, arm64-v8a, `android.toolchain.cmake`). OGRE 14.x similarly builds for Android with Vulkan.
- Build each dependency with the **NDK CMake toolchain** (`-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26` or higher). Prefer building by hand / with a small superbuild CMake project over fighting vcpkg/Conan Android triplets, because several RoR deps (Caelum, PagedGeometry, MyGUI) are OGRE-version-locked forks not in public package registries. vcpkg does have Android triplets and an OGRE port and can handle curl/OpenAL/AngelScript; use it for the leaf libraries and build the OGRE-coupled plugins manually against your chosen OGRE.
- Expect to **fork Caelum, PagedGeometry, and MyGUI** to compile against your target OGRE and to strip Cg.

**Rendering backend decision.** Recommendation: **target Vulkan via OGRE 14.x's Vulkan RenderSystem, with ogre-next/Vulkan as the alternative to evaluate during the spike phase.**
- OGRE's team is explicit: "For anything performance sensitive in Android, use the Vulkan RenderSystem"; GLES3 on Android is unmaintained and driver-buggy.
- Android itself is Vulkan-first: per the Android Developers Blog (13 Mar 2025), Google is "making Vulkan the official graphics API on Android," with OpenGL routed through ANGLE-over-Vulkan on newer devices (the Vulkan requirement fully applies from Android 17; roughly 85% of in-use Android devices are already Vulkan-capable).
- **Fallback strategy:** Vulkan driver quality is uneven on midrange Mali/Adreno/Xclipse/PowerVR. Because you're on Android 8.0+/Vulkan-capable devices, your fallback is not GLES but **feature-scaling within Vulkan** (drop shadow cascades, disable expensive post, reduce render scale) driven by GPU/driver detection, plus a device allow/deny list. Budget real device-testing time here — this is the #1 place "works on my phone" fails.
- **OGRE 1.x (14) vs ogre-next:** ogre-next has a more modern, HLMS/PBR, explicit-API-friendly architecture (better long-term visual ceiling on mobile) but porting RoR from 1.x to 2.x is a large rewrite of all rendering/material code. OGRE 14.x keeps RoR's 1.x API mostly intact (smaller diff, faster to first pixel) but with an older architecture. **For a solo dev, OGRE 14.x is the pragmatic first target** (it's also the path upstream is already committed to); revisit ogre-next only if 14.x's Vulkan/mobile performance proves inadequate in the spike.

**Filesystem & asset packaging.** Android assets are not a normal filesystem. RoR's content is ZIP-based (`.zip` mod bundles, terrains, meshes) loaded through OGRE's resource system (which already supports zip archives via zziplib). Two integration points:
- Use **Google Play Asset Delivery (PAD)** for the large base content (APKs are capped; PAD replaces legacy OBB, supports install-time/fast-follow/on-demand packs, delta patching, and **Texture Compression Format Targeting**). Access packs from C++ via the **Play Core Native SDK** and normal filesystem calls (fast-follow/on-demand packs are extracted to internal storage). For install-time assets bundled in the APK, read via the NDK **AAssetManager**.
- Because OGRE's zip resource layer wants file paths, prefer **on-demand/fast-follow PAD packs (extracted to a real path)** over reading from the compressed APK, so OGRE's existing archive code works unchanged. For non-Play distribution (F-Droid/direct APK/itch), fall back to a first-run download or bundled OBB-style archive.

**Audio.** RoR uses OpenAL Soft with layered engine audio (RPM-driven crossfades). OpenAL Soft does build on Android but its OpenSL backend is known-problematic (no sound / high latency / clicks on some devices); an Oboe backend was requested upstream but is not standard. **Recommendation: keep OpenAL Soft for the mixing/3D-audio logic but route its output through Oboe** (OpenAL Soft has backend abstraction; wiring an Oboe/AAudio sink is far less work than rewriting RoR's audio layer). Oboe (in AGDK) picks AAudio on 8.1+/OpenSL below, does automatic latency tuning, and works around device quirks. Engine-sound mixing is not ultra-low-latency-critical (unlike a rhythm game), so target the normal low-latency path, not exclusive MMAP, for compatibility.

**Input.** Replace OIS entirely. Use **GameActivity's input buffer** for touch/key events and the **Game Controller library** for gamepads. The touch control design is a major design problem in its own right (see §5). Keyboard/mouse code paths become gamepad/touch abstractions; OIS should be compiled out for the Android target behind an input abstraction layer.

**Threading, memory, lifecycle.** 
- Run **physics on a dedicated thread** (the `Actor`/`GfxActor` split already supports this) pinned toward the big cores; keep rendering on its own thread; use ADPF hints to tell the scheduler which threads are latency-critical. Be careful with big.LITTLE: a 2 kHz fixed-step solver migrated onto a LITTLE core will miss its budget.
- **Memory budgets** on midrange phones are real (assume you must live within ~2–3 GB for the process); use the Memory Advice API and aggressively cut texture budgets.
- **Lifecycle/process death:** Android will pause your app on background and can kill the process. A long-running sim must (a) cleanly stop the physics thread and release the Vulkan surface on `onPause`/surface-destroyed, (b) recreate GPU resources on resume, and (c) serialize enough session state to restore gracefully after process death. This is fiddly, human-debugging work.

### 3. Alternative Strategies (Honest Comparison)

You've chosen the native port; it's the right default given the goal is *Rigs of Rods*, not a new game. But weigh it honestly:

- **(a) Godot 4 (or Unity) shell + native C++ soft-body solver as a library.** Godot 4 has mature **GDExtension** for loading native C++ (`.so`) at runtime — you could keep RoR's node/beam solver as a GDExtension and rebuild rendering/UI/input in Godot, which has excellent, modern Android export, Vulkan Forward+/Mobile renderers, and a real touch UI toolkit. **Pros:** far better developer velocity for a solo AI-assisted dev on the rendering/UI/lifecycle/packaging side (Godot handles all the Android plumbing you'd otherwise hand-build), much higher visual ceiling with less effort, and the hardest solver stays C++. **Cons:** you must rewrite RoR's rendering/flexbody/material/terrain integration against Godot; **mod compatibility largely breaks** (OGRE `.mesh`/`.material`, MyGUI layouts, and truck "managedmaterials" don't map to Godot), and you'd need a mesh/material conversion pipeline. Unity is similar but with licensing/runtime-fee baggage and no source access. **This is the strongest alternative and you should reconsider it if the OGRE-14-on-Android spike (Phase 1) proves painful or slow.**
- **(b) Full rewrite.** Only rational if you want to diverge from RoR entirely (i.e., become BeamNG). Throws away 20 years of vehicle physics tuning and all mod compatibility. Not recommended.

**Decision triggers to revisit:** if (1) OGRE 14.x + deps won't build/render acceptably on target hardware after a bounded spike, or (2) the visual-modernization goals keep fighting OGRE 1.x's fixed-function-era architecture, seriously pivot to **Godot 4 + native solver (a)**. The soft-body solver is portable to either world, so keep it isolated behind a clean C-style API from day one to preserve that option.

### 4. Graphics & Performance Work for Midrange→Flagship

**Realistic targets:**
- **Midrange (Snapdragon 7-series, Dimensity 7000–8000):** target **1080p-class render resolution (often rendered below native panel res and upscaled), 30 fps locked** as the baseline, 60 fps only for light scenes, with dynamic resolution and reduced draw distance. Expect thermal throttling in 10–20 min sessions without careful power budgeting.
- **Flagship (Snapdragon 8 Gen 3 / 8 Elite, Dimensity 9000+):** **native-res 60 fps** achievable for moderate scenes; 90/120 fps only with aggressive LOD and few vehicles. Even flagships throttle, so design for sustained (post-throttle) clocks, not peak.
- These are engineering targets to validate on-device, not guarantees — RoR's per-vehicle physics cost and overdraw-heavy content make framerate content-dependent.

**TBDR implications.** Mobile GPUs are **tile-based deferred renderers (TBDR)**. Desktop patterns that are cheap on immediate-mode GPUs are expensive on mobile:
- **Avoid classic deferred rendering / big G-buffers** and frequent render-target switches (each RT switch flushes tiles to main memory — costly bandwidth). Use **forward or clustered/forward+ rendering.**
- **Minimize overdraw** (sort front-to-back, avoid large transparent layers).
- Use `dont_care` load/store (Vulkan render-pass load/store ops) on transient attachments so tiles aren't needlessly loaded/stored.

**Specific techniques to implement:**
- **Clustered/forward+ lighting**; drastically **reduce shadow cascades (1–2 PSSM splits) and shadow map resolution**; prefer **baked lighting** for static terrain where possible.
- **LOD** for vehicles and terrain; **aggressive frustum + distance culling**; **GPU instancing** for vegetation/props (PagedGeometry already batches vegetation).
- **Texture compression: ASTC** (preferred, quality/size tunable) with **ETC2** fallback; use **PAD Texture Compression Format Targeting** to ship per-device formats. Enforce **mipmapping and a strict texture budget** — this is the stated real bottleneck.
- **Reduce draw calls** (material atlasing, batching); **mesh simplification** pipeline for existing vehicle/terrain assets.

**Physics performance.** The solver cost scales roughly with node+beam count × substeps (2 kHz). On mobile you will likely need to **cap vehicle complexity** (node/beam budgets), limit simultaneous active actors, and possibly **reduce substep rate** for distant/inactive vehicles (LOD for physics). Multithread across big cores; keep the fixed timestep but tune substeps per device tier. **GPU compute for the solver is possible in theory but not recommended early** — the solver's tight data dependencies and small per-vehicle working sets map poorly to GPU dispatch overhead on mobile, and it competes with rendering for the GPU. Treat GPU-compute physics as a research spike, not a plan item.

**Dynamic resolution, VRR, thermal, battery.** Implement **dynamic resolution scaling** driven by frame time; support **variable refresh rate** via Swappy; use **ADPF thermal status** to step quality down before throttling; expose a battery-saver/quality toggle. 

**Shaders/materials that will break.** Anything routed through **Cg** (Nicemetal.cg managed materials, OgreTerrain's Cg generator) must be rewritten as **GLSL/GLSL-ES/SPIR-V**, either via OGRE's **RTSS** or hand-written PBR shaders. RoR's PSSM shaders and any fixed-function-era materials will need porting. Plan a **material audit** early: catalog every `.material`/`.cg` in base content and map each to a modern equivalent.

### 5. Making It Look Nicer / Modernizing Visuals & UI

**Affordable visual upgrades on TBDR:**
- **PBR materials** (metallic/roughness) via RTSS or custom HLMS-style shaders — modern renderers give per-pixel lighting "for free" vs. RoR's classic managed materials, which the lead dev confirms.
- **Better sky/atmosphere** — a modern physically-based sky is cheap; consider replacing/augmenting Caelum (also removes an LGPLv3 dependency).
- **Probe-based reflections** (baked cubemap/reflection probes) for vehicle bodywork instead of expensive screen-space reflections — SSR is bandwidth-hostile on mobile.
- **Cheap post:** simple **tonemapping** (ACES-approx), light **bloom**, and mild color grading are affordable; avoid heavy SSAO/SSR/DOF.

**UI/UX redesign — replace MyGUI with a touch-first UI.** Options for an OGRE/native-C++ Android app:
- **Dear ImGui** — RoR **already uses Dear ImGui** for parts of its UI (tuning menu, modding tools, since 2021.01), so you have an in-repo integration to build on. ImGui is not natively touch-friendly but adapts acceptably with enlarged hit targets, on-screen controls, and touch-scroll shims. Best velocity for developer/debug and functional menus.
- **RmlUi** (HTML/CSS-like) — better for a polished, skinnable consumer UI; more work to integrate but far nicer visually than ImGui for the main menus.
- **Native Android views over the SurfaceView** — best platform-native feel (system IME, accessibility, scroll physics) for menus/storefront; awkward for in-game HUD. GameActivity's SurfaceView makes this overlay approach viable.
- **Recommendation:** hybrid — **native Android views (or RmlUi) for front-end menus/mod browser**, **ImGui (touch-adapted) for in-sim HUD/debug**, migrating RoR's existing ImGui panels.

**Touch controls for a physics-heavy articulated-machinery sim — the hardest design problem.** RoR exposes a huge command set: steering/throttle/brake/clutch/gears, plus hydraulics, ropes, ties, winches, and full aircraft/boat controls. Lessons from comparable mobile titles (Construction Simulator mobile, mobile crane/heavy-equipment sims):
- **Contextual, mode-based control layouts** — show only the controls relevant to the current vehicle/mode (driving vs. crane vs. aircraft), swap the layout when the machine or mode changes. Construction Simulator uses on-screen **sliders** for machinery functions (e.g., a dedicated slider appears for a low-loader ramp) — a good pattern for hydraulics/winches.
- **Driving primitives:** virtual wheel or tilt-to-steer (user-selectable), plus throttle/brake pedals or an auto-accelerate toggle; **avoid tiny virtual sticks for precision driving** — buttons/sliders read better on glass.
- **Articulated machinery:** map real crane/excavator conventions (two virtual joysticks: slew/boom on one, hoist/telescope on the other — matching real mobile-crane control mapping) into contextual on-screen twin-stick overlays; expose rarely used commands via an expandable radial/drawer menu rather than cluttering the HUD.
- **Customizable/relocatable controls** and full **Bluetooth/USB gamepad support** (via Game Controller library) as a first-class alternative for serious users.
- Treat this as an iterative, playtested subsystem — it's where a "nicer than desktop" mobile experience is won or lost.

### 6. Mobile-Only Multiplayer

**How RoRnet works today.** Client-server **relay** model (the server doesn't simulate; it rebroadcasts each client's streams). Transport is **TCP via SocketW (libSocketW 3.10.36)** — *not* UDP for game data (a secondary source claiming UDP is wrong; LAN *discovery* uses a UDP broadcast on port 13000 (`RORNET_LAN_BROADCAST_PORT`), while game data is the TCP connection on port 12000 by default). Protocol version: current shipping builds use **RoRnet 2.45** (the 2025.03 RC notes it "only supports RoRnet 2.45, it is not compatible with 2.44"); the reference `ror-server` states compatibility with "RoRnet protocol version 2.43+." Features: token auth, config-file authorization (permissions/blacklist), and serverlist/master-server integration via a REST API (`multiplayer.rigsofrods.org`).

**What it transmits & bandwidth.** Each streamed actor update carries a fixed control struct **`RoRnet::VehicleState`** (~44 bytes: engine RPM/force/clutch/gear, steering `hydrodirstate`, brake, wheelspeed, plus light/horn bitmasks) **followed by a variable-length node-position buffer** sized to the vehicle's node count, capped at **`RORNET_MAX_MESSAGE_LENGTH` = 8192 bytes** per message (implying a rough ceiling of a few hundred nodes per networked vehicle). Vehicles too large to fit trigger a client error: *"Actor is too big to be sent over the net."* The character avatar uses a separate, smaller position stream (`NetCharacterMsgPos`). Official bandwidth guidance (ror-server README, verbatim): **"The RoR server uses large amounts of bandwidth, particularly for upload,"** with per-client budget **~64 kbit/s**, **download = `maxclients × 64 kbit/s`** and **upload = `maxclients × (maxclients−1) × 64 kbit/s`** (quadratic) — e.g. "16 clients: 1Mbit/s download, 15Mbit/s upload" and "32 clients: 2Mbit/s download, 64Mbit/s upload ⇐ will nearly saturate a 100Mbit/s LAN!" Hard cap **64 peers** (`RORNET_MAX_PEERS`), but docs recommend **<16**, default slots 10. The exact network send rate (Hz) is throttled/downsampled and not documented in a primary source I could confirm.

**Viability & recommendation.** For a *mobile client*, download of ~640 kbit/s for 10 players is fine on LTE/5G in absolute terms — but **TCP is the wrong transport for lossy, high-jitter mobile links**: head-of-line blocking and retransmission will stall position sync exactly when the radio degrades, despite packets being flagged "discardable." Notably, RoR's own lead dev has expressed intent to "migrate our netcode as-is to ENet." **Recommendation for a mobile-only fork:**
- **Keep the RoRnet *message model*** (VehicleState + node buffer, stream registration) — it's a reasonable data model — **but replace the transport** with **reliable-UDP**: **ENet** (simple, battle-tested, easy to build on Android, good for a solo dev) or **Valve's GameNetworkingSockets** (adds encryption, message fragmentation, and **NAT traversal via WebRTC ICE / hole-punching**, and has shipped on mobile). Send discardable position data as **unreliable/unordered** and control/spawn events as reliable.
- **Topology:** run **small dedicated servers** (the relay model is cheap to host and avoids trusting clients) rather than pure P2P. Because mobile devices are almost always behind **carrier-grade NAT (CGNAT)** — which frequently presents symmetric-NAT behavior that defeats UDP hole punching — a **publicly reachable relay/dedicated server is the pragmatic choice**; pure device-to-device connections would otherwise force you to run TURN relays anyway. GameNetworkingSockets' relay/ICE support helps for any P2P "host a game from your phone" mode, but expect to fall back to relay.
- **Hosting/cost/moderation for a solo dev:** a single small VPS can host several 8–16 player relay servers cheaply (bandwidth — driven by the quadratic upload — is the cost driver). Provide a simple master/serverlist, basic auth, and moderation tools (kick/ban, chat filtering). Keep populations small (8–16) both for bandwidth and moderation load.
- **Milestone placement:** multiplayer belongs **late** (Phase 7, after single-player is solid). Confirmed — do not build it early; it depends on stable physics, actor lifecycle, and content being finalized.

### 7. Legal, Licensing & Distribution

- **Code license: GPLv3.** A fork must keep GPLv3, provide **complete corresponding source**, and preserve attribution. The famous tension: **GPLv3's anti-tivoization + the app stores' DRM/ToS.** Practical reality: **GPLv3 apps are publishable on Google Play** (Google Play permits GPL software; sideloading/other stores exist, satisfying the "install your modified version" freedom), **but not on the Apple App Store** (FSF's position; App Store ToS + DRM conflict with GPL). This *reinforces your Android-first plan and warns against iOS via the App Store later.* Distribution options: **Google Play (works), F-Droid (ideal for GPL/FOSS), direct APK, itch.io.** Ship the source (e.g., public fork) to satisfy GPLv3 regardless of channel.
- **Dependency licenses for redistribution:** OGRE (MIT), MyGUI (MIT), AngelScript/OIS/PagedGeometry (zlib), curl (MIT), RapidJSON/mofilereader (MIT), UTFCpp (BSL-1.0) are all permissive and fine. **Watch the LGPL deps: Caelum (LGPLv3), OpenAL Soft (LGPLv2), MySocketW/pthreads (LGPLv2.1)** — LGPL is satisfiable but requires the user be able to relink/replace the library (dynamic linking or provision of object files); since the whole app is GPLv3 anyway this is generally compatible, but **LGPLv3 (Caelum) + Apple App Store has the same anti-tivoization conflict** — another reason to drop/replace Caelum and to stay off the App Store.
- **Content licensing — the biggest practical trap.** RoR's own site is explicit: community vehicles/terrains/sounds "are separate from the core project and are not covered under the project's license… licensed by their individual creators." **You generally cannot legally ship community mods** in your fork. You *can* ship RoR's **GPLv3 base content** repo. Anything else needs per-creator permission. Plan to ship a **small, license-clean base content set** and let users add their own content (mod support is a later phase anyway).
- **Trademark/naming.** "Rigs of Rods" and its logo are project identifiers; **fork under a distinct name and branding** to avoid trademark/endorsement confusion (GPL covers copyright, not trademark). Attribute the upstream project clearly.
- **Monetization.** GPLv3 does **not** forbid selling the app, but anyone who obtains it can redistribute the source and binaries freely, so paid-app models leak. Viable models: **free app + optional donations, cosmetic/non-code paid content you own the rights to, or paid convenience (hosted servers)** — not DRM-locked paid downloads (incompatible with GPLv3). Content licensing further constrains what you can sell.

### 8. Community & Upstream Relations

The RoR team is small, active, and has publicly acknowledged mobile demand while stating it's blocked on the OGRE 14 + Cg-removal work. This suggests **contributing the mobile-enabling groundwork upstream is more practical than a hard fork** for the *foundational* pieces: the OGRE upgrade, Cg removal, Vulkan render path, and dependency modernization are things upstream *wants* and would help maintain — landing them upstream shrinks your permanent maintenance burden. Keep Android-specific glue (GameActivity shell, touch UI, PAD packaging, mobile netcode) in your fork. Engage via the **forum (forum.rigsofrods.org), the community Discord, and GitHub issues/PRs.** Documentation resources: **docs.rigsofrods.org** (user + modding + truck file format), **developer.rigsofrods.org** (Doxygen source reference, e.g. `RoRnet.h`), the GitHub wiki (Compile-(Linux/macOS)), and DeepWiki's architecture overview.

### 9. Mod Compatibility (Deferred — but Plan for It)

Formats: **`.truck`/`.load`/`.airplane`/`.boat`/`.trailer`** vehicle definitions (text), **OGRE `.mesh`/`.skeleton`/`.material`**, **`.zip`** mod packaging, **`.terrn2`** terrain (+ `.otc` terrain config), **AngelScript** scripts.

- **What survives a native OGRE-based port:** truck/terrn2/OTC text formats **parse unchanged** (they're renderer-agnostic); `.zip` packaging works via OGRE's archive layer; `.mesh`/`.skeleton` survive **if** re-encoded for the target OGRE version and if materials are ported. **What breaks:** any material using **Cg** (rewrite to GLSL-ES/RTSS/PBR); MyGUI-dependent script UI (if you replace MyGUI); and desktop-input assumptions in scripts.
- **AngelScript on Android:** AngelScript is portable C++ and compiles for ARM (it interprets bytecode; no x86 JIT dependency), so it runs on Android. **Security is the concern:** running user-supplied scripts on mobile requires a **tight sandbox** — restrict the registered API surface (no filesystem/network/process access from scripts), enforce resource/time limits, and validate content. **Play policy** treats downloadable executable content/scripts cautiously: interpreted game-logic scripts that can't call arbitrary native code and can't modify the app are generally acceptable, but you must avoid anything that looks like downloading executable code that changes app behavior at the native level.
- **Content delivery under scoped storage:** Android's scoped storage means you can't freely read arbitrary user directories. Deliver/install mods into **app-specific storage** (or use the Storage Access Framework for user-picked files), with an in-app mod browser/downloader (like RoR's in-game repository browser since 2022.04). Keep a clear separation between shipped base content and user-installed mods.
- **Asset conversion pipeline:** build a repeatable pipeline to **re-encode meshes** (to target OGRE mesh version), **recompress textures to ASTC/ETC2**, **simplify high-poly meshes/LODs**, and **rewrite Cg→GLSL materials**. RoR's Blender import/export addons (`RoROgreAddons`, blender2ogre) are the starting point for mesh tooling.

### 10. Phased, Dependency-Ordered Project Plan

**Phase 0 — Environment, fork, CI.** Fork the repo under a new name; set up Android Studio + NDK + CMake; stand up CI (GitHub Actions can cross-compile Android; a self-hosted or cloud runner for device tests). *Exit:* fork builds for **desktop** unchanged in CI; Android toolchain installed. *AI-assistable:* build scripts, CI YAML, CMake refactoring. *Human:* toolchain/driver quirks.

**Phase 1 — De-risk the killers first (SPIKES, do before committing).** Two throwaway experiments:
- **Spike A (rendering):** Get **OGRE 14.x (or ogre-next) rendering a textured mesh + basic terrain via Vulkan on a real midrange phone.** This validates the entire render path and the Cg-removal assumption. *This is the project's make-or-break test.*
- **Spike B (physics framerate):** Compile **just the node/beam solver** for arm64 and benchmark a representative vehicle (and a few simultaneously) at the 2 kHz fixed step on midrange + flagship, measuring headroom. 
*Exit:* both spikes show acceptable results, or you pivot strategy (Godot + native solver). *Human-heavy:* GPU driver triage, physics numerical stability, profiling.

**Phase 2 — Minimal arm64 build.** Get RoR compiling for arm64 headless/minimal (deps building: OGRE, curl, AngelScript, OpenAL, forked Caelum/PagedGeometry/MyGUI or stubs). *Exit:* app launches on device, initializes OGRE Vulkan, clears the screen. *AI-assistable:* dependency CMake, JNI/GameActivity glue, `#ifdef` platform abstraction.

**Phase 3 — First pixels → terrain → one vehicle.** Render triangle → a terrain → spawn and draw a single vehicle (GfxActor) with ported materials. *Exit:* one vehicle visible on a terrain on device. *Human:* material/Cg porting, RenderDoc-on-Android debugging.

**Phase 4 — Physics at target framerate.** Wire the solver on its own thread; drive one vehicle; hit the framerate target on midrange with physics+render concurrent; tune substeps, threading, NEON, ADPF. *Exit:* a drivable vehicle at target fps/thermals on midrange. *Human:* threading, numerical stability, big.LITTLE tuning.

**Phase 5 — Touch input & UI.** GameActivity input + Game Controller; contextual touch control system; replace/augment MyGUI front-end; migrate ImGui HUD. *Exit:* fully playable single-player by touch and gamepad. *AI-assistable:* UI layout code; *Human:* control feel, playtesting.

**Phase 6 — Visual modernization.** PBR/RTSS materials, modern sky, probe reflections, cheap post, dynamic resolution, LOD, ASTC via PAD. *Exit:* target visual quality at target performance across the device tier.

**Phase 7 — Mobile-only multiplayer.** Replace RoRnet transport with ENet/GameNetworkingSockets (keep message model); dedicated relay servers; serverlist/auth; moderation. *Exit:* stable 8–16 player mobile sessions over LTE/5G. *Human:* netcode, NAT/CGNAT.

**Phase 8 — Mod support.** Sandboxed AngelScript; mod browser/installer under scoped storage; asset conversion pipeline; document supported formats. *Exit:* users can install compatible mods within Play policy.

**Phase 9 — Distribution.** Play Store listing (GPLv3-compliant source published), F-Droid, direct APK; PAD packs; store assets. *Exit:* shipped.

**Recommended tooling:** Android Studio; **ndk-stack / ndk-gdb / lldb** for native crash triage; **RenderDoc for Android** and **Android GPU Inspector (AGI)** for frame debugging; **Snapdragon Profiler** (Adreno) and **Arm Mobile Studio / Streamline** (Mali) for GPU counters; **Perfetto/systrace** for CPU/thread/thermal timelines; **Oboe/OboeTester** for audio latency; GitHub Actions (+ optionally Firebase Test Lab or a device farm) for CI and multi-device testing.

## Recommendations

1. **Do Spike A (OGRE 14/Vulkan on a real midrange phone) before anything else.** If it fails or drags, pivot to **Godot 4 + native soft-body GDExtension** — decide this within a bounded spike, not months in. This is the project's central risk.
2. **Keep the node/beam solver isolated behind a clean C API from day one.** It's your most valuable, most portable asset and the one thing worth preserving across *any* strategy pivot. Spike B validates its ARM framerate early.
3. **Adopt the AGDK stack** (GameActivity + Swappy + ADPF + Oboe + Game Controller + PAD) rather than hand-rolling Android plumbing — it's the highest-leverage way for a solo AI-assisted dev to avoid reinventing lifecycle, frame pacing, thermal, audio, and asset delivery.
4. **Land the foundational modernization (OGRE 14, Cg removal, Vulkan) upstream where possible**, keeping only Android-specific glue in your fork, to minimize long-term maintenance.
5. **Target arm64 + Vulkan only; Android 8.0+ (ideally 10+).** Build a device allow/deny list and feature-scaling fallback; budget real on-device testing across 3–4 GPU families.
6. **For multiplayer, replace RoRnet's TCP transport with reliable-UDP (ENet or GameNetworkingSockets) but keep its message model; run small dedicated relay servers** (CGNAT makes pure P2P unreliable). Do it in Phase 7, not earlier.
7. **Ship Android via Play + F-Droid/direct APK; do not plan iOS via the Apple App Store** (GPLv3 incompatibility). Publish full source to satisfy GPLv3 on every channel.
8. **Ship only license-clean base content; defer mods; sandbox AngelScript.** Do not bundle community mods without per-creator permission.
9. **Treat touch controls as a first-class, iterative design subsystem** — contextual mode-based layouts, sliders for machinery, twin-stick overlays for cranes/aircraft, full gamepad support.

**Benchmarks that change the plan:** if Spike A can't hold ~30 fps at 1080p-class on a midrange 7-series device after optimization → pivot to Godot. If Spike B shows a single representative vehicle can't sustain 2 kHz within ~30% CPU budget on midrange → reduce vehicle complexity budgets and/or physics LOD before proceeding. If Vulkan driver failures exceed a small fraction of target devices → expand the device deny-list and consider ANGLE/GLES fallback despite OGRE's warnings.

## Caveats
- **The OGRE-14/Cg-removal blocker is stated by the RoR team but its completion status is uncertain**; as of the sources gathered, upstream had not declared it done, and the macOS port remains blocked on it. Verify current upstream status before committing — if it has landed, your Phase 1 risk drops substantially.
- **Exact figures are unconfirmed in primary sources for:** RoR's network send rate (Hz) and the precise per-node byte encoding in stream packets; whether the current solver uses any SSE intrinsics (it appears not to, but confirm in `Actor.cpp`); and real on-device framerate/thermal numbers (my performance targets are engineering estimates to validate, not measured results).
- **OGRE Android maturity is real but not turnkey:** ogre-next Android/Vulkan is "preliminary/stable-with-caveats," and OGRE devs report mixed Android Vulkan experiences across vendors. Expect driver-specific bugs.
- **RoRnet version is in flux** (2.44 ↔ 2.45, with a hard incompatibility between them); since you're mobile-only and replacing the transport, cross-version compatibility with desktop servers is explicitly out of scope, which simplifies this.
- **Content and third-party mod licensing is genuinely restrictive** and is the most likely source of legal trouble in a fork — treat any non-GPL content as un-shippable until proven otherwise.
- This plan assumes a solo developer leaning heavily on AI assistants; the tasks flagged "human-debugging" (GPU driver triage, physics stability, native crash/threading, netcode/NAT) are where AI help is weakest and where most schedule risk concentrates.