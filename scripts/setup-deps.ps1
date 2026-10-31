<#
.SYNOPSIS
    Sets up everything OBS-VtuberBear.sln needs from outside the repo and records the paths in BearApps.user.props.
    DiscordBear is not one of them: its prebuilt copy is checked in under ExternalDependencies\DiscordBear.

.DESCRIPTION
    Run once after cloning; running it again is safe and only does what is still missing. CI runs this same script.

      1. git submodules: GoogleTest and psd_sdk, which the solution builds itself. psd_sdk gets
         ThirdParty\patches\psd_sdk.patch on top: fixes for files crafted to crash its parser
      2. NuGet packages: nlohmann.json
      3. OBS Studio at -ObsVersion: libobs only, plus OBS's prebuilt deps (Qt6). This is the slow part.
      4. Python -PythonVersion, found with the py launcher unless -PythonPath is given

    Paths already in BearApps.user.props are kept as long as they still point at something usable, so an OBS
    build or Python you set up yourself is never replaced, and nothing is downloaded for it. Only missing or
    broken paths are filled in, from -DepsDir (default: deps\ in the repo, which git ignores). Use -Force to
    ignore BearApps.user.props and set everything up from scratch.

    Needs: Visual Studio 2022 (or newer) with the C++ workload, CMake, git, and Python.

.EXAMPLE
    .\scripts\setup-deps.ps1
.EXAMPLE
    .\scripts\setup-deps.ps1 -SetObsVersion 33.1.0
.EXAMPLE
    .\scripts\setup-deps.ps1 -PythonPath C:\Python312 -DepsDir D:\bear-deps
#>
[CmdletBinding()]
param(
    [string]$DepsDir = (Join-Path $PSScriptRoot '..\deps'),
    # Keep this in line with what the plugin is meant to build against.
    [string]$ObsVersion = '33.0.0',
    # Move to a new OBS release: builds it, points OBSPATH and QTPATH at it (replacing whatever
    # BearApps.user.props had) and makes it the ObsVersion default above, which CI reads.
    [ValidatePattern('^\d+(\.\d+)+(-[\w.]+)?$')]
    [string]$SetObsVersion,
    [string]$PythonVersion = '3.12',
    # Build libobs with this Windows SDK instead of the one OBS's CMake preset asks for.
    [string]$WindowsSdk,
    # Use this Python install, even if BearApps.user.props names another.
    [string]$PythonPath,
    [string]$PropsPath = (Join-Path $PSScriptRoot '..\BearApps.user.props'),
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
if ($SetObsVersion) { $ObsVersion = $SetObsVersion }
$repo =(Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Step($message)
{
    Write-Host "==> $message" -ForegroundColor Cyan
}

# Runs a native command in a script block (so arguments like -t:restore reach it untouched) and stops on failure.
function Invoke-Checked([scriptblock]$command)
{
    & $command
    if ($LASTEXITCODE) { throw "Failed with exit code ${LASTEXITCODE}: $command" }
}

# Clones (or reuses) a repo at a release tag, fetching only that revision. The tag itself is fetched too:
# OBS's CMake reads its version from `git describe`.
function Get-Tag($url, $tag, $dir)
{
    if (-not (Test-Path (Join-Path $dir '.git')))
    {
        Invoke-Checked { git init --quiet $dir }
        Invoke-Checked { git -C $dir remote add origin $url }
    }

    Invoke-Checked { git -C $dir fetch --quiet --depth 1 --force origin "refs/tags/${tag}:refs/tags/${tag}" }
    Invoke-Checked { git -C $dir checkout --quiet --force "refs/tags/$tag" }
}

# The same checks PropertySheet2.props makes before building OBS-VtuberBear. OBSPATH is a build folder
# inside the OBS source; the plugin needs both its libs and the source's libobs headers.
function Test-ObsBuild($path) { $path -and (Test-Path (Join-Path $path 'libobs\RelWithDebInfo\obs.lib')) -and (Test-Path (Join-Path $path '..\libobs\obs.h')) }
function Test-Qt($path) { $path -and (Test-Path (Join-Path $path 'include')) }
function Test-Python($path) { $path -and (Test-Path (Join-Path $path 'include\Python.h')) }

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'Visual Studio not found (no vswhere.exe). Install Visual Studio with the C++ workload.' }
$msbuild = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio with the C++ workload.' }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git is not on PATH.' }

# ---- What is already set up ----

# Ordered, so rewriting the file keeps its properties in the same order.
$settings = [ordered]@{}
if ((Test-Path $PropsPath) -and -not $Force)
{
    [xml]$existing = Get-Content -Raw $PropsPath
    foreach ($group in @($existing.Project.PropertyGroup))
    {
        foreach ($node in $group.ChildNodes)
        {
            if ($node.NodeType -eq 'Element') { $settings[$node.Name] = $node.InnerText.Trim() }
        }
    }
    Step "Read $PropsPath"
}
$before = ($settings.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ';'

# ---- 1. Submodules ----

Step 'Submodules'
# Not recursive: neither GoogleTest nor psd_sdk needs its own submodules.
Invoke-Checked { git -C $repo submodule update --init }

# psd_sdk trusts the sizes a PSD declares, so a crafted file can crash or hang it. The patch holds our fixes, which are not in
# upstream. It goes onto the submodule's working tree and the pinned commit stays upstream's, so a submodule that shows as
# modified is expected. If the pin is ever moved, `git -C ThirdParty/psd_sdk checkout .` first, then run this again.
$psdSdk = Join-Path $repo 'ThirdParty\psd_sdk'
$psdPatch = Join-Path $repo 'ThirdParty\patches\psd_sdk.patch'
# Blobs in the submodule have CRLF line endings, which the checkout of a text=auto patch may or may not keep, hence --ignore-whitespace
$ErrorActionPreference = 'Continue'
git -C $psdSdk apply --ignore-whitespace --reverse --check $psdPatch 2>&1 | Out-Null
$psdPatched = $LASTEXITCODE -eq 0
$ErrorActionPreference = 'Stop'
if ($psdPatched) { Step 'psd_sdk: patch already applied' } else
{
    Step 'psd_sdk: applying ThirdParty\patches\psd_sdk.patch'
    Invoke-Checked { git -C $psdSdk apply --ignore-whitespace $psdPatch }
}

# ---- 2. NuGet packages ----

Step 'NuGet packages'
Invoke-Checked { & $msbuild (Join-Path $repo 'OBS-VtuberBear.sln') -t:restore -p:RestorePackagesConfig=true -p:Platform=x64 -nologo -verbosity:minimal }

# ---- 3. OBS ----

if (-not $SetObsVersion -and (Test-ObsBuild $settings['OBSPATH']) -and (Test-Qt $settings['QTPATH']))
{
    Step "OBS: using $($settings['OBSPATH'])"
}
else
{
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'cmake is not on PATH.' }
    New-Item -ItemType Directory -Force $DepsDir | Out-Null
    $obsSource = Join-Path (Resolve-Path $DepsDir).Path 'obs-studio'
    $obsBuild = Join-Path $obsSource 'build_x64'

    # The plugin includes libobs's headers from the source, so it has to be there even when the build is
    # (CI restores the build from a cache but not the source).
    $fetchedVersion = if (Test-Path (Join-Path $obsSource '.git')) { git -C $obsSource describe --tags --exact-match 2>$null }
    if (-not (Test-Path (Join-Path $obsSource 'libobs\obs.h')) -or ($fetchedVersion -and $fetchedVersion -ne $ObsVersion))
    {
        if ($fetchedVersion -and $fetchedVersion -ne $ObsVersion)
        {
            # -ObsVersion changed since the last run: the old build doesn't match the new source.
            Step "OBS: switching from $fetchedVersion to $ObsVersion"
            Remove-Item -Recurse -Force (Join-Path $obsSource 'build_x64'), (Join-Path $obsSource 'build_x86') -ErrorAction SilentlyContinue
        }

        Step "OBS ${ObsVersion}: fetching"
        Get-Tag 'https://github.com/obsproject/obs-studio.git' $ObsVersion $obsSource
    }

    if (Test-ObsBuild $obsBuild)
    {
        Step "OBS ${ObsVersion}: already built in $obsBuild (delete it to rebuild)"
    }
    else
    {
        Step "OBS ${ObsVersion}: building libobs (this takes a while the first time)"
        # Use whichever Visual Studio is installed; OBS's preset hardcodes one.
        $vsMajor = (& $vswhere -latest -prerelease -property installationVersion).Split('.')[0]
        $generator = @{ '17' = 'Visual Studio 17 2022'; '18' = 'Visual Studio 18 2026' }[$vsMajor]
        if (-not $generator) { throw "Unrecognised Visual Studio version $vsMajor" }

        # OBS's preset names the Windows SDK it builds with; say so up front rather than as a CMake error.
        $presetArchitecture = @((Get-Content -Raw (Join-Path $obsSource 'CMakePresets.json') | ConvertFrom-Json).configurePresets |
            Where-Object { $_.name -eq 'windows-x64' }).architecture
        $architecture = if ($WindowsSdk) { "x64,version=$WindowsSdk" } elseif ($null -ne $presetArchitecture.value) { $presetArchitecture.value } else { $presetArchitecture }
        $sdk = if ($architecture -match 'version=([\d.]+)') { $Matches[1] }
        if ($sdk -and -not (Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\$sdk"))
        {
            throw "OBS $ObsVersion builds with Windows SDK $sdk, which is not installed. Add it in the Visual Studio Installer (Individual components), or pass -WindowsSdk with one you have, e.g. -WindowsSdk 10.0.22621.0."
        }

        Push-Location $obsSource
        try
        {
            # Configuring reads parts of OBS's own submodules (e.g. libdshowcapture), even for a libobs-only build.
            Invoke-Checked { git submodule update --init --recursive --depth 1 --jobs 8 }

            # Configure downloads OBS's prebuilt deps (incl. Qt6) into .deps; only libobs is built.
            $sdkArguments = if ($WindowsSdk) { @('-A', $architecture) } else { @() }
            Invoke-Checked { cmake --preset windows-x64 -G $generator @sdkArguments -DENABLE_FRONTEND=OFF -DENABLE_PLUGINS=OFF -DENABLE_SCRIPTING=OFF -DENABLE_BROWSER=OFF }
            Invoke-Checked { cmake --build build_x64 --config RelWithDebInfo --target libobs }
        }
        finally
        {
            Pop-Location
        }
    }

    # Fill in only what was missing, a working path from the props file stays. -SetObsVersion replaces both.
    if ($SetObsVersion -or -not (Test-ObsBuild $settings['OBSPATH'])) { $settings['OBSPATH'] = $obsBuild }
    if ($SetObsVersion -or -not (Test-Qt $settings['QTPATH']))
    {
        $qt = Get-ChildItem (Join-Path $obsSource '.deps') -Directory -Filter 'obs-deps-qt6-*-x64' -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $qt) { throw "Qt6 deps not found under $obsSource\.deps" }
        $settings['QTPATH'] = $qt.FullName
    }
}

# ---- 4. Python ----

if ($PythonPath)
{
    $settings['PYTHONPATH'] = $PythonPath
}
elseif (-not (Test-Python $settings['PYTHONPATH']))
{
    if (-not (Get-Command py -ErrorAction SilentlyContinue)) { throw "Python launcher (py) not found. Install Python $PythonVersion or pass -PythonPath." }
    $found = & py "-$PythonVersion" -c 'import sys; print(sys.base_prefix)'
    if ($LASTEXITCODE -or -not $found) { throw "Python $PythonVersion not found. Install it or pass -PythonPath." }
    $settings['PYTHONPATH'] = $found
}
if (-not (Test-Python $settings['PYTHONPATH'])) { throw "$($settings['PYTHONPATH']) has no include\Python.h, is it a full Python install?" }
Step "Python: using $($settings['PYTHONPATH'])"

# ---- Per-machine props ----

$after = ($settings.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ';'
if ((Test-Path $PropsPath) -and -not $Force -and $before -eq $after)
{
    Step "$PropsPath is already up to date"
}
else
{
    Step "Writing $PropsPath"
    $lines = $settings.GetEnumerator() | ForEach-Object { "    <$($_.Key)>$([System.Security.SecurityElement]::Escape($_.Value))</$($_.Key)>" }
    $props = @(
        '<?xml version="1.0" encoding="utf-8"?>'
        '<!-- This machine''s dependency paths; not checked in. Written by scripts\setup-deps.ps1, which keeps any path here'
        '     that still works, so edit freely to point at your own builds. To have builds copied into OBS, add e.g.'
        '     <OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS>'
        '     Anything passed on the msbuild command line (/p:NAME=...) still wins over these. -->'
        '<Project>'
        '  <PropertyGroup>'
        $lines
        '  </PropertyGroup>'
        '</Project>'
    )
    Set-Content -Path $PropsPath -Value $props -Encoding utf8
}

# Only now that the new version has built, so a failed run leaves the old default in place.
if ($SetObsVersion)
{
    $scriptText = [System.IO.File]::ReadAllText($PSCommandPath)
    $updated = $scriptText -replace "(\[string\]\`$ObsVersion = ')[^']+'", "`${1}$SetObsVersion'"
    if ($updated -ne $scriptText)
    {
        Step "ObsVersion default is now $SetObsVersion"
        [System.IO.File]::WriteAllText($PSCommandPath, $updated)
    }
}

Step 'Done. Open OBS-VtuberBear.sln, or build with: msbuild OBS-VtuberBear.sln /p:Configuration=Release /p:Platform=x64'
