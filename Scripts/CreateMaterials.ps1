# Regenerates Content/Materials/*.uasset from Scripts/create_materials.py (headless editor).
param([string]$EngineDir)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ChaosEngine.ps1')

$engine = Find-UnrealEngine $EngineDir
$script = Join-Path $PSScriptRoot 'create_materials.py'
$log = Join-Path $ProjectRoot 'Saved\TestLogs\CreateMaterials.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null

& (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $ProjectFile -run=pythonscript "-script=$script" -unattended -nosplash -nosound "-abslog=$log" | Out-Null
Select-String -Path $log -Pattern 'CHAOS MATERIALS|Error:|Traceback|RuntimeError' | ForEach-Object { $_.Line }
if (-not (Select-String -Path $log -Pattern 'CHAOS MATERIALS: complete' -Quiet)) { throw "Material generation failed - see $log" }
