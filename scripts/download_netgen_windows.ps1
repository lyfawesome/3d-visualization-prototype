Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
    [string]$NetgenRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) "extern\netgen"),
    [string]$Package = "netgen-mesher"
)

$Downloads = Join-Path $NetgenRoot "downloads"
$Venv = Join-Path $NetgenRoot "python-env"
$Python = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $Python) {
    throw "python.exe was not found on PATH. Install Python 3.14+ or set PATH before running this script."
}

New-Item -ItemType Directory -Force -Path $Downloads | Out-Null

& $Python.Source -m pip download $Package --only-binary=:all: --dest $Downloads
if ($LASTEXITCODE -ne 0) {
    throw "Downloading prebuilt Netgen wheels failed."
}

if (-not (Test-Path -LiteralPath $Venv)) {
    & $Python.Source -m venv $Venv
    if ($LASTEXITCODE -ne 0) {
        throw "Creating Netgen Python environment failed."
    }
}

$VenvPython = Join-Path $Venv "Scripts\python.exe"
& $VenvPython -m pip install --no-index --find-links $Downloads $Package
if ($LASTEXITCODE -ne 0) {
    throw "Installing prebuilt Netgen wheels into the local environment failed."
}

"Netgen prebuilt Python backend installed:"
"  $VenvPython"
"Run .\scripts\check_netgen.ps1 to verify discovery."
