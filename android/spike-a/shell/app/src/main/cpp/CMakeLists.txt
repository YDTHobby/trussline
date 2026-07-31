cmake_minimum_required(VERSION 3.22)
project(spikea LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OGRE was cross-compiled by spike-a/build-ogre-android.bat and installed to
# install-<abi>. CMAKE_PREFIX_PATH is supplied by app/build.gradle.
find_package(OGRE REQUIRED CONFIG)

message(STATUS "OGRE ${OGRE_VERSION} from ${OGRE_CONFIG_DIR}")
message(STATUS "OGRE_LIBRARIES: ${OGRE_LIBRARIES}")
message(STATUS "OGRE_STATIC: ${OGRE_STATIC}")

# The NDK ships native_app_glue as source, not a prebuilt library.
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(spikea SHARED main.cpp)

# Link the specific targets, NOT ${OGRE_LIBRARIES}.
#
# Two reasons, both discovered the hard way:
#
#  1. OGRE_LIBRARIES is a blob that drags in optional components (OgreBullet,
#     OgreVolume, ...) whose imported targets reference ZLIB::ZLIB and paths
#     under the build-tree Dependencies dir that were never installed. Linking
#     it fails at configure time.
#
#  2. OGRE_LIBRARIES does not even contain the Vulkan render system. OGREConfig
#     .cmake declares only `ogre_declare_plugin(RenderSystem GLES2)` for Android,
#     so Vulkan is exported as a target by OgreTargets.cmake but never advertised.
#     Same root cause as GLES2 building despite -DOGRE_BUILD_RENDERSYSTEM_GLES2=OFF:
#     OGRE's Android packaging assumes GLES2. Name the Vulkan target explicitly.
target_link_libraries(spikea
    PRIVATE
        native_app_glue
        OgreMain
        RenderSystem_Vulkan
        android
        log)

# NativeActivity resolves its entry point by name at load time; without this the
# linker drops it as unreferenced and the app dies with a cryptic "unable to
# find native library" at startup.
target_link_options(spikea PRIVATE -u ANativeActivity_onCreate)
