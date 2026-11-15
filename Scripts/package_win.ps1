# Builds, cooks and packages Director's Cut for Windows (Win64), then zips it for upload.
#
#   powershell -ExecutionPolicy Bypass -File Scripts\package_win.ps1          # Shipping build
#   powershell -ExecutionPolicy Bypass -File Scripts\package_win.ps1 -Dev     # Development build (logs + console)
#
# Output: Packaged\Windows\ (the game folder) and Packaged\DirectorsCut-Win64.zip
# A previous build is moved into Packaged\Previous-<timestamp>\, never deleted.
# Engine: pass -UERoot or set UE_ROOT to override the default install location.

param(
    [switch]$Dev,
    [string]$UERoot = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.9" })
)

$ErrorActionPreference = "Stop"

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UProject   = Join-Path $ProjectDir "DirectorsCut.uproject"
$RunUAT     = Join-Path $UERoot "Engine\Build\BatchFiles\RunUAT.bat"
$OutDir     = Join-Path $ProjectDir "Packaged"
$OutGame    = Join-Path $OutDir "Windows"
$Zip        = Join-Path $OutDir "DirectorsCut-Win64.zip"
$LimitBytes = 1GB   # jam limit: build must be under 1 GB

$Config = if ($Dev) { "Development" } else { "Shipping" }

function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Confirm-Continue([string]$warning) {
    Write-Host "WARNING: $warning" -ForegroundColor Yellow
    $ans = Read-Host "Continue anyway? [y/N]"
    if ($ans -notmatch '^[Yy]$') { exit 1 }
}

if (-not (Test-Path $RunUAT))   { Fail "RunUAT not found at: $RunUAT (pass -UERoot or set UE_ROOT)" }
if (-not (Test-Path $UProject)) { Fail "Project not found: $UProject" }

# Warn if the game would ship muted.
$GameIni = Join-Path $ProjectDir "Config\DefaultGame.ini"
if (Select-String -Path $GameIni -Pattern '^\s*bMuteAll\s*=\s*True' -Quiet) {
    Confirm-Continue "Config\DefaultGame.ini has bMuteAll=True, so the packaged game will be SILENT."
}

if (Get-Process -Name "UnrealEditor*" -ErrorAction SilentlyContinue) {
    Confirm-Continue "an Unreal Editor is running. Close it to avoid cook/asset lock issues."
}

Write-Host "==> Packaging DirectorsCut ($Config) for Win64"
$Start = Get-Date

& $RunUAT BuildCookRun `
    "-project=$UProject" `
    -noP4 -utf8output -unattended `
    -platform=Win64 `
    "-clientconfig=$Config" `
    -build -cook -stage -pak -iostore -compressed -prereqs
if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun failed (exit code $LASTEXITCODE)" }

# Every "/Game/..." path hard-coded in Source must have been cooked. Assets loaded
# by path from C++ are invisible to the cooker unless their folder is listed in
# DefaultGame.ini DirectoriesToAlwaysCook; missing ones render as grey/black.
$CookedSet = Join-Path $ProjectDir "Saved\Cooked\Windows\DirectorsCut\Metadata\ReferencedSet.txt"
if (-not (Test-Path $CookedSet)) { Fail "Cooked set not found: $CookedSet" }
$Cooked = Get-Content $CookedSet
$GamePaths = Get-ChildItem -Path (Join-Path $ProjectDir "Source") -Recurse -File |
    Select-String -Pattern '"/Game/[^"]+"' -AllMatches |
    ForEach-Object { $_.Matches } |
    ForEach-Object { ($_.Value.Trim('"')) -replace '\.[A-Za-z0-9_]+$', '' } |
    Sort-Object -Unique
$Missing = $false
foreach ($p in $GamePaths) {
    # Folder paths (e.g. /Game/DC/Audio) count as cooked if anything under them is.
    $rx = '^' + [regex]::Escape($p) + '(/|$)'
    if (-not ($Cooked | Where-Object { $_ -match $rx } | Select-Object -First 1)) {
        Write-Host "ERROR: not cooked: $p" -ForegroundColor Red
        $Missing = $true
    }
}
if ($Missing) { Fail "Add the folder(s) to DirectoriesToAlwaysCook in Config\DefaultGame.ini." }

# The complete, self-contained build is the staged one (same as on Mac).
$Staged = Join-Path $ProjectDir "Saved\StagedBuilds\Windows"
$Launcher = Join-Path $Staged "DirectorsCut.exe"
if (-not (Test-Path $Launcher)) { Fail "No staged build at $Staged (DirectorsCut.exe missing)" }
$GameExe = Get-ChildItem -Path (Join-Path $Staged "DirectorsCut\Binaries\Win64") -Filter "DirectorsCut*.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $GameExe) { Fail "Game executable missing under $Staged\DirectorsCut\Binaries\Win64" }
$Paks = Get-ChildItem -Path (Join-Path $Staged "DirectorsCut\Content\Paks") -Include *.pak, *.ucas, *.utoc -Recurse -ErrorAction SilentlyContinue
if (-not $Paks) { Fail "No .pak/.ucas/.utoc files in $Staged\DirectorsCut\Content\Paks" }

# Move any previous build aside instead of deleting it.
if ((Test-Path $OutGame) -or (Test-Path $Zip)) {
    $Prev = Join-Path $OutDir ("Previous-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
    New-Item -ItemType Directory -Path $Prev | Out-Null
    if (Test-Path $OutGame) { Move-Item $OutGame $Prev }
    if (Test-Path $Zip)     { Move-Item $Zip $Prev }
    Write-Host "==> Previous build moved to $Prev"
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
Write-Host "==> Copying staged build to $OutGame"
Copy-Item -Path $Staged -Destination $OutGame -Recurse

# tar.exe ships with Windows 10+ and handles large zips far faster than Compress-Archive.
Write-Host "==> Zipping to $Zip"
& tar.exe -a -c -f $Zip -C $OutDir "Windows"
if ($LASTEXITCODE -ne 0) { Fail "Zipping failed (exit code $LASTEXITCODE)" }

$Size = (Get-Item $Zip).Length
$SizeMB = [math]::Floor($Size / 1MB)
$Elapsed = (Get-Date) - $Start

Write-Host ""
Write-Host ("Done in {0}m {1}s" -f [math]::Floor($Elapsed.TotalMinutes), $Elapsed.Seconds)
Write-Host "  Game: $OutGame\DirectorsCut.exe"
Write-Host "  Zip:  $Zip ($SizeMB MB)"
if ($Size -ge $LimitBytes) {
    Write-Host "  WARNING: zip is over the 1 GB jam limit!" -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Note: the exe is unsigned. Windows SmartScreen may warn players the first time;"
Write-Host "  they click 'More info' > 'Run anyway'. If a player is missing the VC++ runtime,"
Write-Host "  the installer is in Engine\Extras\Redist\en-us\UEPrereqSetup_x64.exe inside the build."
