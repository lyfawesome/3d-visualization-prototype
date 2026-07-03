param(
    [string]$VcpkgRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) "extern\vcpkg"),
    [switch]$BootstrapVcpkg,
    [switch]$ForceSourceBuild,
    [switch]$ReleaseOnly,
    [switch]$LeanVtkQt,
    [int]$MaxConcurrency = 0
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$VsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $VsWhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools 2022 with C++ tools first."
}

$VsInstall = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VsInstall) {
    throw "MSVC C++ tools were not found. Install workload: Desktop development with C++."
}

$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
if (-not (Test-Path $VcpkgExe)) {
    if (-not $BootstrapVcpkg) {
        throw "vcpkg.exe was not found at $VcpkgRoot. Re-run with -BootstrapVcpkg to clone and bootstrap vcpkg, or pass -VcpkgRoot."
    }

    $Parent = Split-Path -Parent $VcpkgRoot
    New-Item -ItemType Directory -Force -Path $Parent | Out-Null
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg bootstrap failed."
    }
}

$CMake = "C:\Program Files\CMake\bin\cmake.exe"
if (-not (Test-Path $CMake)) {
    $CMake = Join-Path $VsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
if (-not (Test-Path $CMake)) {
    throw "CMake was not found."
}

$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $Toolchain)) {
    throw "vcpkg CMake toolchain was not found at $Toolchain."
}

$Triplet = "x64-windows"
$HostTriplet = "x64-windows"
$BuildDir = Join-Path $Root "build\windows-msvc-vcpkg"
if ($LeanVtkQt) {
    $ReleaseOnly = $true
}
if ($ReleaseOnly) {
    $Triplet = "x64-windows-release"
    $HostTriplet = "x64-windows-release"
    $BuildDir = Join-Path $Root "build\windows-msvc-vcpkg-release-only"
}
if ($LeanVtkQt) {
    $BuildDir = Join-Path $Root "build\windows-msvc-vcpkg-lean-release"
}

$OverlayPorts = ""
if ($LeanVtkQt) {
    $OverlayPorts = Join-Path $Root "vcpkg-overlays\ports"
    if (-not (Test-Path $OverlayPorts)) {
        throw "Lean VTK overlay ports were not found at $OverlayPorts."
    }
}

if ($ForceSourceBuild) {
    $env:VCPKG_BINARY_SOURCES = "clear"
} else {
    $env:VCPKG_BINARY_SOURCES = "clear;files,$env:LOCALAPPDATA\vcpkg\archives,readwrite"
}

if ($MaxConcurrency -gt 0) {
    $env:VCPKG_MAX_CONCURRENCY = "$MaxConcurrency"
    $env:CMAKE_BUILD_PARALLEL_LEVEL = "$MaxConcurrency"
}

& $CMake -S $Root -B $BuildDir `
    -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
    -DVCPKG_TARGET_TRIPLET="$Triplet" `
    -DVCPKG_HOST_TRIPLET="$HostTriplet" `
    -DVCPKG_OVERLAY_PORTS="$OverlayPorts" `
    -DVCPKG_MANIFEST_MODE=ON `
    -DVCPKG_APPLOCAL_DEPS=ON

if ($LASTEXITCODE -ne 0) {
    throw "MSVC vcpkg CMake configure failed."
}
