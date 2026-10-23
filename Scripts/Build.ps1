# Compiles the ChaosControl editor target (Win64 Development).
param([string]$EngineDir)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ChaosEngine.ps1')

$engine = Find-UnrealEngine $EngineDir
Write-Host "Engine:  $engine"
Write-Host "Project: $ProjectFile"

& (Join-Path $engine 'Engine\Build\BatchFiles\Build.bat') ChaosControlEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
Write-Host 'BUILD SUCCEEDED'
