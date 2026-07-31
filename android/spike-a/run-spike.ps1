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

Write-Output "=== RENDER TARGET READBACK ==="
$rb = "C:\Users\nico1\Desktop\Rigs Port\spike-a\readback.ktx"
& $adb exec-out "run-as $pkg cat files/readback.ktx" > $rb 2>$null

if ((Test-Path $rb) -and ((Get-Item $rb).Length -gt 1000)) {
    $len = (Get-Item $rb).Length
    Write-Output "pulled readback.ktx ($([math]::Round($len/1KB,1)) KB)"

    # Sample the TAIL rather than a fixed offset: that is pixel data in any of
    # these container formats, so no header-size guessing is needed.
    #
    # A cleared frame is one flat colour, so 'uniform' is EXPECTED. The question
    # is only whether it is uniformly zero (black - clear colour never applied)
    # or uniformly something else (the clear colour really landed).
    $bytes = [System.IO.File]::ReadAllBytes($rb)
    $sample = [Math]::Min(60000, $bytes.Length - 16)
    $start = $bytes.Length - $sample
    $nonZero = 0
    $hist = @{}
    for ($i = $start; $i -lt $bytes.Length; $i++) {
        if ($bytes[$i] -ne 0) { $nonZero++ }
        $hist["$($bytes[$i])"] = 1
    }
    Write-Output "sampled $sample tail bytes: $nonZero non-zero ($([math]::Round(100*$nonZero/$sample,1))%)"
    Write-Output "distinct byte values: $($hist.Keys.Count)"
    Write-Output "last 16 bytes: $(($bytes[($bytes.Length-16)..($bytes.Length-1)] | ForEach-Object { $_.ToString('X2') }) -join ' ')"
    if ($nonZero -eq 0) {
        Write-Output "VERDICT: render target is BLACK - clear colour never applied"
    } else {
        Write-Output "VERDICT: render target holds NON-BLACK pixel data - clear colour applied"
    }
} else {
    Write-Output "readback.ktx not produced or empty"
}
