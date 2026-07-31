# Trussline

**A native Android port of the Rigs of Rods soft-body physics simulator** — arm64, Vulkan, touch-first.

> **Independent fork.** Trussline is not affiliated with, endorsed by, or supported by the Rigs of Rods project. Please don't send Trussline issues their way. See [NOTICE](NOTICE) for full attribution.

---

## Status: pre-alpha, Phase 0

Nothing runs on Android yet. The project is in its de-risking phase, and the honest summary is that two questions decide whether it proceeds at all:

1. **Can OGRE 14 + Vulkan render acceptably on a low-end phone?** Upstream is on OGRE 1.11.6.1 and still depends on the dead, proprietary, x86-only NVIDIA Cg toolkit — which cannot exist on ARM. Removing it is both a porting task and a licensing obligation.
2. **Can the 2 kHz node/beam solver fit the CPU budget?** Early signs are good: a source audit found zero SIMD/x86 dependencies, so the math is portable scalar C++. But the thread pool parallelizes per *vehicle*, never within one, so a single vehicle is bound to a single core.

Both are answered by bounded spikes before any real investment. If they fail, the fallback is Godot 4 with the solver retained as a native extension — which is why the solver is being isolated behind a C API from day one.

## Why this fork exists

Upstream has publicly stated that a mobile port is blocked until the OGRE 14 upgrade lands and Cg is dropped. The deeper reason is instructive: their blocker is not the engine, it's **~15 years of community mods carrying Cg shaders**. A maintainer put it plainly — they agreed shipped shaders could be replaced, but got stuck on shaders in mods.

Trussline drops mod compatibility, and with it inherits none of that constraint. That is the entire strategic bet — and it is a real bet, not a free win: the community content library is the single biggest reason anyone plays Rigs of Rods, and a fork legally cannot ship it.

## Project documents

This repository is documentation-first on purpose; the planning is the work at this stage.

| Document | What it is |
|---|---|
| [ROADMAP.md](ROADMAP.md) | Ten dependency-ordered phases with task lists, exit criteria, and gates. No timelines by design |
| [DECISIONS.md](DECISIONS.md) | What's locked, what's open, and a verification ledger of claims checked against source |
| [RISKS.md](RISKS.md) | Risk register with mitigations and trigger → contingency pairs |
| [LEGAL.md](LEGAL.md) | Licensing, trademark, content, and distribution analysis |
| [AGENTS.md](AGENTS.md) | Working rules for AI assistants — hard constraints and division of labor |
| [GLOSSARY.md](GLOSSARY.md) | Shared vocabulary |
| [BUILDING-ANDROID.md](BUILDING-ANDROID.md) | Android build instructions |
| [PROJECT-PLAN.md](PROJECT-PLAN.md) | Original feasibility research |

## Building

**Desktop** (the reference build — Android results are meaningless without it):

```bash
cmake . -GNinja -DCMAKE_BUILD_TYPE=Release -Bbuild -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=cmake/conan_provider.cmake -DCMAKE_INSTALL_PREFIX=redist -DCREATE_CONTENT_FOLDER=ON
```

**Android**: see [BUILDING-ANDROID.md](BUILDING-ANDROID.md). Requires NDK 27.3.13750724 and JDK 17.

## Target

arm64-v8a, Vulkan, Android 8.0+. No 32-bit, no OpenGL ES path, no iOS — GPLv3 is incompatible with the Apple App Store. Distribution is planned for Google Play plus F-Droid and direct APK.

## Relationship to upstream

Trussline tracks [RigsOfRods/rigs-of-rods](https://github.com/RigsOfRods/rigs-of-rods) as a git remote and reads its work freely — it's GPLv3 — but does not depend on upstream landing anything, and makes no commitment to contribute back. Upstream's public work is monitored at every phase boundary so effort isn't duplicated.

## License

GPLv3 or later, inherited from upstream and permanent. There is no CLA and the contributor list is admittedly incomplete, so relicensing is not realistically possible. Complete corresponding source is published for every distributed binary.

Copyright (c) 2005-2013 Pierre-Michel Ricordel
Copyright (c) 2007-2013 Thomas Fischer
Copyright (c) 2009-2025 Petr Ohlidal and Rigs of Rods contributors
Copyright (c) 2026 Trussline contributors

Full text in [COPYING](COPYING). Library licenses in [DEPENDENCIES.md](DEPENDENCIES.md). Upstream authors in [AUTHORS.md](AUTHORS.md).

**Content is licensed separately from code.** Community-created Rigs of Rods assets are not covered by this license and are not redistributed here.
