# Records the jam-submission video footage of Director's Cut on Windows.
#
#   powershell -ExecutionPolicy Bypass -File Scripts\record_video_win.ps1                  # the whole reel: 60 s of clips
#   powershell -ExecutionPolicy Bypass -File Scripts\record_video_win.ps1 -List            # show the clips in the reel
#   powershell -ExecutionPolicy Bypass -File Scripts\record_video_win.ps1 -Reel my.txt     # another shot list
#   powershell -ExecutionPolicy Bypass -File Scripts\record_video_win.ps1 -NoRecord        # play it without filming
#
# The game is launched once and plays the shot list in Scripts\video_reel.txt by itself: ten seconds on the title
# screen, then it jumps through one seeded run (day 1, day 2, day 5, a night, day 14, the Big Score), holding each
# clip for a few seconds, some with a slow camera move, some with the UI open and buttons being "pressed". Every clip
# fades in from black and out again; the run jumps forward behind the black. Edit video_reel.txt to change the shots.
#
# ffmpeg films the game while that happens (winget install Gyan.FFmpeg, then open a new terminal). It grabs the screen
# on the GPU (Desktop Duplication) and encodes with NVENC when there is an NVIDIA card, otherwise x264. Leave the
# mouse and keyboard alone and keep the game in front and uncovered until it quits. Turn off notifications (Focus
# assist / Do not disturb) so none pop up over it.
#
# The game runs in a Width x Height window when that fits on the main monitor with its title bar; otherwise it fills
# the main monitor (borderless) and is filmed at the monitor's size. Either way the clips are scaled to Width x Height.
#
# Output: Saved\Videos\<timestamp>\ (a new folder every time)
#   reel_full.mp4       the whole recording (fades included), at the size it was filmed
#   Clips\NN_name.mp4   each clip's clean part, cut from it at Width x Height, ready to edit
#   reel_cues.csv       where each clip starts and ends in reel_full.mp4 (seconds)
#   logs\               game and ffmpeg logs
#
# Your own saves and leaderboard are not touched: the run saves under the "DCShot" prefix, and the title screen
# shows a sample leaderboard and sample save slots.
#
# By default the game runs from source through the editor (UnrealEditor.exe -game), which is built first. To film a
# packaged build pass -Exe; it must be a Development build (Scripts\package_win.ps1 -Dev), because a Shipping build
# ignores the dev flags the reel needs.
# Engine: pass -UERoot or set UE_ROOT to override the default install location.

param(
    [string]$Reel,                   # shot list; default Scripts\video_reel.txt
    [switch]$List,                   # print the clips in the reel, then exit
    [int]$Width = 1920,              # video size (and the game window's, when it fits)
    [int]$Height = 1080,
    [switch]$Fullscreen,             # fill the main monitor even if the window would fit
    [int]$Fps = 60,                  # recording frame rate (the game is capped to it too)
    [ValidateSet("auto", "nvenc", "x264")]
    [string]$Encoder = "auto",       # live encoder; auto = NVENC if it works here, else x264
    [int]$Quality = 16,              # constant quality (lower = better, bigger); the clips use this minus 2
    [int]$Seed = 0,                  # run seed (overrides the reel's "newrun" lines); 0 = the reel's
    [switch]$NoRecord,               # don't film; just play the reel
    [switch]$NoSplit,                # don't cut the clips out afterwards
    [switch]$NoBuild,                # skip building the editor target
    [string]$Exe,                    # packaged Development build to film instead (e.g. Packaged\Windows\DirectorsCut.exe)
    [string]$OutDir,                 # default: Saved\Videos\<timestamp>
    [int]$TimeoutSec = 1200,         # the game is stopped if it runs longer than this
    [string]$UERoot = $(if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.9" })
)

$ErrorActionPreference = "Stop"
$Inv = [System.Globalization.CultureInfo]::InvariantCulture

function Fail([string]$msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Num([double]$v) { $v.ToString("0.000", $Inv) }
# ffmpeg talks on stderr, which Windows PowerShell turns into errors (fatal under "Stop"): run it with errors relaxed
# and hand back its output as text. $LASTEXITCODE is ffmpeg's.
function FF([string[]]$A) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $script:FFmpeg $A 2>&1 | ForEach-Object { "$_" } } finally { $ErrorActionPreference = $old }
}

$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Reel) { $Reel = Join-Path $PSScriptRoot "video_reel.txt" }
if (-not (Test-Path $Reel)) { Fail "reel not found: $Reel" }
$Reel = (Resolve-Path $Reel).Path

# Clips in the reel: name, camera, the rest.
$Clips = @(Get-Content $Reel | ForEach-Object { ($_ -replace '#.*$', '').Trim() } | Where-Object { $_ -match '^clip\s' } | ForEach-Object {
    $w = $_ -split '\s+'
    [pscustomobject]@{ Name = $w[1]; Camera = $w[2]; Rest = ($w | Select-Object -Skip 3) -join " " }
})
if ($List) {
    $i = 0
    foreach ($c in $Clips) { $i++; Write-Host ("{0:00}  {1,-20} {2,-28} {3}" -f $i, $c.Name, $c.Camera, $c.Rest) }
    exit 0
}
if ($Clips.Count -eq 0) { Fail "no clips in $Reel" }

$FFmpeg = $null
if (-not $NoRecord) {
    $FFmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
    if (-not $FFmpeg) { Fail "ffmpeg not found: winget install Gyan.FFmpeg, then open a new terminal (or pass -NoRecord and film it yourself)" }
    if (-not ((FF @("-hide_banner", "-filters")) -match '\bddagrab\b')) {
        Fail "this ffmpeg has no ddagrab (needs ffmpeg 6.1+ built with D3D11): winget upgrade Gyan.FFmpeg"
    }
}

# --- Main monitor, in real pixels ---------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class DCWin {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
}
"@
[void][DCWin]::SetProcessDPIAware()
$MonW = [DCWin]::GetSystemMetrics(0)   # SM_CXSCREEN: the primary monitor
$MonH = [DCWin]::GetSystemMetrics(1)

$GameW = $Width; $GameH = $Height
$Borderless = [bool]$Fullscreen
if (-not $Borderless -and ($Width -gt $MonW - 16 -or $Height -gt $MonH - 90)) {
    $Borderless = $true
    Write-Host "==> The main monitor (${MonW}x${MonH}) is too small for a ${Width}x${Height} window: filling it instead"
}
if ($Borderless) { $GameW = $MonW; $GameH = $MonH }

# --- What to launch -----------------------------------------------------------
$UProject = Join-Path $ProjectDir "DirectorsCut.uproject"
if ($Exe) {
    if (-not (Test-Path $Exe)) { Fail "game not found: $Exe" }
    $Launch     = (Resolve-Path $Exe).Path
    $LaunchArgs = @()
    Write-Host "==> Filming the packaged build: $Launch"
    Write-Host "    (it must be a Development build: Scripts\package_win.ps1 -Dev)"
} else {
    $Editor   = Join-Path $UERoot "Engine\Binaries\Win64\UnrealEditor.exe"
    $BuildBat = Join-Path $UERoot "Engine\Build\BatchFiles\Build.bat"
    if (-not (Test-Path $Editor))   { Fail "UnrealEditor.exe not found at: $Editor (pass -UERoot or set UE_ROOT)" }
    if (-not (Test-Path $UProject)) { Fail "Project not found: $UProject" }
    if (-not $NoBuild) {
        if (Get-Process -Name "UnrealEditor*" -ErrorAction SilentlyContinue) {
            Write-Host "WARNING: an Unreal Editor is running. If it has DirectorsCut open the build can't replace its DLL;" -ForegroundColor Yellow
            Write-Host "         close it, or pass -NoBuild to use the binaries that are already there." -ForegroundColor Yellow
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

if (-not $OutDir) { $OutDir = Join-Path $ProjectDir ("Saved\Videos\" + (Get-Date -Format "yyyyMMdd-HHmmss")) }
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$LogDir = Join-Path $OutDir "logs"
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
$Full    = Join-Path $OutDir "reel_full.mp4"
$GameLog = Join-Path $LogDir "game.log"
$CuesCsv = Join-Path $OutDir "reel_cues.csv"

# Player settings that would spoil the video are overridden for this launch only (nothing is written to disk):
# no FPS counter, no debug overlay, no high-contrast UI, no auto-pause when the window loses focus, no edge
# scrolling, no sound; and the window mode.
$Ini = @("ShowFPS=False", "DebugView=False", "HighContrast=False", "PauseOnFocusLoss=False", "EdgeScroll=False", "MuteAll=True") |
    ForEach-Object { "[DirectorsCut.Settings]:$_" }
$Mode = if ($Borderless) { 1 } else { 2 }   # EWindowMode: 1 = borderless fullscreen, 2 = windowed
$Ini += @("FullscreenMode=$Mode", "LastConfirmedFullscreenMode=$Mode", "PreferredFullscreenMode=$Mode",
          "ResolutionSizeX=$GameW", "ResolutionSizeY=$GameH", "bUseVSync=False", "FrameRateLimit=$Fps") |
    ForEach-Object { "[/Script/Engine.GameUserSettings]:$_" }
$Settings = "-ini:GameUserSettings:" + ($Ini -join ",")

$GameArgs = @(
    $(if ($Borderless) { "-WinX=0 -WinY=0" } else { "-windowed" }),
    "-ResX=$GameW", "-ResY=$GameH", "-nosplash", "-DCSaveSlot=DCShot", "-DCUIFakeData",
    "-DCShotDir=`"$OutDir`"", "-DCReel=`"$Reel`"", "-DCReelWait=$(if ($NoRecord) { 0 } else { 60 })",
    "-ExecCmds=`"t.MaxFPS $Fps`"", $Settings, "-abslog=`"$GameLog`""
)
if ($Seed -ne 0) { $GameArgs += "-DCReelSeed=$Seed" }

# --- Encoder ------------------------------------------------------------------
function Test-Encoder([string]$Name) {
    FF @("-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "color=black:s=640x360:d=0.2", "-c:v", $Name, "-f", "null", "-") | Out-Null
    return $LASTEXITCODE -eq 0
}
$Enc = $Encoder
if (-not $NoRecord -and $Enc -eq "auto") { $Enc = if (Test-Encoder "h264_nvenc") { "nvenc" } else { "x264" } }

function Read-Shared([string]$Path) {
    # The game keeps its log open; read it without fighting it for the file.
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try { (New-Object System.IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
    } catch { "" }
}

# --- Play ---------------------------------------------------------------------
Write-Host "==> Playing $($Clips.Count) clips from $(Split-Path $Reel -Leaf) (leave the mouse alone until the game quits)"
$Start = Get-Date
$Game = Start-Process -FilePath $Launch -ArgumentList (($LaunchArgs + $GameArgs) -join " ") -PassThru
$Rec = $null

try {
    if (-not $NoRecord) {
        # The game writes its window rect (client area, desktop pixels) once the loading screen has lifted.
        Write-Host "    waiting for the game window..."
        $WinFile = Join-Path $OutDir "reel_window.txt"
        $T0 = Get-Date
        while (-not (Test-Path $WinFile) -or (Get-Item $WinFile).Length -eq 0) {
            if ($Game.HasExited) { Fail "the game quit before the reel started (see $GameLog)" }
            if (((Get-Date) - $T0).TotalSeconds -gt 600) { Fail "the game never got past loading (see $GameLog)" }
            Start-Sleep -Milliseconds 500
        }
        $r = (Get-Content $WinFile -Raw).Trim() -split '\s+'
        $X = [int]$r[0]; $Y = [int]$r[1]; $W = [int]$r[2]; $H = [int]$r[3]
        # Even sizes for the encoder, and inside the main monitor (Desktop Duplication films one monitor).
        $W -= $W % 2; $H -= $H % 2
        if ($X -lt 0 -or $Y -lt 0 -or $X + $W -gt $MonW -or $Y + $H -gt $MonH) {
            Fail "the game window (${W}x${H} at $X,$Y) isn't on the main monitor (${MonW}x${MonH}); make the monitor you want to film the main one"
        }
        Write-Host "    filming ${W}x${H} at $X,$Y of the main monitor ($Enc)"

        $Grab = "ddagrab=output_idx=0:framerate=${Fps}:draw_mouse=0:video_size=${W}x${H}:offset_x=${X}:offset_y=${Y}"
        $EncArgs = if ($Enc -eq "nvenc") {
            # NVENC takes the captured GPU frames as they are: nothing leaves the GPU.
            "-c:v h264_nvenc -preset p5 -tune hq -rc vbr -cq $Quality -b:v 0 -g $Fps"
        } else {
            "-vf hwdownload,format=bgra,format=yuv420p -c:v libx264 -preset veryfast -crf $Quality -g $Fps"
        }
        $FfLog = Join-Path $LogDir "ffmpeg.log"
        $Progress = Join-Path $LogDir "ffmpeg_progress.txt"
        # Through cmd so the log goes to a file; ffmpeg stops cleanly when it reads "q" on its input.
        $FfCmd = "`"$FFmpeg`" -hide_banner -y -f lavfi -i `"$Grab`" $EncArgs -movflags +faststart -progress `"$Progress`" `"$Full`" 2> `"$FfLog`""
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "cmd.exe"
        $psi.Arguments = "/c `"$FfCmd`""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardInput = $true
        $psi.CreateNoWindow = $true
        $Rec = [System.Diagnostics.Process]::Start($psi)

        # Wait until frames are being written, then let the game start.
        $T0 = Get-Date
        while (-not ((Test-Path $Progress) -and ((Read-Shared $Progress) -match 'frame=[1-9]'))) {
            if ($Rec.HasExited) { Fail "ffmpeg stopped (see $FfLog)" }
            if (((Get-Date) - $T0).TotalSeconds -gt 15) { Fail "ffmpeg isn't recording (see $FfLog)" }
            Start-Sleep -Milliseconds 250
        }
        Start-Sleep -Seconds 1
        New-Item -ItemType File -Path (Join-Path $OutDir "reel_go") -Force | Out-Null
        Write-Host "==> Recording"
    }

    # Progress: the clip the game is on.
    $Last = ""
    $T0 = Get-Date
    while (-not $Game.HasExited) {
        if (((Get-Date) - $T0).TotalSeconds -gt $TimeoutSec) {
            Write-Host "    still running after $TimeoutSec s: stopping it" -ForegroundColor Yellow
            Stop-Process -Id $Game.Id -Force -ErrorAction SilentlyContinue
            break
        }
        $m = [regex]::Matches((Read-Shared $GameLog), '\[Reel\] clip (\S+)')
        if ($m.Count -gt 0 -and $m[$m.Count - 1].Groups[1].Value -ne $Last) {
            $Last = $m[$m.Count - 1].Groups[1].Value
            Write-Host ("    [{0}/{1}] {2}" -f $m.Count, $Clips.Count, $Last)
        }
        Start-Sleep -Milliseconds 500
    }
} finally {
    if ($Rec -and -not $Rec.HasExited) {
        Start-Sleep -Milliseconds 500
        try { $Rec.StandardInput.Write("q"); $Rec.StandardInput.Flush() } catch {}
        if (-not $Rec.WaitForExit(20000)) {
            # cmd won't pass the kill on: stop its ffmpeg too.
            Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Rec.Id)" -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
            Stop-Process -Id $Rec.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (-not $Game.HasExited) { Stop-Process -Id $Game.Id -Force -ErrorAction SilentlyContinue }
}

[regex]::Matches((Read-Shared $GameLog), '\[Reel\][^\r\n]*(did nothing|unknown|can.t|no reel_go)[^\r\n]*') |
    ForEach-Object { Write-Host ("    " + $_.Value) -ForegroundColor Yellow }

if ($NoRecord) { Write-Host "Done (not recorded). Clip times: $CuesCsv"; exit 0 }
if (-not (Test-Path $Full) -or (Get-Item $Full).Length -eq 0) { Fail "no recording was written (see $LogDir\ffmpeg.log)" }
if (-not (Test-Path $CuesCsv)) { Fail "the game wrote no clip times (see $GameLog)" }

# --- Line the cues up with the video ------------------------------------------
# The game's clock starts at the go signal; the recording started a little before. The reel opens on black and the
# first clip fades in from it, so the end of that first black stretch in the video is the "sync" cue.
$Cues = @(Import-Csv $CuesCsv)
$Sync = [double]::Parse(($Cues | Where-Object { $_.kind -eq "sync" } | Select-Object -First 1).start, $Inv)
$Detect = (FF @("-hide_banner", "-nostdin", "-t", "90", "-i", $Full, "-vf", "blackdetect=d=0.3:pix_th=0.06", "-an", "-f", "null", "-")) -join "`n"
$Offset = 1.0
$bm = [regex]::Matches($Detect, 'black_start:([0-9.]+)\s+black_end:([0-9.]+)')
$first = $bm | Where-Object { [double]::Parse($_.Groups[1].Value, $Inv) -lt 3 } | Select-Object -First 1
if ($first) { $Offset = [double]::Parse($first.Groups[2].Value, $Inv) - $Sync }
else { Write-Host "    couldn't find the opening fade; assuming the game started 1 s into the recording" -ForegroundColor Yellow }

# reel_cues.csv: times in the video.
$Cues | ForEach-Object {
    [pscustomobject]@{ kind = $_.kind; name = $_.name
        start = Num ([double]::Parse($_.start, $Inv) + $Offset); end = Num ([double]::Parse($_.end, $Inv) + $Offset) }
} | Export-Csv -NoTypeInformation -Path $CuesCsv
$Cues = @(Import-Csv $CuesCsv)

# --- Cut the clips --------------------------------------------------------------
$Cut = 0
if (-not $NoSplit) {
    Write-Host "==> Cutting the clips"
    $ClipDir = Join-Path $OutDir "Clips"
    New-Item -ItemType Directory -Path $ClipDir -Force | Out-Null
    $Scale = "scale=${Width}:${Height}:flags=lanczos:force_original_aspect_ratio=decrease,pad=${Width}:${Height}:-1:-1:black,format=yuv420p"
    $n = 0
    foreach ($c in ($Cues | Where-Object { $_.kind -eq "clip" })) {
        $n++
        # The whole clean part: the reel cuts hard (fade 0), so there is no fade to trim off.
        $s = [double]::Parse($c.start, $Inv)
        $len = [double]::Parse($c.end, $Inv) - [double]::Parse($c.start, $Inv)
        $Out = Join-Path $ClipDir ("{0:00}_{1}.mp4" -f $n, $c.name)
        $Err = FF @("-hide_banner", "-nostdin", "-loglevel", "error", "-y", "-ss", (Num $s), "-i", $Full, "-t", (Num $len), "-vf", $Scale,
            "-c:v", "libx264", "-preset", "slow", "-crf", [string][Math]::Max(0, $Quality - 2), "-an", "-movflags", "+faststart", $Out)
        if ($LASTEXITCODE -eq 0) { $Cut++; Write-Host "    $(Split-Path $Out -Leaf)" }
        else { Write-Host "    FAILED $(Split-Path $Out -Leaf): $($Err -join ' ')" -ForegroundColor Red }
    }
}

$Elapsed = (Get-Date) - $Start
Write-Host ""
Write-Host ("Done in {0}m {1}s: {2}" -f [math]::Floor($Elapsed.TotalMinutes), $Elapsed.Seconds, $Full)
if (-not $NoSplit) { Write-Host "$Cut clips in $(Join-Path $OutDir 'Clips')" }
Write-Host "Clip times: $CuesCsv"
