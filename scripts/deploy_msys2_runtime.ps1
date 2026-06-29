param(
    [string]$TargetDir = (Join-Path (Split-Path -Parent $PSScriptRoot) "build\windows-msys2-ucrt64")
)

$ErrorActionPreference = "Stop"

$MsysBin = "C:\msys64\ucrt64\bin"
$Objdump = Join-Path $MsysBin "objdump.exe"

if (-not (Test-Path $Objdump)) {
    throw "objdump.exe was not found. Install MSYS2 UCRT64 binutils first."
}

$TargetDir = (Resolve-Path $TargetDir).Path
$queue = New-Object System.Collections.Generic.Queue[string]
$seen = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
$systemPrefixes = @(
    "api-ms-win-",
    "ext-ms-"
)
$systemDlls = @(
    "advapi32.dll", "authz.dll", "bcrypt.dll", "bcryptprimitives.dll",
    "comctl32.dll", "comdlg32.dll", "crypt32.dll", "cryptbase.dll", "d3d9.dll",
    "d3d11.dll", "d3d12.dll", "d3dcompiler_47.dll", "dnsapi.dll", "dwmapi.dll",
    "dwrite.dll", "dxcore.dll", "dxgi.dll", "gdiplus.dll", "gdi32.dll",
    "glu32.dll", "imm32.dll", "iphlpapi.dll", "kernel32.dll", "kernelbase.dll",
    "mpr.dll", "msimg32.dll", "msvcp_win.dll", "msvcrt.dll", "ncrypt.dll",
    "netapi32.dll", "netutils.dll", "ntdll.dll", "ole32.dll", "oleaut32.dll",
    "opengl32.dll", "powrprof.dll", "psapi.dll", "rpcrt4.dll", "sechost.dll",
    "secur32.dll", "setupapi.dll", "shcore.dll", "shell32.dll", "shlwapi.dll",
    "srvcli.dll", "ucrtbase.dll", "user32.dll", "userenv.dll", "usp10.dll",
    "uxtheme.dll", "version.dll", "win32u.dll", "winhttp.dll", "winmm.dll",
    "winspool.drv", "ws2_32.dll", "wsock32.dll", "wtsapi32.dll"
)

function Is-SystemDll([string]$name) {
    $lower = $name.ToLowerInvariant()
    foreach ($prefix in $systemPrefixes) {
        if ($lower.StartsWith($prefix)) {
            return $true
        }
    }
    return $systemDlls -contains $lower
}

function Enqueue-Binary([string]$path) {
    if ((Test-Path $path) -and $seen.Add((Resolve-Path $path).Path)) {
        $queue.Enqueue((Resolve-Path $path).Path)
    }
}

Get-ChildItem $TargetDir -Recurse -Include *.exe,*.dll | ForEach-Object {
    Enqueue-Binary $_.FullName
}

while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    $output = & $Objdump -p $binary 2>$null
    foreach ($line in $output) {
        if ($line -match "DLL Name:\s+(.+)$") {
            $dll = $Matches[1].Trim()
            if (Is-SystemDll $dll) {
                continue
            }

            $local = Join-Path $TargetDir $dll
            if (Test-Path $local) {
                Enqueue-Binary $local
                continue
            }

            $source = Join-Path $MsysBin $dll
            if (Test-Path $source) {
                Copy-Item -LiteralPath $source -Destination $local -Force
                Write-Output "deployed $dll"
                Enqueue-Binary $local
            } else {
                Write-Warning "Missing non-system DLL: $dll required by $binary"
            }
        }
    }
}
