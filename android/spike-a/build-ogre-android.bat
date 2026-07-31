@echo off
REM Spike A - build OGRE 14.5.2 for Android with the Vulkan RenderSystem.
REM
REM THROWAWAY by design (ROADMAP 1.1): the deliverable is knowledge, not code.
REM Targets x86_64 because the reference phone is unavailable and the emulator
REM is the interim target (D-013). Everything here applies unchanged to
REM arm64-v8a - flip TL_ABI - but emulator results are FUNCTIONAL ONLY and
REM produce no Gate 1 evidence.

setlocal

REM ABI is arg 1, defaulting to x86_64. Pass arm64-v8a for the shipping ABI.
set "TL_ABI=%~1"
if "%TL_ABI%"=="" set "TL_ABI=x86_64"
set "TL_API=29"
set "NDK=C:\Users\nico1\AppData\Local\Android\Sdk\ndk\27.3.13750724"
set "SRC=C:\Users\nico1\Desktop\Rigs Port\spike-a\ogre"
set "BLD=C:\Users\nico1\Desktop\Rigs Port\spike-a\build-%TL_ABI%"
set "OUT=C:\Users\nico1\Desktop\Rigs Port\spike-a\install-%TL_ABI%"

set "PATH=C:\Strawberry\c\bin;%PATH%"

if not exist "%NDK%\build\cmake\android.toolchain.cmake" (
  echo NDK_TOOLCHAIN_MISSING at %NDK%
  exit /b 1
)

echo === CONFIGURE  abi=%TL_ABI% api=%TL_API% ===
cmake -S "%SRC%" -B "%BLD%" -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%NDK%/build/cmake/android.toolchain.cmake" ^
  -DANDROID_ABI=%TL_ABI% ^
  -DANDROID_PLATFORM=android-%TL_API% ^
  -DANDROID_STL=c++_shared ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INSTALL_PREFIX="%OUT%" ^
  -DOGRE_BUILD_DEPENDENCIES=ON ^
  -DOGRE_BUILD_RENDERSYSTEM_VULKAN=ON ^
  -DOGRE_BUILD_RENDERSYSTEM_GLES2=OFF ^
  -DOGRE_BUILD_RENDERSYSTEM_GL=OFF ^
  -DOGRE_BUILD_RENDERSYSTEM_GL3PLUS=OFF ^
  -DOGRE_BUILD_RENDERSYSTEM_TINY=OFF ^
  -DOGRE_BUILD_COMPONENT_RTSHADERSYSTEM=ON ^
  -DOGRE_BUILD_COMPONENT_TERRAIN=ON ^
  -DOGRE_BUILD_COMPONENT_OVERLAY=ON ^
  -DOGRE_BUILD_COMPONENT_BITES=OFF ^
  -DOGRE_BUILD_COMPONENT_JAVA=OFF ^
  -DOGRE_BUILD_COMPONENT_PYTHON=OFF ^
  -DOGRE_BUILD_COMPONENT_CSHARP=OFF ^
  -DOGRE_BUILD_SAMPLES=OFF ^
  -DOGRE_BUILD_TOOLS=OFF ^
  -DOGRE_BUILD_TESTS=OFF ^
  -DOGRE_BUILD_ANDROID_JNI_SAMPLE=OFF ^
  -DOGRE_INSTALL_SAMPLES=OFF ^
  -DOGRE_RESOURCEMANAGER_STRICT=0
echo CONFIGURE_EXIT=%ERRORLEVEL%
if errorlevel 1 exit /b 1

echo === BUILD ===
cmake --build "%BLD%" --parallel
echo BUILD_EXIT=%ERRORLEVEL%
if errorlevel 1 exit /b 1

echo === INSTALL ===
cmake --install "%BLD%"
echo INSTALL_EXIT=%ERRORLEVEL%
