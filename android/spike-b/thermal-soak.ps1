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

Write-Output "pass,temp_start_C,daf_us,agora_us,typical_us,heavy_us"

for ($i = 1; $i -le $Passes; $i++) {
    $temp = Get-Temp
    $out = & $adb -s $Serial shell "/data/local/tmp/solver_bench $Steps"

    $daf = ($out | Select-String "DAF semi").Line
    $ago = ($out | Select-String "Agora bus").Line
    $typ = ($out | Select-String "typical").Line
    $hvy = ($out | Select-String "heavy vehicle").Line

    function Field($line) {
        if (-not $line) { return "?" }
        $parts = ($line -split '\s+') | Where-Object { $_ -ne "" }
        return $parts[$parts.Count - 3]
    }

    Write-Output ("{0},{1},{2},{3},{4},{5}" -f $i, $temp, (Field $daf), (Field $ago), (Field $typ), (Field $hvy))
}

Write-Output ""
Write-Output "final temp: $(Get-Temp) C"
