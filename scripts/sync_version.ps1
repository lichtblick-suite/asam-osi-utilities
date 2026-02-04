#
# Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# Synchronize version from VERSION file to vcpkg.json and Doxyfile.in
# This script ensures all version references stay in sync.
# Run this script when updating the version.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

$VersionFile = Join-Path $RootDir "VERSION"
$VcpkgFile = Join-Path $RootDir "vcpkg.json"
$Doxyfile = Join-Path $RootDir "doc" "Doxyfile.in"

# Read version from VERSION file
if (-not (Test-Path $VersionFile)) {
    Write-Host "ERROR: VERSION file not found at $VersionFile" -ForegroundColor Red
    exit 1
}

$Version = (Get-Content $VersionFile -Raw).Trim()

# Validate version format (MAJOR.MINOR.PATCH)
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Host "ERROR: Invalid version format '$Version'. Expected MAJOR.MINOR.PATCH" -ForegroundColor Red
    exit 1
}

Write-Host "Version from VERSION file: $Version"

# Update vcpkg.json
if (Test-Path $VcpkgFile) {
    $VcpkgContent = Get-Content $VcpkgFile -Raw | ConvertFrom-Json
    $OldVersion = $VcpkgContent.'version-string'
    
    $VcpkgContent.'version-string' = $Version
    $VcpkgContent | ConvertTo-Json -Depth 10 | Set-Content $VcpkgFile -NoNewline
    Add-Content $VcpkgFile ""  # Add trailing newline
    
    if ($OldVersion -ne $Version) {
        Write-Host "Updated vcpkg.json: $OldVersion -> $Version" -ForegroundColor Green
    }
    else {
        Write-Host "vcpkg.json already at version $Version"
    }
}
else {
    Write-Host "WARNING: vcpkg.json not found at $VcpkgFile" -ForegroundColor Yellow
}

# Update Doxyfile.in
if (Test-Path $Doxyfile) {
    $DoxyContent = Get-Content $Doxyfile -Raw
    $OldDoxyVersion = if ($DoxyContent -match 'PROJECT_NUMBER\s*=\s*(.*)') { $Matches[1].Trim() } else { "unknown" }
    
    $NewDoxyContent = $DoxyContent -replace 'PROJECT_NUMBER\s*=\s*.*', "PROJECT_NUMBER         = $Version"
    
    if ($DoxyContent -ne $NewDoxyContent) {
        Set-Content $Doxyfile $NewDoxyContent -NoNewline
        Write-Host "Updated Doxyfile.in PROJECT_NUMBER to $Version" -ForegroundColor Green
    }
    else {
        Write-Host "Doxyfile.in already at version $Version"
    }
}
else {
    Write-Host "WARNING: Doxyfile.in not found at $Doxyfile" -ForegroundColor Yellow
}

Write-Host "Version synchronization complete!" -ForegroundColor Green
