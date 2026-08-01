# Spike B thermal soak. A 10-second benchmark says nothing about sustained
# performance; RISKS.md R-06 rates thermal throttling High/High on entry tier.
# This runs repeated passes and watches whether us/step degrades as the SoC heats.
param([string]$Serial = "192.168.0.165:38977", [int]$Passes = 6, [int]$Steps = 200000)

$adb = "C:\platform-tools\adb.exe"

function Get-Temp {
    $t = & $adb -s $Serial shell "cat /sys/class/thermal/thermal_zone0/temp" 2>$null
    if ("$t".Trim() -match '^\d+$') {
        $v = [double]"$t".Trim()
        if ($v -gt 1000) { $v = $v / 1000.0 }
        return [math]::Round($v, 1)
    }
    return $null
}

# NOTE: temp_start_C is often blank - many devices (this Xiaomi included)
# restrict /sys/class/thermal reads. That is not load-bearing: the throttling
# signal we actually care about is whether us/step degrades across passes.
Write-Output "pass,temp_start_C,daf_us,agora_us,typical_us,heavy_us,4x_us"

for ($i = 1; $i -le $Passes; $i++) {
    $temp = Get-Temp
    $out = & $adb -s $Serial shell "/data/local/tmp/solver_bench $Steps"

    # Anchor on the line START: "DAF semi" also matches "4x DAF semi", which
    # silently reported the 4-actor figure in the single-vehicle column.
    $daf = ($out | Select-String "^DAF semi").Line     | Select-Object -First 1
    $ago = ($out | Select-String "^Agora bus").Line    | Select-Object -First 1
    $typ = ($out | Select-String "^typical").Line      | Select-Object -First 1
    $hvy = ($out | Select-String "^heavy vehicle").Line| Select-Object -First 1
    $x4  = ($out | Select-String "^4x DAF").Line       | Select-Object -First 1

    function Field($line) {
        if (-not $line) { return "?" }
        $parts = ($line -split '\s+') | Where-Object { $_ -ne "" }
        return $parts[$parts.Count - 3]
    }

    Write-Output ("{0},{1},{2},{3},{4},{5},{6}" -f $i, $temp, (Field $daf), (Field $ago), (Field $typ), (Field $hvy), (Field $x4))
}

Write-Output ""
Write-Output "final temp: $(Get-Temp) C"
