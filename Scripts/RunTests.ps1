# Runs the automation unit tests and the full in-game match self-test headlessly.
param([string]$EngineDir)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ChaosEngine.ps1')

$engine = Find-UnrealEngine $EngineDir
$editorCmd = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDir = Join-Path $ProjectRoot 'Saved\TestLogs'
New-Item -ItemType Directory -Force $logDir | Out-Null

Write-Host '== Unit tests (ChaosControl.Rules) =='
$unitLog = Join-Path $logDir 'UnitTests.log'
& $editorCmd $ProjectFile '-ExecCmds=Automation RunTests ChaosControl; Quit' -unattended -nullrhi -nosplash -nosound "-abslog=$unitLog" | Out-Null
$unitOk = Select-String -Path $unitLog -Pattern 'Test Completed. Result=\{Success\}' -SimpleMatch:$false
$unitFail = Select-String -Path $unitLog -Pattern 'Test Completed. Result=\{Fail' -SimpleMatch:$false
Write-Host ("Unit tests passed: {0}, failed: {1}  (log: {2})" -f @($unitOk).Count, @($unitFail).Count, $unitLog)

Write-Host '== Gameplay self-test (-ChaosAutoTest) =='
$gameLog = Join-Path $logDir 'AutoTest.log'
& $editorCmd $ProjectFile -game -ChaosAutoTest -nullrhi -unattended -nosplash -nosound "-abslog=$gameLog" | Out-Null
Select-String -Path $gameLog -Pattern '\[FAIL\]|CHAOS AUTOTEST RESULT' | ForEach-Object { $_.Line }

$passed = (Select-String -Path $gameLog -Pattern 'CHAOS AUTOTEST RESULT: PASSED' -Quiet) -and @($unitFail).Count -eq 0 -and @($unitOk).Count -gt 0
if ($passed) { Write-Host 'ALL TESTS PASSED' } else { Write-Host "TESTS FAILED - see $logDir"; exit 1 }
