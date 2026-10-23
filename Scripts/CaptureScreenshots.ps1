# Launches the game with -ChaosScreenshots and prints the folder the PNGs were written to.
param([string]$EngineDir, [int]$ResX = 1600, [int]$ResY = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ChaosEngine.ps1')

$engine = Find-UnrealEngine $EngineDir
$log = Join-Path $ProjectRoot 'Saved\TestLogs\Screenshots.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null

$game = Start-Process -FilePath (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe') -PassThru -ArgumentList @(
    "`"$ProjectFile`"", '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-ChaosScreenshots', '-nosound', '-unattended', "-abslog=`"$log`"")
if (-not $game.WaitForExit(540000)) { $game.Kill(); throw 'Screenshot run timed out' }

Select-String -Path $log -Pattern 'Failed to compile|missing usage flag' | ForEach-Object { Write-Warning $_.Line }
$shots = Select-String -Path $log -Pattern 'CHAOS SCREENSHOT: (.*)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
if (-not $shots) { throw "No screenshots taken - see $log" }
Write-Host "Screenshots: $(Split-Path $shots[0])"
