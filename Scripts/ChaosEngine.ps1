# Shared helper: locates the Unreal Engine install and the project file.
param()

$script:ProjectRoot = Split-Path -Parent $PSScriptRoot
$script:ProjectFile = Join-Path $ProjectRoot 'ChaosControl.uproject'

function Find-UnrealEngine {
    param([string]$EngineDir)

    $candidates = @()
    if ($EngineDir) { $candidates += $EngineDir }
    if ($env:UE_ROOT) { $candidates += $env:UE_ROOT }

    $association = (Get-Content $script:ProjectFile -Raw | ConvertFrom-Json).EngineAssociation
    $reg = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association" -ErrorAction SilentlyContinue
    if ($reg -and $reg.InstalledDirectory) { $candidates += $reg.InstalledDirectory }

    Get-ChildItem 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' -ErrorAction SilentlyContinue |
        Sort-Object PSChildName -Descending |
        ForEach-Object { $p = (Get-ItemProperty $_.PSPath).InstalledDirectory; if ($p) { $candidates += $p } }

    foreach ($drive in 'C:', 'D:', 'E:', 'F:') {
        $candidates += Get-ChildItem "$drive\Program Files\Epic Games\UE_5*" -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | ForEach-Object FullName
    }

    foreach ($dir in $candidates) {
        if ($dir -and (Test-Path (Join-Path $dir 'Engine\Build\BatchFiles\Build.bat'))) { return $dir }
    }
    throw "Unreal Engine 5 not found. Install it from the Epic Games Launcher or pass -EngineDir 'C:\Program Files\Epic Games\UE_5.4'."
}
