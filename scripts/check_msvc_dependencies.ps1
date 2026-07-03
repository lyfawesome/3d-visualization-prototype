param(
    [string[]]$QtRoots = @(
        "C:\Qt",
        "D:\Qt",
        "$env:USERPROFILE\Qt",
        "$env:LOCALAPPDATA\Programs\Qt"
    ),
    [string[]]$PackageSearchRoots = @(
        "C:\Qt",
        "D:\Qt",
        "C:\VTK",
        "D:\VTK",
        "C:\OpenCASCADE",
        "D:\OpenCASCADE",
        "C:\OCCT",
        "D:\OCCT",
        "D:\pick_point\build"
    ),
    [string]$VcpkgRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) "extern\vcpkg")
)

$ErrorActionPreference = "Continue"

function Show-Command {
    param([string]$Name)

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        "${Name}: $($cmd.Source)"
    } else {
        "${Name}: not found on PATH"
    }
}

"== MSVC / Visual Studio =="
$VsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $VsWhere) {
    & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
} else {
    "vswhere.exe: not found"
}

""
"== Build Tools =="
Show-Command "cmake.exe"
Show-Command "ninja.exe"
Show-Command "conan.exe"
Show-Command "vcpkg.exe"
if (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe")) {
    "project vcpkg: $(Join-Path $VcpkgRoot "vcpkg.exe")"
}

""
"== Official Qt Candidates =="
foreach ($root in $QtRoots) {
    if (-not (Test-Path $root)) {
        continue
    }

    Get-ChildItem -LiteralPath $root -Recurse -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "msvc2022_64" -or $_.Name -eq "msvc2019_64" } |
        Select-Object -ExpandProperty FullName
}

""
"== CMake Package Config Candidates =="
$configNames = @(
    "Qt6Config.cmake",
    "VTKConfig.cmake",
    "OpenCASCADEConfig.cmake",
    "opencascade-config.cmake"
)
foreach ($root in $PackageSearchRoots) {
    if (-not (Test-Path $root)) {
        continue
    }

    foreach ($name in $configNames) {
        Get-ChildItem -LiteralPath $root -Recurse -Filter $name -File -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
    }
}

""
"== vcpkg Installed Packages =="
$InfoDir = Join-Path (Split-Path -Parent $PSScriptRoot) "build\windows-msvc-vcpkg\vcpkg_installed\vcpkg\info"
if (Test-Path $InfoDir) {
    Get-ChildItem -LiteralPath $InfoDir -Filter "*.list" |
        Sort-Object Name |
        Select-Object -ExpandProperty Name
} else {
    "No vcpkg install info found at $InfoDir"
}
