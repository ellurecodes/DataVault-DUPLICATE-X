# scripts/run_server.ps1
# Starts the DUPLICATE-X C11 REST server on 127.0.0.1:8080

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$ExePath = "$WorkspaceRoot\backend\build\duplicate-x.exe"

if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found at $ExePath. Please run scripts/build.ps1 first."
    exit 1
}

# Unblock file in case of Windows Execution Policy or OneDrive zone security
Unblock-File -Path $ExePath -ErrorAction SilentlyContinue

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Starting DUPLICATE-X REST API Server     " -ForegroundColor Cyan
Write-Host " http://127.0.0.1:8080                   " -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Cyan

Set-Location $WorkspaceRoot
& $ExePath --server

