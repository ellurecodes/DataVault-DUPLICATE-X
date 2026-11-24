# scripts/run.ps1
# Execution script for DUPLICATE-X (Team DataVault)

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Directories
)

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$TargetExe     = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/build/duplicate-x.exe")

# 1. Check whether duplicate-x.exe exists; if not, trigger build
if (-not (Test-Path $TargetExe)) {
    Write-Host "[INFO] Executable 'duplicate-x.exe' not found. Triggering build..." -ForegroundColor Yellow
    & "$PSScriptRoot/build.ps1"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $TargetExe)) {
        Write-Host "`n[ERROR] Build step failed. Cannot execute DUPLICATE-X backend." -ForegroundColor Red
        exit 1
    }
}

# 2. Execute binary forwarding user arguments
Write-Host "[INFO] Launching DUPLICATE-X backend ($TargetExe)...`n" -ForegroundColor Cyan

if ($Directories -and $Directories.Count -gt 0) {
    & "$TargetExe" @Directories
} else {
    & "$TargetExe"
}

$appExitCode = $LASTEXITCODE

Write-Host "`n[INFO] Backend process exited with code $appExitCode." -ForegroundColor Gray
exit $appExitCode
