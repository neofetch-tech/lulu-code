#!/usr/bin/env powershell
# build-windows.ps1 — Build Lulu Code on Windows (MSVC or MinGW)
# Requires Qt 6 installed with the MSVC or MinGW kit, or MinGW-w64 with Qt plugins.
# Run from a Developer PowerShell prompt (MSVC) or a MinGW64 shell.

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('MSVC', 'MinGW')]
    [string]$Compiler='MSVC',

    [int]$Jobs = 8
)

$ErrorActionPreference = 'Stop'

$home = $env:USERPROFILE
$project = Resolve-Path . -ErrorVariable _
if (-not $ _) { throw "Must run from project root." }

Write-Host "=== Lulu Code Windows Build ===" -ForegroundColor Cyan

# Determine Qt prefix based on compiler
if ($Compiler -eq 'MSVC') {
    # Typical MSVC 2022 install location
    $qtPaths = @(
        "C:\Program Files\Qt\Qt6.8\6.8.0\msvc2022_64",
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.x.x\msvc2022_64"
    )
    $qtDir = $qtPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $qtDir) {
        Write-Error "No MSVC Qt 6 installation found. Expected under C:\Qt\..."
        return
    }
    $qtPrefix = Join-Path $qtDir "bin"
    $cmakeArgs = "-DCMAKE_PREFIX_PATH=$qtDir"
} else {
    # MinGW — assume Qt is on PATH or in a known location
    $qtDir = Get-ChildItem "C:\Qt\Qt6.x\mingw64" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $qtDir) {
        Write-Error "No MinGW Qt 6 installation found."
        return
    }
    $qtPrefix = Join-Path $qtDir "bin"
    $cmakeArgs = ""
}

$llamaDir = "/root/llama.cpp"  # WSL path; on native Windows adjust as needed
$llamaExists = Test-Path "$llamaDir\CMakeLists.txt"
if (-not $llamaExists) {
    Write-Warning "llama.cpp not found at $llamaDir. Set LULU_LLAMA_SOURCE_DIR or ensure it is available."
}

# Create build directory
$buildDir = Join-Path $project "build-windows"
if (Test-Path $buildDir) { Remove-Item $buildDir -Recurse -Force }
New-Item -ItemType Directory -Path $buildDir | Out-Null

# Configure with CMake
cd $buildDir
Write-Host "Running cmake configure..." -ForegroundColor Yellow
cmake "$project" `
    -DCMAKE_BUILD_TYPE=Release `
    -DLULU_WITH_LLAMA=ON `
    -DLULU_FETCH_LLAMA=ON `
    -DLULU_LLAMA_TAG=b10333 `
    -DLULU_BUILD_TESTS=OFF `
    $cmakeArgs

Write-Host "Configuring done." -ForegroundColor Green

# Build
Write-Host "Building (jobs=$Jobs)..." -ForegroundColor Yellow
cmake --build $buildDir --config Release -j $Jobs

Write-Host "Build complete." -ForegroundColor Cyan
Write-Host "Binary: $buildDir\Release\lulu_code.exe" -ForegroundColor Cyan

# Create ZIP archive for distribution
$zipPath = Join-Path $buildDir "lulu_code.zip"
Write-Host "Creating ZIP archive..." -ForegroundColor Yellow
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory("$buildDir\Release\lulu_code.exe", $zipPath) | Out-Null
Write-Host "Archive: $zipPath" -ForegroundColor Cyan

# Optional: collect Qt DLLs next to the executable
if ($Compiler -eq 'MSVC') {
    $windeployqt = Join-Path $qtDir "windeployqt.exe"
    if (Test-Path $windeployqt) {
        Write-Host "Running windeployqt..." -ForegroundColor Yellow
        & $windeployqt "$buildDir\Release\lulu_code.exe"
    }
}