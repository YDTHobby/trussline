@echo off
REM Spike A - build the throwaway app shell.
REM ABI is arg 1, defaulting to x86_64. Pass arm64-v8a for the shipping ABI.

set "TL_ABI=%~1"
if "%TL_ABI%"=="" set "TL_ABI=x86_64"

set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot"
set "ANDROID_HOME=C:\Users\nico1\AppData\Local\Android\Sdk"
set "GRADLE=C:\Users\nico1\.gradle\wrapper\dists\gradle-8.14.3-all\10utluxaxniiv4wxiphsi49nj\gradle-8.14.3\bin\gradle.bat"

cd /d "C:\Users\nico1\Desktop\Rigs Port\spike-a\shell"

echo === BUILDING FOR %TL_ABI% ===
call "%GRADLE%" :app:assembleDebug -PtrusslineAbi=%TL_ABI% --no-daemon --console=plain
echo GRADLE_EXIT=%ERRORLEVEL%
