<#
.SYNOPSIS
    Automated WebAssembly & WebGPU build & serve script for Eatscharted 2.5D Living Cartography Engine.

.DESCRIPTION
    Configures and compiles Eatscharted into WebAssembly + WebGPU (WGSL) using Emscripten (emdawnwebgpu).
    Optionally serves the built application locally on 0.0.0.0:$Port with COOP/COEP headers for desktop and mobile testing.

.EXAMPLE
    .\build-web.ps1
    .\build-web.ps1 -NoServe
    .\build-web.ps1 -Port 9000
    .\build-web.ps1 -Clean -NoServe
#>

[CmdletBinding()]
param (
    [switch]$Clean,
    [switch]$NoServe,
    [switch]$Serve,
    [int]$Port = 8080,
    [switch]$BootstrapEmsdk
)

$ErrorActionPreference = "Stop"

$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}
Set-Location $rootDir

Write-Host "==================================================================" -ForegroundColor Cyan
Write-Host "  Eatscharted WebAssembly & WebGPU Living Cartography Build" -ForegroundColor Cyan
Write-Host "==================================================================" -ForegroundColor Cyan

function Stop-ExistingWebServer {
    param([int]$TargetPort = 8080)
    try {
        $connections = Get-NetTCPConnection -LocalPort $TargetPort -ErrorAction SilentlyContinue
        if ($connections) {
            $pidsToKill = $connections | Select-Object -ExpandProperty OwningProcess -Unique | Where-Object { $_ -gt 0 -and $_ -ne $PID }
            foreach ($p in $pidsToKill) {
                Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
            }
        }
    } catch {}
}

# ------------------------------------------------------------------
# STEP 1: Verify Emscripten Toolchain
# ------------------------------------------------------------------
Write-Host "`n[*] STEP 1: Verifying Emscripten (emcc) toolchain..." -ForegroundColor Yellow

$emsdkCandidatePaths = @(
    "C:\emsdk",
    "$env:USERPROFILE\emsdk",
    "C:\git\eatsbits\emsdk",
    (Join-Path (Split-Path $rootDir -Parent) "eatsbits\emsdk"),
    (Join-Path $rootDir "emsdk")
)

if (-not (Get-Command "emcc" -ErrorAction SilentlyContinue)) {
    $foundEmsdk = $null
    foreach ($cand in $emsdkCandidatePaths) {
        if (Test-Path $cand) {
            $foundEmsdk = $cand
            break
        }
    }

    if ($foundEmsdk) {
        Write-Host "    Found Emscripten SDK at: $foundEmsdk" -ForegroundColor Green
        $batScript = Join-Path $foundEmsdk "emsdk_env.bat"
        if (Test-Path $batScript) {
            cmd.exe /c "call `"$batScript`" > nul && set" | ForEach-Object {
                if ($_ -match '^(.*?)=(.*)$') {
                    [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                }
            }
        }
    } elseif ($BootstrapEmsdk) {
        $emsdkDir = Join-Path $rootDir "emsdk"
        if (-not (Test-Path $emsdkDir)) {
            git clone https://github.com/emscripten-core/emsdk.git $emsdkDir
        }
        Set-Location $emsdkDir
        .\emsdk.bat install latest
        .\emsdk.bat activate latest
        . .\emsdk_env.ps1
        Set-Location $rootDir
    } else {
        Write-Error "Emscripten compiler (emcc) was not found. Install C:\emsdk or run .\build-web.ps1 -BootstrapEmsdk"
        exit 1
    }
}

$emccVer = (emcc --version)[0]
Write-Host "    Found Emscripten: $emccVer" -ForegroundColor Green

# ------------------------------------------------------------------
# STEP 2: Configure & Build Web Target
# ------------------------------------------------------------------
$buildDir = Join-Path $rootDir "build-web"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "`n[*] Cleaning previous web build directory ('$buildDir')..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

try {
    $ninjaDir = python -c "import ninja; print(ninja.BIN_DIR)" 2>$null
    if ($ninjaDir -and (Test-Path $ninjaDir)) {
        $env:PATH = "$ninjaDir;$env:PATH"
    }
    $userBase = python -m site --user-base 2>$null
    if ($userBase) {
        $userScripts = Join-Path $userBase "Scripts"
        if (Test-Path $userScripts) {
            $env:PATH = "$userScripts;$env:PATH"
        }
    }
} catch {}

Write-Host "`n[*] STEP 2: Configuring CMake with Emscripten..." -ForegroundColor Yellow
emcmake cmake -B build-web -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    exit $LASTEXITCODE
}

Write-Host "`n[*] STEP 3: Compiling eatscharted (WebGPU + WASM + Eatscript + Foley DSP)..." -ForegroundColor Yellow
cmake --build build-web --target eatscharted --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed."
    exit $LASTEXITCODE
}

if (Test-Path (Join-Path $buildDir "eatscharted.html")) {
    Copy-Item (Join-Path $buildDir "eatscharted.html") -Destination (Join-Path $buildDir "index.html") -Force
}

Write-Host "`n[+] Web build successful!" -ForegroundColor Green

# ------------------------------------------------------------------
# Payload & Binary Size Analysis (Raw vs Gzip Wire Transfer)
# ------------------------------------------------------------------
Write-Host "`n========================================================================================" -ForegroundColor Cyan
Write-Host "  Eatscharted WebAssembly & WebGPU Payload Analysis (Asset & Wire Footprint)" -ForegroundColor Cyan
Write-Host "========================================================================================" -ForegroundColor Cyan

$webArtifacts = @(
    @{ Name = "eatscharted.wasm"; Desc = "WebAssembly 2.5D Living Cartography Engine" },
    @{ Name = "eatscharted.data"; Desc = "Preloaded Eatscript Chart Recipes" },
    @{ Name = "eatscharted.js";   Desc = "Emscripten & emdawnwebgpu JS Runtime Bridge" },
    @{ Name = "index.html";       Desc = "HTML5 WebGPU Host & Mobile Touch UI" }
)

$totalRaw = 0
$totalWire = 0
$metrics = @()

foreach ($item in $webArtifacts) {
    $p = Join-Path $buildDir $item.Name
    if (Test-Path $p) {
        $rawBytes = (Get-Item $p).Length
        $totalRaw += $rawBytes

        $rawBuffer = [System.IO.File]::ReadAllBytes($p)
        $ms = New-Object System.IO.MemoryStream
        $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Compress)
        $gz.Write($rawBuffer, 0, $rawBuffer.Length)
        $gz.Close()
        $wireBytes = $ms.ToArray().Length
        $ms.Close()
        $totalWire += $wireBytes

        $savedPct = if ($rawBytes -gt 0) { [math]::Round((1.0 - ($wireBytes / $rawBytes)) * 100.0, 1) } else { 0.0 }

        $metrics += [PSCustomObject]@{
            Artifact  = $item.Name
            Desc      = $item.Desc
            RawBytes  = $rawBytes
            WireBytes = $wireBytes
            SavedPct  = $savedPct
        }
    }
}

Write-Host ("  {0,-18} {1,12} {2,14} {3,10}   {4,-36}" -f "Artifact", "Raw Size", "Wire (Gzip)", "Savings", "Description") -ForegroundColor Yellow
Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray

foreach ($m in $metrics) {
    $rawStr = if ($m.RawBytes -ge 1MB) { "{0:N2} MB" -f ($m.RawBytes / 1MB) } else { "{0:N1} KB" -f ($m.RawBytes / 1KB) }
    $wireStr = if ($m.WireBytes -ge 1MB) { "{0:N2} MB" -f ($m.WireBytes / 1MB) } else { "{0:N1} KB" -f ($m.WireBytes / 1KB) }
    $savedStr = "-{0:N1}%" -f $m.SavedPct
    Write-Host ("  {0,-18} {1,12} {2,14} {3,10}   {4,-36}" -f $m.Artifact, $rawStr, $wireStr, $savedStr, $m.Desc) -ForegroundColor White
}

Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray
$totRawStr = if ($totalRaw -ge 1MB) { "{0:N2} MB" -f ($totalRaw / 1MB) } else { "{0:N1} KB" -f ($totalRaw / 1KB) }
$totWireStr = if ($totalWire -ge 1MB) { "{0:N2} MB" -f ($totalWire / 1MB) } else { "{0:N1} KB" -f ($totalWire / 1KB) }
$totSavedPct = if ($totalRaw -gt 0) { [math]::Round((1.0 - ($totalWire / $totalRaw)) * 100.0, 1) } else { 0.0 }
$totSavedStr = "-{0:N1}%" -f $totSavedPct
Write-Host ("  {0,-18} {1,12} {2,14} {3,10}   {4,-36}" -f "TOTAL PAYLOAD", $totRawStr, $totWireStr, $totSavedStr, "CDN / Mobile Wire Footprint") -ForegroundColor Green
Write-Host "========================================================================================`n" -ForegroundColor Cyan

# ------------------------------------------------------------------
# STEP 4: Local Dev Server with COOP / COEP Headers
# ------------------------------------------------------------------
if (-not $NoServe) {
    Stop-ExistingWebServer -TargetPort $Port
    Write-Host "`n[*] STEP 4: Starting local HTTP server on port $Port..." -ForegroundColor Yellow
    Write-Host "    [+] Desktop URL: http://localhost:$Port/index.html" -ForegroundColor Green

    $serverScript = @"
import os, http.server, socketserver

web_dir = r'$buildDir'
if os.path.exists(web_dir):
    os.chdir(web_dir)

class CustomHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('', $Port), CustomHandler) as httpd:
    print('Serving HTTP on 0.0.0.0 port $Port with COOP/COEP headers...', flush=True)
    httpd.serve_forever()
"@

    $serverScriptPath = Join-Path $buildDir "serve.py"
    Set-Content -Path $serverScriptPath -Value $serverScript
    Start-Process "http://localhost:$Port/index.html"
    python $serverScriptPath
} else {
    Write-Host "[*] Build completed. Server launch skipped (-NoServe specified)." -ForegroundColor Gray
}
