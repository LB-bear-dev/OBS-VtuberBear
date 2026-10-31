<#
.SYNOPSIS
    Measures what each OBS scene costs: frame render time, CPU, RAM and VRAM, split by process.

.DESCRIPTION
    Puts each of -Scenes on program through obs-websocket, lets it settle for -Warmup seconds, then samples once a
    second for -Seconds:
      - OBS's own stats: average frame render time, and frames lagged while the scene was up
      - every process under obs64.exe, grouped as
          OBS       obs64.exe: the render thread, native source drawing and browser texture copies
          Browser   obs-browser-page.exe: the CEF processes behind browser sources and browser docks
          WebView2  msedgewebview2.exe: the StreamKit page OBS-VtuberBear hosts for speaking state
          Other     anything else
        with CPU (percent of one core), private memory, dedicated GPU memory and 3D engine use

    Render time and CPU follow the scene on program. Memory is for the whole process tree, because sources in other
    scenes stay loaded: browser sources unless "Shutdown source when not visible" is ticked, and characters keep
    their textures for as long as their person is in the call. Compare memory by group (Browser is all browser
    sources, WebView2 is all StreamKit) rather than by scene.

    Before running:
      1. Tools > WebSocket Server Settings: tick Enable WebSocket server. Pass its password with -Password or the
         OBS_WEBSOCKET_PASSWORD environment variable, or untick Enable Authentication.
      2. Make one scene per setup, with the same characters on screen in each: an empty baseline, one with the
         fugi.tech browser sources, one with OBS-VtuberBear characters.
      3. Close browser docks, projectors and the Stats dock so they don't add their own cost, and keep the preview
         and studio mode the same for every run.
    Run once with the call silent and once with people talking (-Label idle, -Label talking), since both setups do
    more work while someone speaks.

    Raw samples and the summary go to CSVs in -OutDir.

.EXAMPLE
    .\scripts\measure-performance.ps1
    Lists OBS's scenes.
.EXAMPLE
    .\scripts\measure-performance.ps1 -Scenes 'Perf Empty','Perf Fugi','Perf Plugin' -Label talking
#>
[CmdletBinding()]
param(
    # Scenes to measure, in order. Leave out to list OBS's scenes.
    [string[]]$Scenes,
    # Seconds sampled per scene.
    [ValidateRange(5, 3600)]
    [int]$Seconds = 60,
    # Seconds each scene runs before sampling, so its sources finish loading.
    [ValidateRange(0, 600)]
    [int]$Warmup = 10,
    # Tags this run in the CSVs, e.g. idle or talking.
    [string]$Label = 'run',
    [int]$Port = 4455,
    [string]$Password = $env:OBS_WEBSOCKET_PASSWORD,
    [string]$OutDir = (Join-Path ([IO.Path]::GetTempPath()) "obs-perf\$(Get-Date -Format yyyyMMdd-HHmmss)")
)

$ErrorActionPreference = 'Stop'
$clock = [Diagnostics.Stopwatch]::StartNew()
$processRows = [Collections.Generic.List[object]]::new()
$statRows = [Collections.Generic.List[object]]::new()
$lagRows = [Collections.Generic.List[object]]::new()

function Step($message)
{
    Write-Host "==> $message" -ForegroundColor Cyan
}

# obs-websocket protocol 5: https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md

function Get-Sha256Base64([string]$text)
{
    [Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($text)))
}

function Send-ObsMessage($socket, [int]$op, $data)
{
    $bytes = [Text.Encoding]::UTF8.GetBytes((@{ op = $op; d = $data } | ConvertTo-Json -Depth 8 -Compress))
    $socket.SendAsync([ArraySegment[byte]]::new($bytes), [Net.WebSockets.WebSocketMessageType]::Text, $true,
        [Threading.CancellationToken]::None).GetAwaiter().GetResult()
}

function Receive-ObsMessage($socket)
{
    $buffer = [byte[]]::new(65536)
    $message = [IO.MemoryStream]::new()
    do
    {
        $result = $socket.ReceiveAsync([ArraySegment[byte]]::new($buffer), [Threading.CancellationToken]::None).GetAwaiter().GetResult()
        if ($result.MessageType -eq [Net.WebSockets.WebSocketMessageType]::Close)
        {
            # 4009 is AuthenticationFailed
            throw "OBS closed the connection: $([int]$socket.CloseStatus) $($socket.CloseStatusDescription)"
        }
        $message.Write($buffer, 0, $result.Count)
    } while (-not $result.EndOfMessage)
    [Text.Encoding]::UTF8.GetString($message.ToArray()) | ConvertFrom-Json
}

function Connect-Obs([int]$port, [string]$password)
{
    $socket = [Net.WebSockets.ClientWebSocket]::new()
    $socket.Options.AddSubProtocol('obswebsocket.json')
    try
    {
        $socket.ConnectAsync([uri]"ws://127.0.0.1:$port", [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    }
    catch
    {
        throw "Couldn't reach obs-websocket on port ${port} ($($_.Exception.Message)). Enable it in Tools > WebSocket Server Settings."
    }

    $hello = Receive-ObsMessage $socket
    $identify = @{ rpcVersion = 1; eventSubscriptions = 0 }
    if ($hello.d.authentication)
    {
        if (-not $password) { throw 'obs-websocket wants a password: pass -Password or set OBS_WEBSOCKET_PASSWORD.' }
        $secret = Get-Sha256Base64 ($password + $hello.d.authentication.salt)
        $identify.authentication = Get-Sha256Base64 ($secret + $hello.d.authentication.challenge)
    }
    Send-ObsMessage $socket 1 $identify
    # Identified. A wrong password closes the connection instead.
    $null = Receive-ObsMessage $socket
    $socket
}

function Invoke-ObsRequest($socket, [string]$type, [hashtable]$data = @{})
{
    $id = [guid]::NewGuid().ToString()
    Send-ObsMessage $socket 6 @{ requestType = $type; requestId = $id; requestData = $data }
    while ($true)
    {
        $message = Receive-ObsMessage $socket
        if ($message.op -ne 7 -or $message.d.requestId -ne $id) { continue }
        if (-not $message.d.requestStatus.result) { throw "$type failed: $($message.d.requestStatus.comment)" }
        return $message.d.responseData
    }
}

# Sampling

function Get-GroupName([string]$name)
{
    switch ($name)
    {
        'obs64.exe' { 'OBS' }
        'obs-browser-page.exe' { 'Browser' }
        'msedgewebview2.exe' { 'WebView2' }
        default { 'Other' }
    }
}

# The root process and every process it started, directly or not.
function Get-ProcessTree([int]$rootId)
{
    $all = Get-CimInstance Win32_Process -Property ProcessId, ParentProcessId, Name, CreationDate
    $children = $all | Group-Object ParentProcessId -AsHashTable -AsString
    $tree = [Collections.Generic.List[object]]::new()
    $pending = [Collections.Generic.Queue[object]]::new()
    $root = $all | Where-Object ProcessId -eq $rootId
    if (-not $root) { return $tree }
    $pending.Enqueue($root)
    while ($pending.Count)
    {
        $process = $pending.Dequeue()
        $tree.Add($process)
        foreach ($child in $children["$($process.ProcessId)"])
        {
            # A child older than its parent is an orphan whose parent's id got reused
            if ($child.CreationDate -ge $process.CreationDate -and $child.ProcessId -ne $process.ProcessId) { $pending.Enqueue($child) }
        }
    }
    $tree
}

# Per process id: 3D engine use (percent, over the second this takes) and dedicated GPU memory (bytes).
function Get-GpuUsage
{
    $usage = @{}
    $samples = (Get-Counter '\GPU Engine(*engtype_3D)\Utilization Percentage', '\GPU Process Memory(*)\Dedicated Usage' -ErrorAction SilentlyContinue).CounterSamples
    foreach ($sample in $samples)
    {
        if ($sample.InstanceName -notmatch '^pid_(\d+)_') { continue }
        $id = [int]$Matches[1]
        if (-not $usage.ContainsKey($id)) { $usage[$id] = [pscustomobject]@{ Gpu3D = 0.0; Vram = 0.0 } }
        if ($sample.Path -like '*utilization percentage') { $usage[$id].Gpu3D += $sample.CookedValue }
        else { $usage[$id].Vram += $sample.CookedValue }
    }
    $usage
}

function Measure-Scene($socket, [string]$scene, [int]$obsId)
{
    $cpuBefore = @{}
    $before = 0
    $firstStats = $null
    # Sample 0 only sets the CPU baseline
    for ($sample = 0; $sample -le $Seconds; $sample++)
    {
        # Takes about a second, which paces the loop
        $gpu = Get-GpuUsage
        $now = $clock.Elapsed.TotalMilliseconds
        $stats = Invoke-ObsRequest $socket GetStats
        if (-not $firstStats) { $firstStats = $stats }

        $cpuNow = @{}
        foreach ($entry in Get-ProcessTree $obsId)
        {
            $process = Get-Process -Id $entry.ProcessId -ErrorAction SilentlyContinue
            if (-not $process) { continue }
            $id = [int]$entry.ProcessId
            $cpuNow[$id] = $process.TotalProcessorTime.TotalMilliseconds
            if ($sample -eq 0 -or -not $cpuBefore.ContainsKey($id)) { continue }

            $processRows.Add([pscustomobject]@{
                Label = $Label
                Scene = $scene
                Sample = $sample
                Group = Get-GroupName $entry.Name
                ProcessId = $id
                Name = $entry.Name
                CpuPercent = [math]::Round(($cpuNow[$id] - $cpuBefore[$id]) / ($now - $before) * 100, 2)
                PrivateMB = [math]::Round($process.PrivateMemorySize64 / 1MB, 1)
                VramMB = [math]::Round($(if ($gpu[$id]) { $gpu[$id].Vram } else { 0 }) / 1MB, 1)
                Gpu3DPercent = [math]::Round($(if ($gpu[$id]) { $gpu[$id].Gpu3D } else { 0 }), 2)
            })
        }

        if ($sample -gt 0)
        {
            $statRows.Add([pscustomobject]@{
                Label = $Label
                Scene = $scene
                Sample = $sample
                RenderMs = [math]::Round($stats.averageFrameRenderTime, 3)
                ActiveFps = [math]::Round($stats.activeFps, 2)
                ObsCpuPercent = [math]::Round($stats.cpuUsage, 2)
                ObsMemoryMB = [math]::Round($stats.memoryUsage, 1)
            })
        }
        $cpuBefore = $cpuNow
        $before = $now
    }

    $lagRows.Add([pscustomobject]@{
        Scene = $scene
        FramesLagged = $stats.renderSkippedFrames - $firstStats.renderSkippedFrames
        FramesRendered = $stats.renderTotalFrames - $firstStats.renderTotalFrames
    })
}

function Get-Percentile([double[]]$values, [double]$percentile)
{
    $sorted = $values | Sort-Object
    $sorted[[math]::Min($sorted.Count - 1, [math]::Floor($percentile / 100 * $sorted.Count))]
}

# Mean over samples of each group's total, so a group that had no processes in a sample counts as zero there.
function Get-GroupSummary([string]$scene)
{
    $rows = $processRows | Where-Object Scene -eq $scene
    foreach ($group in 'OBS', 'Browser', 'WebView2', 'Other', 'Total')
    {
        $groupRows = if ($group -eq 'Total') { $rows } else { $rows | Where-Object Group -eq $group }
        if (-not $groupRows) { continue }
        $totals = foreach ($sample in 1..$Seconds)
        {
            $sampleRows = $groupRows | Where-Object Sample -eq $sample
            [pscustomobject]@{
                Cpu = ($sampleRows | Measure-Object CpuPercent -Sum).Sum
                Private = ($sampleRows | Measure-Object PrivateMB -Sum).Sum
                Vram = ($sampleRows | Measure-Object VramMB -Sum).Sum
                Gpu = ($sampleRows | Measure-Object Gpu3DPercent -Sum).Sum
            }
        }
        [pscustomobject]@{
            Scene = $scene
            Group = $group
            'CPU % core' = [math]::Round(($totals | Measure-Object Cpu -Average).Average, 2)
            'RAM MB' = [math]::Round(($totals | Measure-Object Private -Average).Average, 0)
            'VRAM MB' = [math]::Round(($totals | Measure-Object Vram -Average).Average, 0)
            'GPU 3D %' = [math]::Round(($totals | Measure-Object Gpu -Average).Average, 2)
        }
    }
}

# Dot-sourcing loads the functions only
if ($MyInvocation.InvocationName -eq '.') { return }

$obs = Get-Process obs64 -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $obs) { throw 'OBS is not running.' }

$socket = Connect-Obs $Port $Password
try
{
    $sceneList = Invoke-ObsRequest $socket GetSceneList
    $names = @($sceneList.scenes.sceneName)
    if (-not $Scenes)
    {
        Write-Host 'Scenes in OBS:'
        $names | Sort-Object | ForEach-Object { Write-Host "  $_" }
        return
    }
    $missing = $Scenes | Where-Object { $_ -notin $names }
    if ($missing) { throw "OBS has no scene named: $($missing -join ', ')" }

    $video = Invoke-ObsRequest $socket GetVideoSettings
    $budget = 1000 * $video.fpsDenominator / $video.fpsNumerator
    $original = $sceneList.currentProgramSceneName
    try
    {
        foreach ($scene in $Scenes)
        {
            Step "${scene}: warming up for $Warmup s, then sampling for $Seconds s"
            $null = Invoke-ObsRequest $socket SetCurrentProgramScene @{ sceneName = $scene }
            Start-Sleep -Seconds $Warmup
            Measure-Scene $socket $scene $obs.Id
        }
    }
    finally
    {
        $null = Invoke-ObsRequest $socket SetCurrentProgramScene @{ sceneName = $original }
    }

    $render = foreach ($scene in $Scenes)
    {
        $times = ($statRows | Where-Object Scene -eq $scene).RenderMs
        $lag = $lagRows | Where-Object Scene -eq $scene
        [pscustomobject]@{
            Label = $Label
            Scene = $scene
            'Render ms median' = Get-Percentile $times 50
            'Render ms p95' = Get-Percentile $times 95
            'Frame budget ms' = [math]::Round($budget, 2)
            'Frames lagged' = "$($lag.FramesLagged) of $($lag.FramesRendered)"
        }
    }
    $groups = foreach ($scene in $Scenes) { Get-GroupSummary $scene }

    $null = New-Item -ItemType Directory -Force $OutDir
    $processRows | Export-Csv (Join-Path $OutDir "$Label-processes.csv") -NoTypeInformation
    $statRows | Export-Csv (Join-Path $OutDir "$Label-obs-stats.csv") -NoTypeInformation
    $render | Export-Csv (Join-Path $OutDir "$Label-render.csv") -NoTypeInformation
    $groups | Export-Csv (Join-Path $OutDir "$Label-groups.csv") -NoTypeInformation

    Step 'Render time (OBS graphics thread, per frame)'
    $render | Format-Table -AutoSize | Out-Host
    Step 'Process groups (CPU and GPU follow the scene on program; memory is the whole tree)'
    $groups | Format-Table -AutoSize | Out-Host
    Step "CSVs in $OutDir"
}
finally
{
    if ($socket.State -eq [Net.WebSockets.WebSocketState]::Open)
    {
        $socket.CloseAsync([Net.WebSockets.WebSocketCloseStatus]::NormalClosure, '', [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    }
    $socket.Dispose()
}
