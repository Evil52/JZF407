# Chaos / production-style stress test for jtf407 firmware.
#
# Three parallel activities:
#   1. Ping  — mosquitto_rr every 200 ms, measures RTT
#   2. LED   — random LED toggles every 5 sec (real command traffic)
#   3. Chaos — Stop-Service mosquitto for 15 sec every 5 min, then Start
#
# Subscriber tracks status / diag / heartbeat to count reconnects and
# verify that fail-safe works as designed (heartbeat absent during outages).

param(
    [string]$Broker        = "192.168.137.1",
    [int]   $PingMs        = 200,
    [int]   $LedSec        = 5,
    [int]   $OutageEverySec = 300,   # 5 min
    [int]   $OutageDurSec  = 15,
    [double]$DurationHours = 2.0,
    [string]$MosquittoDir  = "C:\Program Files\mosquitto"
)

$ErrorActionPreference = 'Continue'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rttLog    = Join-Path $scriptDir "chaos.rtt.log"
$subLog    = Join-Path $scriptDir "chaos.sub.log"
$evtLog    = Join-Path $scriptDir "chaos.events.log"
$sumFile   = Join-Path $scriptDir "chaos.summary"

$rrExe  = Join-Path $MosquittoDir "mosquitto_rr.exe"
$pubExe = Join-Path $MosquittoDir "mosquitto_pub.exe"
$subExe = Join-Path $MosquittoDir "mosquitto_sub.exe"

$startMs = [int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds
$endMs   = $startMs + ([int64]($DurationHours * 3600 * 1000))

"# chaos test $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File $rttLog
Remove-Item $subLog -ErrorAction SilentlyContinue
"# events $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File $evtLog
"# summary $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') broker=$Broker dur=${DurationHours}h" | Out-File $sumFile

Write-Host "Chaos test: $DurationHours h"
Write-Host "  Ping:    every $PingMs ms"
Write-Host "  LED:     every $LedSec s (random)"
Write-Host "  Outage:  every $OutageEverySec s for $OutageDurSec s"
Write-Host "  Logs:    $scriptDir`n"

# Subscriber background process — captures status/diag/heartbeat
$subProc = Start-Process -FilePath $subExe `
    -ArgumentList @('-h', $Broker, '-t', 'stm32/status', '-t', 'stm32/diag', '-t', 'stm32/heartbeat', '-v') `
    -RedirectStandardOutput $subLog -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2

# Per-event accumulators
$id           = 0
$sent         = 0
$received     = 0
$timeouts     = 0
$rttSum       = [int64]0
$rttMin       = [int64]::MaxValue
$rttMax       = [int64]0
$rtts         = New-Object System.Collections.Generic.List[int]

$ledSent      = 0
$outagesDone  = 0

$lastLedMs    = $startMs
$lastOutageMs = $startMs
$lastHourlyMs = $startMs
$ledStates    = @{1=0; 2=0; 3=0}

function Log-Event([string]$msg) {
    $ts = (Get-Date -Format 'HH:mm:ss')
    "$ts  $msg" | Out-File -Append $evtLog
    Write-Host "[$ts] $msg"
}

Log-Event "START"

try {
    while ($true) {
        $nowMs = [int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds
        if ($nowMs -ge $endMs) { break }

        # ---- Ping/Pong ----
        $id++
        $payload = "$id"
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $resp = & $rrExe -h $Broker -t "stm32/ping" -e "stm32/pong" -m $payload -W 3 2>&1
        $sw.Stop()
        $sent++
        if ($resp -eq $payload) {
            $rtt = $sw.ElapsedMilliseconds
            $received++
            $rttSum += $rtt
            if ($rtt -lt $rttMin) { $rttMin = $rtt }
            if ($rtt -gt $rttMax) { $rttMax = $rtt }
            $rtts.Add($rtt)
            "$nowMs $id $rtt" | Out-File -Append $rttLog
        } else {
            $timeouts++
            "$nowMs $id TIMEOUT" | Out-File -Append $rttLog
        }

        # ---- Random LED command every LedSec ----
        if (($nowMs - $lastLedMs) -ge ($LedSec * 1000)) {
            $led = (1..3 | Get-Random)
            $newState = 1 - $ledStates[$led]
            $ledStates[$led] = $newState
            & $pubExe -h $Broker -t "stm32/led/$led" -m "$newState" 2>&1 | Out-Null
            $ledSent++
            $lastLedMs = $nowMs
        }

        # ---- Chaos: stop mosquitto for OutageDurSec every OutageEverySec ----
        if (($nowMs - $lastOutageMs) -ge ($OutageEverySec * 1000)) {
            Log-Event "OUTAGE start (stopping mosquitto for ${OutageDurSec}s)"
            Stop-Service mosquitto -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds $OutageDurSec
            Start-Service mosquitto -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 3   # let broker accept connections
            Log-Event "OUTAGE end (mosquitto restarted)"
            $outagesDone++
            $lastOutageMs = [int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds
        }

        # ---- Hourly progress ----
        if (($nowMs - $lastHourlyMs) -ge (3600 * 1000)) {
            $h    = [math]::Round(($nowMs - $startMs) / 3600000.0, 2)
            $loss = if ($sent -gt 0) { [math]::Round(100.0 * $timeouts / $sent, 3) } else { 0 }
            $avg  = if ($received -gt 0) { [math]::Round($rttSum / $received, 1) } else { 0 }
            Log-Event "[$h h] sent=$sent recv=$received loss=$loss% rtt_avg=${avg}ms led=$ledSent outages=$outagesDone"
            $lastHourlyMs = $nowMs
        }

        # Pace
        $elapsed = ([int64]([DateTime]::UtcNow - (Get-Date "1970-01-01")).TotalMilliseconds) - $nowMs
        $sleepMs = $PingMs - $elapsed
        if ($sleepMs -gt 0) { Start-Sleep -Milliseconds $sleepMs }
    }
}
finally {
    Log-Event "STOP"
    Stop-Process -Id $subProc.Id -Force -ErrorAction SilentlyContinue
    # Make sure mosquitto is running before we exit
    Start-Service mosquitto -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

# ---- Analysis ----
$onlineEvents     = 0
$offlineEvents    = 0
$heartbeats       = 0
$diagEvents       = @()

if (Test-Path $subLog) {
    Get-Content $subLog | ForEach-Object {
        if     ($_ -match '^stm32/status online')   { $onlineEvents++ }
        elseif ($_ -match '^stm32/status offline')  { $offlineEvents++ }
        elseif ($_ -match '^stm32/heartbeat')       { $heartbeats++ }
        elseif ($_ -match '^stm32/diag (.+)$')      { $diagEvents += $matches[1] }
    }
}

$loss   = if ($sent -gt 0) { [math]::Round(100.0 * $timeouts / $sent, 4) } else { 0 }
$avgRtt = if ($received -gt 0) { [math]::Round($rttSum / $received, 2) } else { 0 }
if ($rttMin -eq [int64]::MaxValue) { $rttMin = 0 }

$sorted = $rtts | Sort-Object
$p50  = if ($sorted.Count -gt 0) { $sorted[[int]($sorted.Count * 0.50)] } else { 0 }
$p95  = if ($sorted.Count -gt 0) { $sorted[[int]($sorted.Count * 0.95)] } else { 0 }
$p99  = if ($sorted.Count -gt 0) { $sorted[[int]($sorted.Count * 0.99)] } else { 0 }
$p999 = if ($sorted.Count -gt 0) { $sorted[[int]($sorted.Count * 0.999)] } else { 0 }

$summary = @"
=============== CHAOS TEST FINAL RESULTS ===============
duration:           $DurationHours hours
broker:             $Broker
ping interval:      $PingMs ms
LED interval:       $LedSec s
planned outages:    every $OutageEverySec s, $OutageDurSec s long

THROUGHPUT
  pings sent:       $sent
  pongs received:   $received
  timeouts:         $timeouts ($loss %)
  LED commands:     $ledSent
  outages executed: $outagesDone

LATENCY (ping → board → pong)
  min:     $rttMin ms
  p50:     $p50 ms
  avg:     $avgRtt ms
  p95:     $p95 ms
  p99:     $p99 ms
  p99.9:   $p999 ms
  max:     $rttMax ms

CONNECTION HEALTH
  online events:    $onlineEvents
  offline events:   $offlineEvents
  heartbeats recv:  $heartbeats
  reset events:     $($diagEvents.Count) :: $($diagEvents -join ', ')

EXPECTATION:
  - online events  ≈ 1 + outagesDone   (initial + 1 per outage)
  - offline events ≈ outagesDone       (LWT on every outage)
  - heartbeats     ≈ duration_sec / 10 - 12 * outagesDone (lost during outage)
========================================================
"@
Write-Host "`n$summary"
$summary | Out-File -Append $sumFile
