# scripts/create_test_files.ps1
# Safe test fixture generator for DUPLICATE-X (Team DataVault)

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$TestRoot      = [System.IO.Path]::GetFullPath("$WorkspaceRoot/data/test_files")
$LaptopDir     = "$TestRoot/device_laptop"
$UsbDir        = "$TestRoot/device_usb"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Test Fixture Generator" -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Ensure test directories exist
foreach ($dir in @($LaptopDir, $UsbDir)) {
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
}

# 2. Write exact ASCII bytes without BOM or trailing newline characters
$notesPath = "$LaptopDir/notes.txt"
$uniquePath = "$LaptopDir/unique.txt"
$renamedPath = "$UsbDir/renamed_notes.txt"
$emptyPath = "$TestRoot/empty.txt"

# Exact byte payloads
[System.IO.File]::WriteAllBytes($notesPath, [System.Text.Encoding]::ASCII.GetBytes("abc"))
[System.IO.File]::WriteAllBytes($uniquePath, [System.Text.Encoding]::ASCII.GetBytes("abd"))
[System.IO.File]::WriteAllBytes($renamedPath, [System.Text.Encoding]::ASCII.GetBytes("abc"))
[System.IO.File]::WriteAllBytes($emptyPath, @())

Write-Host "`n[SUCCESS] Test fixture files generated:" -ForegroundColor Green
Write-Host "  - $notesPath ($(Get-Item $notesPath).Length bytes, content: 'abc')" -ForegroundColor White
Write-Host "  - $uniquePath ($(Get-Item $uniquePath).Length bytes, content: 'abd')" -ForegroundColor White
Write-Host "  - $renamedPath ($(Get-Item $renamedPath).Length bytes, content: 'abc')" -ForegroundColor White
Write-Host "  - $emptyPath ($(Get-Item $emptyPath).Length bytes, content: empty)" -ForegroundColor White

Write-Host "`nExpected Known SHA-256 for 'abc':" -ForegroundColor Yellow
Write-Host "  ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" -ForegroundColor Gray
Write-Host "Expected Known SHA-256 for empty file:" -ForegroundColor Yellow
Write-Host "  e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" -ForegroundColor Gray

