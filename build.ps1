<#
.SYNOPSIS
    Automated build script for Eatscharted 2.5D Living Cartography Engine (Perilous Shores x Cat Quest).
#>

[CmdletBinding()]
param (
    [switch]$Clean,
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release",
    [switch]$Test,
    [switch]$RenderPreview,
    [switch]$Run,
    [switch]$NoLaunch,
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

Write-Host "==========================================================================================" -ForegroundColor Cyan
Write-Host "  Eatscharted 2.5D Living Cartography Engine (Perilous Shores x Cat Quest) Build Script" -ForegroundColor Cyan
Write-Host "==========================================================================================" -ForegroundColor Cyan

if ($Clean -and (Test-Path "build")) {
    Write-Host "[Eatscharted] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

cmake -B build -S . -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

cmake --build build --config $Config --target eatscharted --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

$exeDir = Join-Path $PSScriptRoot "build\$Config"
if (-not (Test-Path (Join-Path $exeDir "eatscharted.exe"))) {
    $exeDir = Join-Path $PSScriptRoot "build"
}
$exePath = Join-Path $exeDir "eatscharted.exe"

Write-Host "[Eatscharted] Built successfully: $exePath" -ForegroundColor Green

if ($NoRun -or $NoLaunch) {
    Write-Host "[Eatscharted] -NoRun / -NoLaunch specified; skipping execution." -ForegroundColor Gray
    exit 0
}

if ($RenderPreview) {
    & $exePath --script scenes/perilous_shores.eats --render-png perilous_shores_preview.png
    exit $LASTEXITCODE
}

& $exePath
