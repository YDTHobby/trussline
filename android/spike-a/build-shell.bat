@echo off
REM Spike A - build the throwaway app shell.

set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot"
set "ANDROID_HOME=C:\Users\nico1\AppData\Local\Android\Sdk"
set "GRADLE=C:\Users\nico1\.gradle\wrapper\dists\gradle-8.14.3-all\10utluxaxniiv4wxiphsi49nj\gradle-8.14.3\bin\gradle.bat"

cd /d "C:\Users\nico1\Desktop\Rigs Port\spike-a\shell"

call "%GRADLE%" :app:assembleDebug --no-daemon --console=plain
echo GRADLE_EXIT=%ERRORLEVEL%
