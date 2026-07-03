$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$MsysRoot = "C:\msys64"
$Bash = Join-Path $MsysRoot "usr\bin\bash.exe"

if (-not (Test-Path $Bash)) {
    throw "MSYS2 was not found at $MsysRoot. Install it with: winget install MSYS2.MSYS2"
}

$packages = @(
    "mingw-w64-ucrt-x86_64-toolchain",
    "mingw-w64-ucrt-x86_64-cmake",
    "mingw-w64-ucrt-x86_64-ninja",
    "mingw-w64-ucrt-x86_64-eigen3",
    "mingw-w64-ucrt-x86_64-exprtk",
    "mingw-w64-ucrt-x86_64-nlohmann-json",
    "mingw-w64-ucrt-x86_64-qt6-base",
    "mingw-w64-ucrt-x86_64-vtk",
    "mingw-w64-ucrt-x86_64-opencascade"
)

& $Bash -lc "pacman -S --needed --noconfirm $($packages -join ' ')"
if ($LASTEXITCODE -ne 0) {
    throw "MSYS2 package installation failed."
}

$Source = $Root -replace "\\", "/"
$BuildCommand = "export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:`$PATH; cd '$Source' && /ucrt64/bin/cmake --preset windows-msys2-ucrt64 && /ucrt64/bin/cmake --build --preset windows-msys2-ucrt64-release"
& $Bash -lc $BuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed."
}

$TargetExe = Join-Path $Root "build\windows-msys2-ucrt64\VisualizationPrototype.exe"
$WinDeployQt = Join-Path $MsysRoot "ucrt64\bin\windeployqt6.exe"
if (-not (Test-Path $WinDeployQt)) {
    throw "windeployqt6.exe was not found. Install mingw-w64-ucrt-x86_64-qt6-base first."
}

& $WinDeployQt --release --compiler-runtime $TargetExe
if ($LASTEXITCODE -ne 0) {
    throw "Qt runtime deployment failed."
}

& (Join-Path $PSScriptRoot "deploy_msys2_runtime.ps1")
