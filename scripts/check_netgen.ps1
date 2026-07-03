Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$Candidates = @()

if ($env:NETGEN_EXE) {
    $Candidates += $env:NETGEN_EXE
}

$Candidates += Join-Path $Root "build\windows-msys2-ucrt64\netgen.exe"
$Candidates += Join-Path $Root "extern\netgen\bin\netgen.exe"
$Candidates += "C:\msys64\ucrt64\bin\netgen.exe"

$PathCommand = Get-Command netgen.exe -ErrorAction SilentlyContinue
if ($PathCommand) {
    $Candidates += $PathCommand.Source
}

$Seen = @{}
foreach ($Candidate in $Candidates) {
    if (-not $Candidate) {
        continue
    }
    $FullPath = [System.IO.Path]::GetFullPath($Candidate)
    if ($Seen.ContainsKey($FullPath)) {
        continue
    }
    $Seen[$FullPath] = $true
    if (Test-Path -LiteralPath $FullPath) {
        "FOUND: $FullPath"
        try {
            & $FullPath -help 2>&1 | Select-Object -First 20
        } catch {
            "Netgen executable exists, but running '-help' failed: $($_.Exception.Message)"
        }
        exit 0
    }
}

$PythonCandidates = @()
if ($env:NETGEN_PYTHON) {
    $PythonCandidates += $env:NETGEN_PYTHON
}
$PythonCandidates += Join-Path $Root "extern\netgen\python-env\Scripts\python.exe"

foreach ($Candidate in $PythonCandidates) {
    if (-not $Candidate) {
        continue
    }
    $FullPath = [System.IO.Path]::GetFullPath($Candidate)
    if (Test-Path -LiteralPath $FullPath) {
        "FOUND: $FullPath"
        try {
            & $FullPath -c "import netgen, netgen.occ; print('Netgen Python backend ' + netgen.__version__)" 2>&1 | Select-Object -First 20
        } catch {
            "Netgen Python backend exists, but import failed: $($_.Exception.Message)"
            exit 1
        }
        exit 0
    }
}

"Netgen was not found."
"Run .\scripts\download_netgen_windows.ps1, set NETGEN_EXE, or place netgen.exe at extern\netgen\bin\netgen.exe."
exit 1
