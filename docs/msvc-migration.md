# MSVC Migration Notes

This note records the current Windows/MSVC dependency state for the Qt + VTK + OCCT prototype. It is intentionally separate from the existing MSYS2 build path.

## Local Toolchain Findings

Run the local checker:

```powershell
.\scripts\check_msvc_dependencies.ps1
```

- Repository root: `D:\pick_point`
- Existing working build path: `windows-msys2-ucrt64`, using `C:\msys64\ucrt64` packages and GCC/MinGW ABI.
- Visual Studio Build Tools 2022 is installed at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`.
- MSVC compiler found after `vcvars64.bat`: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`, compiler version `19.44.35228`.
- MSBuild found at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe`, version `17.14.40.60911`.
- Visual Studio CMake and Ninja are installed under `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake`.
- Standalone CMake is installed at `C:\Program Files\CMake\bin\cmake.exe`, version `4.3.3`.
- No official Qt install was found under common roots such as `C:\Qt`, `D:\Qt`, `%USERPROFILE%\Qt`, or `%LOCALAPPDATA%\Programs\Qt`.
- Project-local vcpkg is available at `D:\pick_point\extern\vcpkg`.
- `conan` is not currently on `PATH`.
- No global `vcpkg` is currently on `PATH`.

## Dependency State

MSYS2 UCRT64 packages cannot be linked into an MSVC build. The MSVC migration needs a separate ABI-compatible dependency set.

Current local prebuilt/previously-built MSVC findings:

- Found Qt CMake package: `D:\pick_point\build\windows-msvc-vcpkg\vcpkg_installed\x64-windows\share\Qt6\Qt6Config.cmake`.
- Found OCCT CMake package: `D:\pick_point\build\windows-msvc-vcpkg\vcpkg_installed\x64-windows\share\opencascade\OpenCASCADEConfig.cmake`.
- Found previous OCCT CMake package: `D:\pick_point\build\windows-vcpkg\vcpkg_installed\x64-windows\share\opencascade\OpenCASCADEConfig.cmake`.
- No local `VTKConfig.cmake` was found under the normal project, Qt, VTK, OCCT, or build roots.
- No official Qt `msvc2022_64` installation was found locally.

The all-vcpkg path is vcpkg `x64-windows`:

- `qtbase` provides Qt 6 Widgets for MSVC.
- `vtk` must include the `qt` feature because the project requests VTK `GUISupportQt`.
- `opencascade` provides OCCT CMake config and imported MSVC `.lib`/`.dll` targets.

The official-Qt path is also viable, but VTK must be built or obtained against that same official Qt. A plain `vcpkg install vtk[qt]` pulls vcpkg's own `qtbase`; it does not reuse Qt Online Installer's `msvc2022_64` package.

Public package/source check:

- Qt official Windows documentation lists Windows 10/11 x86_64 with MSVC 2022 as a supported configuration.
- VTK official downloads list source tarballs and Python wheels/SDK packages. Treat this as a candidate to investigate, but not yet as a confirmed drop-in C++ Qt Widgets development package.
- ConanCenter lists Qt and OpenCASCADE Windows/MSVC Release packages.
- No ConanCenter VTK recipe was found during this check.

Observed from the previous incomplete vcpkg install in `build\windows-vcpkg`:

- vcpkg successfully built/installed `opencascade:x64-windows@8.0.0#1`.
- OCCT imported libraries are MSVC ABI artifacts such as `TKSTEP.lib` and `TKSTEP.dll`.
- The install stopped before Qt and VTK completed. It reached `lz4` after completing OCCT and OpenSSL.
- That previous plan listed VTK without the `qt` feature, so it was not sufficient for the current `find_package(VTK REQUIRED COMPONENTS ... GUISupportQt ...)` requirement.

## Recommended Path

Do not make full source builds the default path. Try these in order:

1. Reuse already compiled local MSVC packages.
   - Current reusable pieces are QtBase and OCCT from `build\windows-msvc-vcpkg\vcpkg_installed\x64-windows`.
   - The missing piece is VTK with Qt Widgets support.
   - If a compatible local `VTKConfig.cmake` is added later, use `windows-msvc-external-deps` with `MSVC_DEPS_PREFIX_PATH`.

2. Official Qt + package-managed CAD/visualization dependencies.
   - Install Qt with Qt Online Installer, component `msvc2022_64`.
   - Use vcpkg or Conan for OCCT.
   - Use a VTK package that is built against the same Qt install. If using vcpkg's stock `vtk[qt]`, expect vcpkg to install its own `qtbase`.
   - Configure this project with `windows-msvc-external-deps` after setting `MSVC_DEPS_PREFIX_PATH` to the Qt, VTK, and OCCT CMake package prefixes.

3. All-vcpkg MSVC dependencies.
   - Use `qtbase`, `vtk[qt]`, and `opencascade` from the same vcpkg triplet.
   - Prefer a configured binary cache. Without a cache, vcpkg will build missing packages from source.

4. Source-build fallback.
   - Use this only when the desired ABI/runtime/options cannot be obtained from packages.
   - Prefer `x64-windows-release` or the lean VTK overlay first to avoid Debug and QtQuick/QML cost.

Use project-local vcpkg in `extern\vcpkg` plus the new `windows-msvc-vcpkg` preset:

```powershell
.\scripts\configure_msvc_vcpkg.ps1 -BootstrapVcpkg
cmake --build --preset windows-msvc-vcpkg-release
```

For external prebuilt dependencies:

```powershell
$env:MSVC_DEPS_PREFIX_PATH = "C:\Qt\6.x.x\msvc2022_64;C:\path\to\VTK;C:\path\to\OCCT"
cmake --preset windows-msvc-external-deps
cmake --build --preset windows-msvc-external-deps-release
```

For dependency source builds on this machine, prefer the release-only triplet first:

```powershell
.\scripts\configure_msvc_vcpkg.ps1 -ReleaseOnly -ForceSourceBuild -MaxConcurrency 2
cmake --build --preset windows-msvc-vcpkg-release-only
```

This uses vcpkg's built-in `x64-windows-release` triplet for both target and host dependencies, which keeps the MSVC ABI and dynamic CRT/library linkage of `x64-windows` but skips Debug variants of third-party packages. It is the lowest-risk way to reduce Qt/VTK build time and disk usage.

For this Qt Widgets + `QVTKOpenGLNativeWidget` prototype, the lean VTK Qt overlay is the preferred source-build route:

```powershell
.\scripts\configure_msvc_vcpkg.ps1 -LeanVtkQt -ForceSourceBuild -MaxConcurrency 2
cmake --build --preset windows-msvc-vcpkg-lean-release
```

The overlay at `vcpkg-overlays\ports\vtk` is copied from the matching vcpkg VTK port and only narrows the VTK `qt` feature. It keeps `VTK_GUISupportQt` and `VTK_RenderingQt`, but removes `qtdeclarative`, `VTK_GROUP_ENABLE_Qt`, `VTK_GUISupportQtSQL`, and `VTK_ViewsQt`. This avoids building QtQuick/QML/QtDeclarative for a Widgets-only application.

The first configure can take a long time because vcpkg may build Qt, VTK, and OCCT from source if a binary cache is not already populated. The script enables a local file binary cache at `%LOCALAPPDATA%\vcpkg\archives`, but that only helps after a package has already been built once or imported into the cache.

To only use an existing vcpkg checkout:

```powershell
.\scripts\configure_msvc_vcpkg.ps1 -VcpkgRoot C:\path\to\vcpkg
cmake --build --preset windows-msvc-vcpkg-release
```

Equivalent raw CMake configure command:

```powershell
cmake -S . -B build\windows-msvc-vcpkg `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=extern\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_MODE=ON `
  -DVCPKG_APPLOCAL_DEPS=ON
```

## Differences From MSYS2 Preset

- Compiler changes from `C:\msys64\ucrt64\bin\g++.exe` to MSVC `cl.exe`.
- Generator changes from single-config Ninja to Visual Studio 2022 multi-config.
- Dependency root changes from `CMAKE_PREFIX_PATH=C:\msys64\ucrt64` to `CMAKE_TOOLCHAIN_FILE=extern\vcpkg\scripts\buildsystems\vcpkg.cmake`.
- Runtime deployment changes from copying MinGW/MSYS2 DLLs to vcpkg app-local deployment plus Qt deployment checks.
- Build output moves from `build\windows-msys2-ucrt64` to `build\windows-msvc-vcpkg`.

## Current Blocking Item

The full `x64-windows` source build pulled `qtdeclarative` through VTK's `qt` feature and was stopped during QtDeclarative Debug compilation to avoid unnecessary time and disk use. A release-only attempt reduced the plan but still included `qtdeclarative:x64-windows-release`, so it was also stopped before entering the heavy QtDeclarative stage. The already completed MSVC packages include OCCT and QtBase for the original `x64-windows` triplet.

The next recommended action is to locate or install a prebuilt VTK package compatible with the chosen Qt/MSVC ABI. Source-build VTK only if no compatible binary package is available. Specifically, check in this order:

1. Existing local `VTKConfig.cmake` from any prebuilt VTK package.
2. Conan package availability for the exact MSVC/runtime/Qt combination.
3. VTK wheel SDK feasibility for C++ linking and `QVTKOpenGLNativeWidget`.
4. vcpkg binary cache or all-vcpkg install.
5. Lean VTK source fallback.

If a package-compatible VTK is unavailable, the lean source fallback command is:

```powershell
.\scripts\configure_msvc_vcpkg.ps1 -LeanVtkQt -ForceSourceBuild -MaxConcurrency 2
```
