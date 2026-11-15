# Takes the store / README screenshots of Director's Cut on Windows.
#
#   powershell -ExecutionPolicy Bypass -File Scripts\screenshots_win.ps1                    # every scene, 1920x1080
#   powershell -ExecutionPolicy Bypass -File Scripts\screenshots_win.ps1 -Only menu,late     # just those scenes
#   powershell -ExecutionPolicy Bypass -File Scripts\screenshots_win.ps1 -List               # show the scenes and shots
#   powershell -ExecutionPolicy Bypass -File Scripts\screenshots_win.ps1 -Width 2560 -Height 1440
#
# The game is launched once per scene. Each launch puts the game in the state it needs with the
# dev flags in Source\DirectorsCut\Core\DCDevTools.h (a bot plays the run forward, the camera and
# panels are set per shot), saves the PNGs and quits by itself. Nothing is clicked or typed, so
# leave the mouse alone while a game window is open.
#
# Output: Saved\Screenshots\<timestamp>\*.png (a new folder every time), logs in its logs\ folder.
#
# Your own saves and leaderboard are not touched: the runs save under the "DCShot" prefix. The
# title screen shows a sample leaderboard and sample save slots.
#
# By default the game runs from source through the editor (UnrealEditor.exe -game), which is built
# first. To shoot a packaged build instead pass -Exe; it must be a Development build
# (Scripts\package_win.ps1 -Dev), because a Shipping build ignores the settings overrides and the
# sample leaderboard.
# Engine: pass -UERoot or set UE_ROOT to override the default install location.

param(
    [string[]]$Only,                 # scene names (see -List); default = all
    [switch]$List,                   # print the scenes and their shots, then exit
    [int]$Width = 1920,
    [int]$Height = 1080,
    [int]$Seed = 7,                  # run seed for the gameplay scenes (same seed = same crew, jobs and house)
    [int]$ShotDelay = 8,             # seconds before a scene starts shooting (raise it if shaders are still compiling)
    [int]$TimeoutSec = 600,          # a scene that runs longer than this is stopped
    [switch]$Fullscreen,             # default is a window; use this if the window does not fit your monitor
    [switch]$NoBuild,                # skip building the editor target
    [string]$Exe,                    # packaged Development build to shoot instead (e.g. Packaged\Windows\DirectorsCut.exe)
    [string]$OutDir,                 # default: Saved\Screenshots\<timestamp>
    [string]$UERoot = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.9" })
)

$ErrorActionPreference = "Stop"

function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }

# --- Scenes -------------------------------------------------------------------
# Shots are "name:focus[:panel]" (see DCDevTools.h). focus@Z sets the zoom: 0 = whole site .. 1 = closest.
# Auto = run-seconds the bot plays before the shots; Hour = clock time the bot stops at.
$Scenes = @(
    @{
        Name  = "menu"
        About = "Title screen with a full leaderboard, then each options tab"
        Args  = @("-DCUIFakeData", "-DCUIView=-,options.general,options.graphics,options.access,options.gameplay")
        Shots = @("title:wide:Title", "options_audio:wide:Title", "options_graphics:wide:Title",
                  "options_accessibility:wide:Title", "options_gameplay:wide:Title")
    },
    @{
        Name  = "early"
        About = "Day 2: the office, two crew, the first jobs"
        Args  = @("-DCSeed=$Seed", "-DCAutoPlay=600", "-DCHour=10")
        Shots = @("early_overview:wide", "early_office:Office@0.6", "early_jobs:wide:Missions", "early_recruits:wide:Recruits")
    },
    @{
        Name  = "mid"
        About = "Around day 5: a few rooms, a handful of crew"
        Args  = @("-DCSeed=$Seed", "-DCAutoPlay=6000", "-DCHour=15")
        Shots = @("mid_overview:wide", "mid_house:house@0.5", "mid_yard:yard@0.35", "mid_crew:house@0.3:Crew",
                  "mid_build:wide:Build", "mid_workbench:wide:Workbench")
    },
    @{
        Name  = "late"
        About = "Around day 14: the whole house, a full crew, the Big Score on the board"
        Args  = @("-DCSeed=$Seed", "-DCAutoPlay=18000", "-DCHour=17")
        Shots = @("late_overview:wide", "late_house:house@0.5", "late_house_close:house@0.7", "late_ops_room:OpsRoom@0.75",
                  "late_lounge:Lounge@0.75", "late_crew_closeup:crew@0.8", "late_jobs:wide:Missions", "late_planner:wide:MissionPlanner",
                  "late_crew:house@0.3:Crew", "late_reports:wide:Reports")
    },
    @{
        Name  = "night"
        About = "Late game after dark"
        Args  = @("-DCSeed=$Seed", "-DCAutoPlay=12000", "-DCHour=22")
        Shots = @("night_overview:wide@0.15", "night_house:house@0.55", "night_yard:yard@0.4")
    },
    @{
        Name  = "victory"
        About = "End screen: the Big Score pulled off"
        Args  = @("-DCSeed=$Seed", "-DCAutoPlay=18000", "-DCEndRun=win")
        Shots = @("end_victory:wide")
    },
    @{
        Name  = "gameover"
        About = "End screen: went broke"
        Args  = @("-DCSeed=$($Seed + 4)", "-DCAutoPlay=2500", "-DCEndRun=lose")
        Shots = @("end_gameover:wide")
    }
)

if ($List) {
    foreach ($s in $Scenes) {
        Write-Host ("{0,-9} {1}" -f $s.Name, $s.About)
        foreach ($shot in $s.Shots) { Write-Host ("            {0}.png" -f ($shot -split ":")[0]) }
    }
    exit 0
}

# -Only menu,late arrives as one string when run through "powershell -File".
if ($Only) {
    $Only = @($Only | ForEach-Object { $_ -split "," } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $Known = $Scenes | ForEach-Object { $_.Name }
    foreach ($o in $Only) { if ($Known -notcontains $o) { Fail "unknown scene '$o' (scenes: $($Known -join ', '))" } }
    $Scenes = @($Scenes | Where-Object { $Only -contains $_.Name })
}

# --- What to launch -----------------------------------------------------------
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UProject   = Join-Path $ProjectDir "DirectorsCut.uproject"

if ($Exe) {
    if (-not (Test-Path $Exe)) { Fail "game not found: $Exe" }
    $Launch     = (Resolve-Path $Exe).Path
    $LaunchArgs = @()
    Write-Host "==> Shooting the packaged build: $Launch"
    Write-Host "    (it must be a Development build: Scripts\package_win.ps1 -Dev)"
} else {
    $Editor  = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
    $BuildBat = Join-Path $UERoot "Engine\Build\BatchFiles\Build.bat"
    if (-not (Test-Path $Editor))   { Fail "UnrealEditor.exe not found at: $Editor (pass -UERoot or set UE_ROOT)" }
    if (-not (Test-Path $UProject)) { Fail "Project not found: $UProject" }
    if (-not $NoBuild) {
        if (Get-Process -Name "UnrealEditor*" -ErrorAction SilentlyContinue) {
            Write-Host "WARNING: an Unreal Editor is running. If it has DirectorsCut open the build can't replace its DLL;" -ForegroundColor Yellow
            Write-Host "         close it, or pass -NoBuild to shoot with the binaries that are already there." -ForegroundColor Yellow
        }
        Write-Host "==> Building DirectorsCutEditor (Win64 Development)"
        & $BuildBat DirectorsCutEditor Win64 Development "-project=$UProject" -waitmutex
        if ($LASTEXITCODE -ne 0) { Fail "Build failed (exit code $LASTEXITCODE)" }
    }
    $Launch     = $Editor
    # The last one keeps this game instance from starting the editor's MCP server (it would fight an open editor for the port).
    $LaunchArgs = @("`"$UProject`"", "-game",
        "-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocol.ModelContextProtocolSettings]:bAutoStartServer=False")
}

if (-not $OutDir) { $OutDir = Join-Path $ProjectDir ("Saved\Screenshots\" + (Get-Date -Format "yyyyMMdd-HHmmss")) }
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$LogDir = Join-Path $OutDir "logs"
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null

# Player settings that would spoil a shot are overridden for these launches only (nothing is written to disk):
# no FPS counter, no debug overlay, no high-contrast UI, no auto-pause when the window loses focus, no sound.
$Settings = "-ini:GameUserSettings:" + ((@(
    "ShowFPS=False", "DebugView=False", "HighContrast=False", "PauseOnFocusLoss=False", "MuteAll=True"
) | ForEach-Object { "[DirectorsCut.Settings]:$_" }) -join ",")

$Common = @(
    $(if ($Fullscreen) { "-fullscreen" } else { "-windowed" }),
    "-ResX=$Width", "-ResY=$Height", "-nosplash",
    "-DCSaveSlot=DCShot",
    "-DCShotDelay=$ShotDelay",
    "-DCShotDir=`"$OutDir`"",
    $Settings
)

# --- Shoot --------------------------------------------------------------------
$Start   = Get-Date
$Missing = @()
$Taken   = 0
$n = 0
foreach ($s in $Scenes) {
    $n++
    Write-Host ""
    Write-Host ("==> [{0}/{1}] {2}: {3}" -f $n, $Scenes.Count, $s.Name, $s.About)
    $Log  = Join-Path $LogDir ($s.Name + ".log")
    # No spaces in the shot list, so it needs no quotes (quotes around it would end up inside the last shot's name).
    $Cmd  = $LaunchArgs + $Common + $s.Args + @("-DCShots=$($s.Shots -join ',')", "-abslog=`"$Log`"")

    $p = Start-Process -FilePath $Launch -ArgumentList ($Cmd -join " ") -PassThru
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        Write-Host "    still running after $TimeoutSec s: stopping it (see $Log)" -ForegroundColor Yellow
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }

    foreach ($shot in $s.Shots) {
        $File = Join-Path $OutDir (($shot -split ":")[0] + ".png")
        # The PNG is written a moment after the game is told to quit.
        for ($i = 0; $i -lt 10 -and -not (Test-Path $File); $i++) { Start-Sleep -Milliseconds 300 }
        if (Test-Path $File) { $Taken++; Write-Host "    $(Split-Path $File -Leaf)" }
        else { $Missing += $File; Write-Host "    MISSING $(Split-Path $File -Leaf)" -ForegroundColor Red }
    }
    if (Test-Path $Log) {
        Select-String -Path $Log -Pattern '\[AutoPlay\] t=', '\[DCShots\] unknown focus' |
            ForEach-Object { Write-Host ("    " + ($_.Line -replace '^.*?(\[(AutoPlay|DCShots)\])', '$1')) -ForegroundColor DarkGray }
    }
}

$Elapsed = (Get-Date) - $Start
Write-Host ""
Write-Host ("Done in {0}m {1}s: {2} screenshots in {3}" -f [math]::Floor($Elapsed.TotalMinutes), $Elapsed.Seconds, $Taken, $OutDir)
if ($Missing.Count -gt 0) {
    Write-Host ("{0} shot(s) missing; logs are in {1}" -f $Missing.Count, $LogDir) -ForegroundColor Red
    exit 1
}
