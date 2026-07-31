# Trussline — Android arm64 dependency superbuild
#
# One command builds every third-party dependency for arm64-v8a using the NDK
# CMake toolchain. Modeled on upstream's ror-dependencies meta-project
# (per-dependency BUILD_* options + ExternalProject), retargeted for Android.
#
# Why a superbuild instead of Conan: RoR's Conan setup is desktop-only, and
# several dependencies are OGRE-version-locked forks that aren't in any public
# package registry. See ROADMAP.md § 2.1.
#
# Usage:
#   cmake -S . -B build -G Ninja -DANDROID_NDK=/path/to/ndk
#   cmake --build build --parallel
#
# TODO(fork): every version pin below is marked TODO — fill from the fork's
# conanfile.py once the OGRE 14.x upgrade (ROADMAP § 2.0) settles the versions.

cmake_minimum_required(VERSION 3.24)
project(trussline-android-deps LANGUAGES NONE)

include(ExternalProject)

# ---------------------------------------------------------------------------
# Target configuration
# ---------------------------------------------------------------------------

set(TL_ANDROID_ABI "arm64-v8a" CACHE STRING "Target ABI — arm64-v8a only (D-003)")
set(TL_ANDROID_API "26" CACHE STRING "Minimum API level (Android 8.0; D-003 prefers 29+)")
set(TL_SUPERBUILD_OUT "${CMAKE_BINARY_DIR}/out" CACHE PATH "Install prefix for all dependencies")

# arm64-v8a is the only SHIPPING ABI (D-003). x86_64 is allowed because the
# emulator is the primary development target while no physical device is
# reachable (D-013) — but it is loudly marked as non-shippable so an x86 build
# can never be mistaken for a release artifact. Any other ABI is an error.
if(TL_ANDROID_ABI STREQUAL "x86_64")
  message(WARNING
    "Building for x86_64: EMULATOR/DEVELOPMENT ONLY, never shipped. "
    "The emulator cannot validate Vulkan driver behavior, thermal throttling, "
    "or frame pacing, so nothing built here counts as Gate 1 evidence (D-013).")
elseif(NOT TL_ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR
    "TL_ANDROID_ABI is '${TL_ANDROID_ABI}'. Only arm64-v8a (shipping) and "
    "x86_64 (emulator development) are supported. See D-003.")
endif()

if(NOT ANDROID_NDK)
  if(DEFINED ENV{ANDROID_NDK_HOME})
    set(ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
  elseif(DEFINED ENV{ANDROID_NDK_ROOT})
    set(ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
  endif()
endif()

set(TL_TOOLCHAIN "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
if(NOT EXISTS "${TL_TOOLCHAIN}")
  message(FATAL_ERROR
    "NDK toolchain not found at:\n  ${TL_TOOLCHAIN}\n"
    "Pass -DANDROID_NDK=<path> or set ANDROID_NDK_HOME. See BUILDING-ANDROID.md.")
endif()

message(STATUS "Trussline deps -> ${TL_SUPERBUILD_OUT}")
message(STATUS "  ABI ${TL_ANDROID_ABI} / API ${TL_ANDROID_API} / NDK ${ANDROID_NDK}")

# Arguments every dependency inherits. Anything cross-compilation-related belongs
# here, not in individual ExternalProject_Add calls — divergence here is how you
# get one library built for the wrong ABI and a link error three hours later.
set(TL_COMMON_ARGS
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TL_TOOLCHAIN}
  -DANDROID_ABI:STRING=${TL_ANDROID_ABI}
  -DANDROID_PLATFORM:STRING=android-${TL_ANDROID_API}
  -DANDROID_STL:STRING=c++_shared
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DCMAKE_INSTALL_PREFIX:PATH=${TL_SUPERBUILD_OUT}
  -DCMAKE_PREFIX_PATH:PATH=${TL_SUPERBUILD_OUT}
  -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON
  -DBUILD_SHARED_LIBS:BOOL=OFF
)

# ---------------------------------------------------------------------------
# What to build
#
# OIS is absent by design: it is desktop-only and removed on Android (D-006).
# Input comes from GameActivity + the Game Controller library instead.
# ---------------------------------------------------------------------------

option(TL_BUILD_ZLIB        "Build zlib"                      ON)
option(TL_BUILD_ZZIPLIB     "Build zziplib (OGRE zip archives)" ON)
option(TL_BUILD_LIBPNG      "Build libpng"                    ON)
option(TL_BUILD_FREETYPE    "Build FreeType"                  ON)
option(TL_BUILD_CURL        "Build curl"                      ON)
option(TL_BUILD_OPENAL      "Build OpenAL Soft"               ON)
option(TL_BUILD_ANGELSCRIPT "Build AngelScript"               ON)
option(TL_BUILD_FMT         "Build fmt"                       ON)
option(TL_BUILD_OGRE        "Build OGRE 14.x (Vulkan)"        ON)
option(TL_BUILD_PAGEDGEO    "Build PagedGeometry fork"        ON)
# Replacement-bound — see the stub-first policy in ROADMAP § 2.1.
option(TL_BUILD_MYGUI       "Build MyGUI fork (replaced in Phase 5)"  OFF)
option(TL_BUILD_CAELUM      "Build Caelum fork (replaced in Phase 6)" OFF)

# ---------------------------------------------------------------------------
# Leaf libraries — no OGRE coupling, safe to build first and in any order
# ---------------------------------------------------------------------------

if(TL_BUILD_ZLIB)
  ExternalProject_Add(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1   # TODO(fork): match conanfile.py pin
    GIT_SHALLOW    TRUE
    CMAKE_ARGS     ${TL_COMMON_ARGS}
  )
endif()

if(TL_BUILD_ZZIPLIB)
  ExternalProject_Add(zziplib
    GIT_REPOSITORY https://github.com/gdraheim/zziplib.git
    GIT_TAG        v0.13.78  # TODO(fork): upstream pins a custom recipe — verify
    GIT_SHALLOW    TRUE
    DEPENDS        $<$<BOOL:${TL_BUILD_ZLIB}>:zlib>
    CMAKE_ARGS     ${TL_COMMON_ARGS}
                   -DZZIPTEST:BOOL=OFF
                   -DZZIPSDL:BOOL=OFF
  )
endif()

if(TL_BUILD_CURL)
  # Android has first-class curl support. Use the NDK's system TLS rather than
  # dragging in OpenSSL unless something specifically needs it.
  ExternalProject_Add(curl
    GIT_REPOSITORY https://github.com/curl/curl.git
    GIT_TAG        curl-8_11_0   # TODO(fork): match conanfile.py pin
    GIT_SHALLOW    TRUE
    DEPENDS        $<$<BOOL:${TL_BUILD_ZLIB}>:zlib>
    CMAKE_ARGS     ${TL_COMMON_ARGS}
                   -DBUILD_CURL_EXE:BOOL=OFF
                   -DCURL_USE_LIBPSL:BOOL=OFF
  )
endif()

if(TL_BUILD_OPENAL)
  # Mixing/3D-audio logic stays; the OpenSL output backend is the problem child.
  # An Oboe/AAudio sink is wired in Phase 4.2 (D-005) — not here.
  ExternalProject_Add(openal-soft
    GIT_REPOSITORY https://github.com/kcat/openal-soft.git
    GIT_TAG        1.24.2   # TODO(fork): match conanfile.py pin (1.24.3 upstream)
    GIT_SHALLOW    TRUE
    CMAKE_ARGS     ${TL_COMMON_ARGS}
                   -DALSOFT_EXAMPLES:BOOL=OFF
                   -DALSOFT_UTILS:BOOL=OFF
                   -DALSOFT_TESTS:BOOL=OFF
  )
endif()

if(TL_BUILD_ANGELSCRIPT)
  # Portable interpreted bytecode — no x86 JIT dependency, compiles clean for ARM.
  # Sandboxing is the real work, and it happens in Phase 8, not here.
  ExternalProject_Add(angelscript
    URL            https://www.angelcode.com/angelscript/sdk/files/angelscript_2.36.1.zip
    # TODO(fork): match conanfile.py pin (2.35.1 upstream); verify URL + hash
    SOURCE_SUBDIR  angelscript/projects/cmake
    CMAKE_ARGS     ${TL_COMMON_ARGS}
  )
endif()

if(TL_BUILD_FMT)
  ExternalProject_Add(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2   # TODO(fork): match conanfile.py pin (12.1.0 upstream)
    GIT_SHALLOW    TRUE
    CMAKE_ARGS     ${TL_COMMON_ARGS} -DFMT_TEST:BOOL=OFF
  )
endif()

# ---------------------------------------------------------------------------
# OGRE — the load-bearing one
# ---------------------------------------------------------------------------

if(TL_BUILD_OGRE)
  # Vulkan only (D-002/D-003). GLES3 is unmaintained on Android and driver-buggy.
  # Nothing here enables Cg: it does not exist on ARM, and eliminating Cg *content*
  # is separate work in Phase 3.2.
  ExternalProject_Add(ogre
    GIT_REPOSITORY https://github.com/OGRECave/ogre.git
    GIT_TAG        v14.3.4   # TODO(fork): pin the version § 2.0 settles on
    GIT_SHALLOW    TRUE
    DEPENDS        $<$<BOOL:${TL_BUILD_ZZIPLIB}>:zziplib>
                   $<$<BOOL:${TL_BUILD_FREETYPE}>:freetype>
    CMAKE_ARGS     ${TL_COMMON_ARGS}
                   -DOGRE_BUILD_RENDERSYSTEM_VULKAN:BOOL=ON
                   -DOGRE_BUILD_RENDERSYSTEM_GLES2:BOOL=OFF
                   -DOGRE_BUILD_RENDERSYSTEM_GL:BOOL=OFF
                   -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS:BOOL=OFF
                   -DOGRE_BUILD_COMPONENT_RTSHADERSYSTEM:BOOL=ON
                   -DOGRE_BUILD_COMPONENT_TERRAIN:BOOL=ON
                   -DOGRE_BUILD_COMPONENT_OVERLAY:BOOL=ON
                   -DOGRE_BUILD_COMPONENT_BITES:BOOL=OFF
                   -DOGRE_BUILD_SAMPLES:BOOL=OFF
                   -DOGRE_BUILD_TOOLS:BOOL=OFF
                   -DOGRE_BUILD_TESTS:BOOL=OFF
                   -DOGRE_INSTALL_SAMPLES:BOOL=OFF
                   -DOGRE_RESOURCEMANAGER_STRICT:STRING=0
  )
endif()

# ---------------------------------------------------------------------------
# OGRE-coupled plugins — must build AFTER ogre, and against the same version
# ---------------------------------------------------------------------------

if(TL_BUILD_PAGEDGEO)
  ExternalProject_Add(pagedgeometry
    # TODO(fork): point at the Trussline fork once it exists.
    GIT_REPOSITORY https://github.com/RigsOfRods/ogre-pagedgeometry.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    DEPENDS        ogre
    CMAKE_ARGS     ${TL_COMMON_ARGS}
  )
endif()

if(TL_BUILD_MYGUI)
  ExternalProject_Add(mygui
    GIT_REPOSITORY https://github.com/MyGUI/mygui.git
    GIT_TAG        MyGUI3.4.3   # TODO(fork): match conanfile.py pin (3.4.0 upstream)
    GIT_SHALLOW    TRUE
    DEPENDS        ogre
    CMAKE_ARGS     ${TL_COMMON_ARGS}
                   -DMYGUI_RENDERSYSTEM:STRING=3   # Ogre platform
                   -DMYGUI_BUILD_DEMOS:BOOL=OFF
                   -DMYGUI_BUILD_TOOLS:BOOL=OFF
                   -DMYGUI_BUILD_PLUGINS:BOOL=OFF
  )
endif()

if(TL_BUILD_CAELUM)
  # Left OFF by default: Caelum is LGPLv3 (a licensing liability, see RISKS track A),
  # it is Cg-exclusive, and Phase 6 replaces it with a modern PBR sky. Upstream is
  # independently moving to SkyX for the same reason (PR #3380).
  ExternalProject_Add(caelum
    GIT_REPOSITORY https://github.com/RigsOfRods/ogre-caelum.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    DEPENDS        ogre
    CMAKE_ARGS     ${TL_COMMON_ARGS}
  )
endif()

# ---------------------------------------------------------------------------
# License manifest — feeds the licensing cross-cutting track (ROADMAP track A).
# A dependency without a recorded license is a shipping blocker, not a footnote.
# ---------------------------------------------------------------------------
# TODO(fork): emit a generated manifest of every built dependency + its license
# file as a post-build step, so Phase 9's compliance audit is mechanical.
