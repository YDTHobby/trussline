# Spike A on real hardware. Serial as arg 1 (wireless ADB serial is ip:port).
param([string]$Serial = "192.168.0.165:38977")

$adb = "C:\platform-tools\adb.exe"
$apk = "C:\Users\nico1\Desktop\Rigs Port\spike-a\shell\app\build\outputs\apk\debug\app-debug.apk"
$pkg = "org.trussline.spikea"

Write-Output "=== TARGET ==="
& $adb -s $Serial shell "getprop ro.product.model; getprop ro.soc.model; getprop ro.build.version.release"

Write-Output "=== INSTALL (arm64) ==="
& $adb -s $Serial install -r -t "$apk"
Write-Output "INSTALL_EXIT=$LASTEXITCODE"

Write-Output "=== LAUNCH ==="
& $adb -s $Serial logcat -c
& $adb -s $Serial shell am start -n "$pkg/android.app.NativeActivity"
Start-Sleep -Seconds 30

Write-Output "=== LOGCAT ==="
& $adb -s $Serial logcat -d -s TrusslineSpikeA:V

Write-Output "=== CRASHES ==="
& $adb -s $Serial logcat -d -s AndroidRuntime:E DEBUG:F libc:F CRASH:E

Write-Output "=== ALIVE? ==="
$p = & $adb -s $Serial shell "pidof $pkg"
if ("$p".Trim()) { Write-Output "ALIVE pid=$($p.Trim())" } else { Write-Output "NOT RUNNING" }

Write-Output "=== VULKAN DEVICE (from Ogre.log) ==="
& $adb -s $Serial shell "run-as $pkg cat files/Ogre.log" 2>$null | Select-String -Pattern "Vulkan|Vendor ID|Device ID|API Version|Driver" | Select-Object -First 10

Write-Output "=== SCREENSHOT ==="
& $adb -s $Serial shell screencap -p /sdcard/spikea_device.png
& $adb -s $Serial pull /sdcard/spikea_device.png "C:\Users\nico1\Desktop\Rigs Port\spike-a\spikea-device.png"
