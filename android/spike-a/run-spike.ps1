# Spike A - install and run the shell on the emulator, then report the milestones.
$adb = "C:\platform-tools\adb.exe"
$apk = "C:\Users\nico1\Desktop\Rigs Port\spike-a\shell\app\build\outputs\apk\debug\app-debug.apk"
$pkg = "org.trussline.spikea"

Write-Output "=== WAITING FOR DEVICE ==="
& $adb wait-for-device

Write-Output "=== WAITING FOR BOOT ==="
$booted = $false
for ($i = 0; $i -lt 90; $i++) {
    $b = (& $adb shell getprop sys.boot_completed 2>$null)
    if ("$b".Trim() -eq "1") { $booted = $true; break }
    Start-Sleep -Seconds 5
}
if (-not $booted) { Write-Output "BOOT_TIMEOUT"; exit 1 }
Write-Output "booted after ~$($i*5)s"

Write-Output "=== DEVICE INFO ==="
& $adb shell getprop ro.build.version.release
& $adb shell getprop ro.product.cpu.abi
Write-Output "--- vulkan feature ---"
& $adb shell "pm list features | grep -i vulkan"

Write-Output "=== INSTALL ==="
& $adb install -r -t "$apk"
Write-Output "INSTALL_EXIT=$LASTEXITCODE"

Write-Output "=== LAUNCH ==="
& $adb logcat -c
& $adb shell am start -n "$pkg/android.app.NativeActivity"
Start-Sleep -Seconds 25

Write-Output "=== LOGCAT (TrusslineSpikeA) ==="
& $adb logcat -d -s TrusslineSpikeA:V

Write-Output "=== CRASHES / NATIVE ERRORS ==="
& $adb logcat -d -s AndroidRuntime:E DEBUG:F libc:F

Write-Output "=== PROCESS ALIVE? ==="
$p = & $adb shell "pidof $pkg"
if ("$p".Trim()) { Write-Output "ALIVE pid=$p" } else { Write-Output "NOT RUNNING" }

# Pixel verification happens inside the app now: main.cpp reads the framebuffer
# back with copyContentsToMemory and logs MILESTONE 3b / CONTROL lines with a
# verdict. (The file-based readback this section used to do is a dead end on
# this build: no PNG codec, DDS rejects non-power-of-two, KTX cannot encode.)
Write-Output "=== VISUAL CHECK ==="
& $adb shell screencap -p /sdcard/spikea.png
& $adb pull /sdcard/spikea.png "C:\Users\nico1\Desktop\Rigs Port\spike-a\spikea-latest.png"
Write-Output "screenshot pulled to spike-a\spikea-latest.png"
