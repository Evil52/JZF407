# 24-hour load test for STM32 MQTT firmware.
#
# Two background processes:
#  1) RR pinger: publishes to stm32/ping and waits for stm32/pong, measures RTT
#  2) Status listener: subscribes to stm32/status and stm32/diag, counts events
#
# Outputs:
#   loadtest.rtt.log    : "<unix_ms> <id> <rtt_ms|TIMEOUT>"
#   loadtest.sub.log    : raw subscriber output (topic + payload)
#   loadtest.summary    : human-readable hourly + final stats

param(
    [string]$Broker        = "192.168.137.1",
    [int]   $IntervalMs    = 200,
    [double]$DurationHours = 24.0,
    [int]   $TimeoutSec    = 3,
    [string]$MosquittoDir  = "C:\Program Files\mosquitto"
)

$ErrorActionPreference = 'Continue'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$rttLog  = Join-Path $scriptDir "loadtest.rtt.log"
$subLog  = Join-Path $scriptDir "loadtest.sub.log"
$sumFile = Join-Path $scriptDir "loadtest.summary"

$rrExe  = Join-Path $MosquittoDir "mosquitto_rr.exe"
$subExe = Join-Path $MosquittoDir "mosquitto_sub.exe"

$startMs = [int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds
$endMs   = $startMs + ([int64]($DurationHours * 3600 * 1000))

"# RTT log started $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File $rttLog
Remove-Item $subLog -ErrorAction SilentlyContinue
"# summary $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') broker=$Broker interval=${IntervalMs}ms dur=${DurationHours}h" | Out-File $sumFile

Write-Host "Load test: $DurationHours h, interval ${IntervalMs}ms, timeout ${TimeoutSec}s"
Write-Host "RTT log : $rttLog"
Write-Host "Sub log : $subLog"
Write-Host "Summary : $sumFile"

# Subscriber as separate process (status/diag only). -v gives "topic payload"
$subProc = Start-Process -FilePath $subExe `
    -ArgumentList @('-h', $Broker, '-t', 'stm32/status', '-t', 'stm32/diag', '-v') `
    -RedirectStandardOutput $subLog -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

# Main RR loop
$id          = 0
$sent        = 0
$received    = 0
$timeouts    = 0
$rttSum      = [int64]0
$rttMin      = [int64]::MaxValue
$rttMax      = [int64]0
$lastHourly  = $startMs

try {
    while ($true) {
        $nowMs = [int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds
        if ($nowMs -ge $endMs) { break }

        $id++
        $payload = "$id"
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $resp = & $rrExe -h $Broker -t "stm32/ping" -e "stm32/pong" -m $payload -W $TimeoutSec 2>&1
        $sw.Stop()
        $sent++

        if ($resp -eq $payload) {
            $rtt = $sw.ElapsedMilliseconds
            $received++
            $rttSum += $rtt
            if ($rtt -lt $rttMin) { $rttMin = $rtt }
            if ($rtt -gt $rttMax) { $rttMax = $rtt }
            "$nowMs $id $rtt" | Out-File -Append $rttLog
        } else {
            $timeouts++
            "$nowMs $id TIMEOUT" | Out-File -Append $rttLog
        }

        # Hourly summary
        if (($nowMs - $lastHourly) -ge (3600 * 1000)) {
            $h = [math]::Round(($nowMs - $startMs) / 3600000.0, 2)
            $loss = if ($sent -gt 0) { [math]::Round(100.0 * $timeouts / $sent, 3) } else { 0 }
            $avg  = if ($received -gt 0) { [math]::Round($rttSum / $received, 1) } else { 0 }
            $line = "[$(Get-Date -Format 'HH:mm:ss')] elapsed=${h}h sent=$sent recv=$received loss=${loss}% rtt_avg=${avg}ms rtt_max=${rttMax}ms"
            Write-Host $line
            $line | Out-File -Append $sumFile
            $lastHourly = $nowMs
        }

        # Pace
        $elapsed = ([int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds) - $nowMs
        $sleepMs = $IntervalMs - $elapsed
        if ($sleepMs -gt 0) { Start-Sleep -Milliseconds $sleepMs }
    }
}
finally {
    Stop-Process -Id $subProc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

# Final analysis
$loss   = if ($sent -gt 0) { [math]::Round(100.0 * $timeouts / $sent, 3) } else { 0 }
$avgRtt = if ($received -gt 0) { [math]::Round($rttSum / $received, 1) } else { 0 }
if ($rttMin -eq [int64]::MaxValue) { $rttMin = 0 }

$online = 0
$resets = New-Object System.Collections.Generic.List[String]
if (Test-Path $subLog) {
    Get-Content $subLog | ForEach-Object {
        if ($_ -match '^stm32/status online') { $online++ }
        elseif ($_ -match '^stm32/diag (.+)$') { $resets.Add($matches[1]) }
    }
}

$summary = @"
================== LOAD TEST SUMMARY ==================
duration:       $DurationHours hours
interval:       $IntervalMs ms
pings sent:     $sent
pongs received: $received
timeouts:       $timeouts ($loss %)
RTT min:        $rttMin ms
RTT avg:        $avgRtt ms
RTT max:        $rttMax ms
online events:  $online   (1 = initial, >1 = reconnects)
reset events:   $($resets.Count): $($resets -join ', ')
=======================================================
"@
Write-Host $summary
$summary | Out-File -Append $sumFile
