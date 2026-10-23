# Launches the game in a window (no editor UI). Build first with Build.ps1.
param([string]$EngineDir, [int]$ResX = 1600, [int]$ResY = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ChaosEngine.ps1')

$engine = Find-UnrealEngine $EngineDir
& (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe') $ProjectFile -game -windowed "-ResX=$ResX" "-ResY=$ResY" -log
