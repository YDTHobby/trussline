# Spike A — does OGRE 14 + Vulkan render on Android?

**Throwaway by design** (ROADMAP § 1.1). The deliverable is the answer and the notes below, not this code. It is preserved because the gotchas cost real time and will be hit again in Phase 2.

The `ogre/`, `build-*/`, and `install-*/` directories are build artifacts and are **not** committed.

## Status

| Milestone | Result |
|---|---|
| OGRE 14.5.2 cross-compiles for Android with Vulkan | ✅ zero errors |
| Vulkan RenderSystem links into an APK | ✅ 73 `VulkanRenderSystem` symbols in `libspikea.so` |
| APK builds | ✅ 4.48 MB |
| Initialises Vulkan on Android | ✅ selects a real Vulkan 1.4.0 device |
| Creates surface + swapchain from `ANativeWindow` | ✅ 2400×1080 |
| Presents frames | ✅ vsync-locked 60 fps, zero errors, six independent runs |
| **Frame *content* verified** | ❓ **unresolved — see "Verifying pixels" below** |
| Renders on hardware | ⛔ blocked — reference phone unavailable (D-013) |

Emulator results are **functional only**. No Gate 1 evidence can come from them: framerate against a host GPU is meaningless, and driver quirks are exactly what the emulator cannot reproduce.

## Reproducing

```bash
build-ogre-android.bat
```

```bash
build-shell.bat
```

```bash
powershell -File run-spike.ps1
```

## Gotchas, in the order they bite

**1. `find_package(OGRE)` fails even with a correct `CMAKE_PREFIX_PATH`.**
`android.toolchain.cmake` sets `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY`, which confines `find_package` to the NDK sysroot and makes it ignore any prefix outside it. The path is right, the file exists, and CMake is simply forbidden from looking. Pass `-DOGRE_DIR=<install>/lib/OGRE/cmake` to bypass the search, and relax the mode to `BOTH`.

**2. `${OGRE_LIBRARIES}` does not contain the Vulkan render system — and cannot be linked anyway.**
Two separate problems in one variable:
- `OGREConfig.cmake` declares only `ogre_declare_plugin(RenderSystem GLES2)` for Android. `RenderSystem_Vulkan` *is* exported by `OgreTargets.cmake` but never advertised, so linking the blob silently omits the entire renderer you built.
- The blob also drags in optional components (`OgreBullet`, `OgreVolume`) whose imported targets reference `ZLIB::ZLIB` and build-tree paths that were never installed, so it fails at configure time regardless.

Name targets explicitly: `OgreMain`, `RenderSystem_Vulkan`.

**3. `-DOGRE_BUILD_RENDERSYSTEM_GLES2=OFF` is not honoured on Android.**
`libRenderSystem_GLES2Static.a` is produced anyway. Same root cause as #2 — OGRE's Android packaging assumes GLES2. Harmless for a spike; **must be resolved before Phase 2** so no GLES path ships (D-003).

**4. `OGRE_BUILD_DEPENDENCIES=ON` genuinely works when cross-compiling.**
It fetched and cross-built freetype, Bullet, pugixml and the rest with the NDK toolchain unattended. This was expected to be painful and was not — it removes a chunk of anticipated Phase 2.1 work.

**5. The window handle contract.**
`OgreVulkanWindow.cpp` reads `externalWindowHandle` from the misc params as a `size_t`, and asserts it is present. Pass the `ANativeWindow*` via `StringConverter::toString((size_t)window)`.

**6. NativeActivity drops its own entry point.**
Without `-u ANativeActivity_onCreate` the linker strips it as unreferenced and the app dies at startup with a misleading "unable to find native library".

**7. A fresh AVD ships with `hw.gpu.enabled=no`.**
That forces software rendering and silently invalidates the whole test. Set `hw.gpu.enabled=yes` and `hw.gpu.mode=host` in the AVD's `config.ini`.

## Verifying pixels — five dead ends, and the lesson

The render loop demonstrably presents frames. Proving those frames *contain* the clear colour defeated every programmatic method available on the emulator:

| Method | Result |
|---|---|
| `adb screencap` | Uninformative. Black either way — whether the app draws black *or* the capture cannot see a GPU-composited surface. Two captures 900 ms apart were byte-identical despite a per-frame cycling colour. |
| `writeContentsToFile(".png")` | No PNG codec linked. *"Supported formats are: astc dds ktx mesh pkm."* |
| `writeContentsToFile(".dds")` | *"DDS encoding for non power two textures not supported"* — the window is 2400×1080. |
| `writeContentsToFile(".ktx")` | *"ktx - encoding to file not supported."* |
| `copyContentsToMemory` on the swapchain | Returned `0 0 0 0`. A 0xAB sentinel **was** overwritten, so the call did execute. |
| `copyContentsToMemory` on an offscreen RTT cleared to pure red | Also returned `0 0 0 0`, not `255 0 0 255`. |

**That last row is the finding.** Two entirely independent render targets — a swapchain image and an offscreen texture — both returning *exactly* zero, with the sentinel overwritten in each case, is not what a genuinely black frame looks like. It is what a readback that zero-fills looks like.

> **Do not use `copyContentsToMemory` to verify rendering on OGRE 14.5.2's Vulkan backend.** It appears to write zeros rather than pixels, which makes it worse than useless: it produces confident, wrong answers.

So frame content remains **unverified**, not disproven. The cheap resolution is a human looking at a screen — the emulator window, or real hardware — which costs seconds and is exactly what the hardware pass will do anyway. Chasing it further on a machine where the instrument is broken has poor return.

## Deliberate scope choices

**NativeActivity, not GameActivity.** The `ANativeWindow` contract OGRE consumes is identical either way, so this proves the same thing with one less external dependency. GameActivity remains the product choice (D-004) and lands in Phase 2.3.

**Clear-colour first, geometry second.** The milestone ladder in `main.cpp` — Root constructs → RenderWindow created → frames present → geometry draws — exists so a failure localises itself instead of producing one undifferentiated "it didn't work". The clear colour cycles per frame so a static screenshot can't be mistaken for a live swapchain.
