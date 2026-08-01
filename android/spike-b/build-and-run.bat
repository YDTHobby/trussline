@echo off
REM Spike B - build the solver benchmark for the phone and for the desktop
REM reference, then run both. Serial as arg 1 (wireless ADB serial is ip:port).

set "SERIAL=%~1"
if "%SERIAL%"=="" set "SERIAL=192.168.0.165:38977"

set "NDK=C:\Users\nico1\AppData\Local\Android\Sdk\ndk\27.3.13750724"
set "CLANG=%NDK%\toolchains\llvm\prebuilt\windows-x86_64\bin\clang++.exe"
set "SRC=C:\Users\nico1\Desktop\Rigs Port\spike-b\solver_bench.cpp"
set "OUT=C:\Users\nico1\Desktop\Rigs Port\spike-b"
set "ADB=C:\platform-tools\adb.exe"

echo === BUILD arm64 (matching RoR release flags: -O2 -ffast-math) ===
"%CLANG%" --target=aarch64-none-linux-android29 ^
  --sysroot="%NDK%/toolchains/llvm/prebuilt/windows-x86_64/sysroot" ^
  -O2 -ffast-math -std=c++17 -static-libstdc++ ^
  "%SRC%" -o "%OUT%\solver_bench_arm64"
if errorlevel 1 ( echo ARM64_BUILD_FAILED & exit /b 1 )
echo   built solver_bench_arm64

echo === PUSH AND RUN ON DEVICE ===
"%ADB%" -s %SERIAL% push "%OUT%\solver_bench_arm64" /data/local/tmp/solver_bench
"%ADB%" -s %SERIAL% shell chmod 755 /data/local/tmp/solver_bench
echo.
echo --- DEVICE CPU ---
"%ADB%" -s %SERIAL% shell "cat /proc/cpuinfo | grep -m1 'Hardware\|Processor'; nproc"
"%ADB%" -s %SERIAL% shell "for f in /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq; do cat $f; done"
echo.
echo --- BENCHMARK (device) ---
"%ADB%" -s %SERIAL% shell /data/local/tmp/solver_bench
echo DEVICE_EXIT=%ERRORLEVEL%
