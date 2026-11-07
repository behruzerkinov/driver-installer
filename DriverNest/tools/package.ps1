# package.ps1
# Builds, tests, and packages DriverNest.
#
# Usage:
#   .\tools\package.ps1 -Version 1.0.0 [-CertThumbprint <thumbprint>]
#
# Requirements:
#   - CMake 3.25+
#   - MSVC v143 (Visual Studio 2022 Build Tools)
#   - Windows SDK 10.0.22621.0 or newer
#   - PowerShell 5.1+

param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [string]$CertThumbprint
)

$ErrorActionPreference = 'Stop'

$rootDir = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $rootDir 'out'
$buildDir = Join-Path $outDir 'release-x64'
$binDir = Join-Path $buildDir 'bin'
$distDir = Join-Path $rootDir 'dist'
$packageDir = Join-Path $distDir "DriverNest-$Version-x64"

Write-Host "=============================================="
Write-Host "DriverNest Packaging Script"
Write-Host "Version: $Version"
Write-Host "=============================================="
Write-Host ""

# Step 1: Build release
Write-Host "[1/5] Building release..."
Push-Location $rootDir
try {
    cmake --preset release-x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed"
    }

    cmake --build --preset release-x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed"
    }
}
finally {
    Pop-Location
}

# Step 2: Run tests
Write-Host "[2/5] Running tests..."
Push-Location $rootDir
try {
    $testResult = ctest --preset release-x64 --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed"
    }
}
finally {
    Pop-Location
}

# Step 3: Copy files to package directory
Write-Host "[3/5] Packaging..."
if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

# Copy executable (multi-config generators nest it under <Config>\)
$exeSource = @(
    (Join-Path $binDir 'DriverNest.exe'),
    (Join-Path $binDir 'Release\DriverNest.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exeSource) {
    throw "DriverNest.exe not found under $binDir"
}
Copy-Item -Path $exeSource -Destination $packageDir -Force

# Copy README
Copy-Item -Path (Join-Path $rootDir 'README.md') -Destination (Join-Path $packageDir 'README.txt') -Force

# Copy LICENSE
Copy-Item -Path (Join-Path $rootDir 'LICENSE') -Destination (Join-Path $packageDir 'LICENSE') -Force

# Generate SHA256SUMS.txt
$exePath = Join-Path $packageDir 'DriverNest.exe'
$hash = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash
$size = (Get-Item $exePath).Length
$sha256File = Join-Path $packageDir 'SHA256SUMS.txt'
@"
DriverNest.exe $hash $size
"@ | Out-File -FilePath $sha256File -Encoding UTF8

Write-Host "  SHA256: $hash"
Write-Host "  Size:   $size bytes"

# Step 4: Create zip
Write-Host "[4/5] Creating zip archive..."
$zipPath = Join-Path $distDir "DriverNest-$Version-x64.zip"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

# Use Compress-Archive (built-in PowerShell)
Compress-Archive -Path $packageDir -DestinationPath $zipPath -Force

Write-Host "  Zip: $zipPath"

# Step 5: Sign if certificate provided
if (-not [string]::IsNullOrEmpty($CertThumbprint)) {
    Write-Host "[5/5] Signing with certificate: $CertThumbprint"

    $signtool = Get-Command 'signtool.exe' -ErrorAction SilentlyContinue
    if (-not $signtool) {
        Write-Warning "signtool.exe not found. Install Windows SDK to sign."
    } else {
        $timestampServer = "http://timestamp.digicert.com"
        & signtool.exe sign /fd SHA256 /tr $timestampServer /td SHA256 /f $CertThumbprint $exePath
        if ($LASTEXITCODE -ne 0) {
            throw "signtool failed"
        }
        Write-Host "  Signed successfully."

        # Update SHA256SUMS.txt after signing (hash may change)
        $hash = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash
        $size = (Get-Item $exePath).Length
        @"
DriverNest.exe $hash $size
"@ | Out-File -FilePath $sha256File -Encoding UTF8

        # Recreate zip with signed exe
        Remove-Item -Force $zipPath
        Compress-Archive -Path $packageDir -DestinationPath $zipPath -Force
        Write-Host "  Zip recreated with signed executable."
    }
} else {
    Write-Host "[5/5] Skipping signing (no certificate provided)."
    Write-Host "      To sign, run: .\tools\package.ps1 -Version $Version -CertThumbprint <thumbprint>"
}

Write-Host ""
Write-Host "=============================================="
Write-Host "Packaging complete!"
Write-Host "=============================================="
Write-Host ""
Write-Host "Package directory: $packageDir"
Write-Host "Zip archive:       $zipPath"
Write-Host ""
Write-Host "Contents:"
Get-ChildItem $packageDir | ForEach-Object {
    Write-Host "  $($_.Name) ($($_.Length) bytes)"
}
