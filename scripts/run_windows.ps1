$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Exe = Join-Path $Root "build\windows-msys2-ucrt64\VisualizationPrototype.exe"

if (-not (Test-Path $Exe)) {
    & (Join-Path $PSScriptRoot "build_windows.ps1")
}

$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path
& $Exe
