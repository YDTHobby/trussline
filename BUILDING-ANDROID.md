# Building Trussline for Android

Target: **arm64-v8a, Vulkan, Android 8.0+ (API 26+)**. No 32-bit, no GLES, no x86 except an optional emulator build for iteration (D-003).

> The build must be reproducible from this document alone. If you had to figure something out that isn't written here, that's a documentation bug — fix it while you remember.

## 1. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Android Studio | latest stable | Optional for building, but you want it for the profilers and device manager |
| Android SDK Platform | API 34+ | Compile SDK; min SDK stays at 26 |
| Android NDK | **`27.3.13750724`** (r27 LTS, latest patch) | Installed via `sdkmanager --install "ndk;27.3.13750724"`. Pinned in D-014; the CI workflow's r-name must be confirmed to match |
| CMake | 3.24+ | The NDK ships one; a system CMake also works |
| Ninja | any recent | Much faster than Make for the superbuild |
| JDK | **17** | Required by current AGP. ⚠️ A JRE is not enough — check `javac -version`, not `java -version`. Android Studio's bundled JBR at `%ProgramFiles%\Android\Android Studio\jbr` works for `sdkmanager` but should not be `JAVA_HOME` for Gradle |
| MSVC | VS Build Tools, C++ workload | Windows desktop build only — but required, since the desktop build is the reference behavior for the OGRE upgrade (ROADMAP § 2.0) |
| Git | any | Superbuild clones dependencies |

Set `ANDROID_NDK_HOME` (or pass `-DANDROID_NDK=` explicitly). Everything below assumes it is set.

**Platform tools / ADB** must be on `PATH`. On this project's development machine ADB lives at `C:\platform-tools\adb.exe`.

## 2. Build the dependency superbuild

Every third-party dependency is cross-compiled once into a single prefix. This is slow the first time (OGRE dominates) and cached afterward.

```bash
cmake -S android/superbuild -B android/superbuild/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DANDROID_NDK="$ANDROID_NDK_HOME"
```

```bash
cmake --build android/superbuild/build --parallel
```

Output lands in `android/superbuild/out/{include,lib}`. Useful knobs:

- `-DTL_ANDROID_API=29` — raise the minimum API level.
- `-DTL_BUILD_MYGUI=ON` / `-DTL_BUILD_CAELUM=ON` — both are **off by default**; they are replacement-bound (Phases 5 and 6) and Caelum is additionally LGPLv3 and Cg-exclusive.
- `-DTL_SUPERBUILD_OUT=<path>` — relocate the install prefix.

OIS is deliberately absent — it is desktop-only and replaced by GameActivity input plus the Game Controller library (D-006).

## 3. Build the app

`TODO(Phase 2.3)` — no Gradle project exists until the GameActivity shell is written. It will be:

```bash
./gradlew :app:assembleDebug
```

## 4. Install and run

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

```bash
adb logcat -s Trussline:V OGRE:V AndroidRuntime:E
```

Symbolize a native crash by piping logcat through `ndk-stack`:

```bash
adb logcat | $ANDROID_NDK_HOME/ndk-stack -sym app/build/intermediates/cxx/Debug/obj/arm64-v8a
```

## 5. Troubleshooting

### ADB reports no devices

Encountered on this project's setup (2026-07-31) — worth working through in order, cheapest first:

1. **Confirm the daemon is up and re-check.** A cold `adb` start can miss a device that was already plugged in.
   ```bash
   adb kill-server; adb start-server; adb devices -l
   ```
2. **Check the USB mode on the phone.** Pull down the notification shade, tap the USB notification, and select **File transfer / MTP** (or **PTP**). "Charging only" will not expose ADB. If Windows shows the phone as a drive letter but `adb devices` is empty, this is usually the cause.
3. **Confirm USB debugging is actually on.** Settings → About phone → tap *Build number* seven times → back → System → Developer options → **USB debugging**.
4. **Accept the RSA prompt.** The first connection shows an "Allow USB debugging?" dialog *on the phone*. Until you tap Allow, the device appears as `unauthorized` or not at all. Tick "Always allow from this computer". If the dialog never appears, revoke and retry: Developer options → **Revoke USB debugging authorizations**, then replug.
5. **Try a different cable and port.** Charge-only cables are extremely common and produce exactly this symptom. Prefer a rear-panel USB port over a hub.
6. **Windows driver.** If the device doesn't appear under `Get-PnpDevice -Class AndroidUsbDeviceClass`, Windows lacks a driver. Install the OEM USB driver, or Google's USB Driver via SDK Manager → SDK Tools → *Google USB Driver*, then update the driver in Device Manager.
7. **Wireless debugging as a fallback** (Android 11+): Developer options → Wireless debugging → Pair device with pairing code.
   ```bash
   adb pair <ip>:<pairing-port>
   ```

### Superbuild fails on a dependency

Build that dependency alone to get readable output — the superbuild interleaves logs across parallel targets:

```bash
cmake --build android/superbuild/build --target <name>
```

### App launches then immediately exits

Almost always Vulkan surface or lifecycle related. Check logcat for the OGRE render-system init sequence first, then confirm the device actually reports Vulkan support:

```bash
adb shell pm list features | grep vulkan
```

## 6. Emulator builds

The emulator is currently the **primary development target** (D-013 — no physical device is reachable). Configure the superbuild with `-DTL_ANDROID_ABI=x86_64`; it builds and prints a loud warning rather than failing, so an x86 artifact can never be mistaken for a shipping build.

```bash
cmake -S android/superbuild -B android/superbuild/build-x86 -G Ninja -DCMAKE_BUILD_TYPE=Release -DTL_ANDROID_ABI=x86_64 -DANDROID_NDK="$ANDROID_NDK_HOME"
```

**What the emulator can and cannot tell you.** It *can* validate that the code builds, that OGRE initializes its Vulkan RenderSystem, that shaders compile, that scenes render correctly, and that the Android lifecycle is handled — which is most of Spike A's functional half. It **cannot** validate Vulkan driver quirks on real GPUs, thermal throttling, frame pacing, or sustained performance. **No Gate 1 evidence may come from it** (D-013); the framerate thresholds are meaningless on host-GPU-backed emulation.

Installed system images: `android-33/35/36/36.1`, all `x86_64`. Available AVD: `Pixel_7_Auto`.

```bash
emulator -list-avds
```
