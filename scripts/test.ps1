# scripts/test.ps1
# Test orchestrator for DUPLICATE-X (Team DataVault)

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$IncludeDir    = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/include")
$OutputDir     = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/build")
$TestExe       = [System.IO.Path]::GetFullPath("$OutputDir/test_duplicate_detector.exe")

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Test Suite Runner" -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Ensure output directory exists
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# 2. Ensure test fixture files exist
& "$PSScriptRoot/create_test_files.ps1"

# 3. Locate GCC
$GccPath = $null
$GccCmd = Get-Command gcc -ErrorAction SilentlyContinue
if ($GccCmd -and $GccCmd.Source) {
    $GccPath = $GccCmd.Source
} else {
    $KnownWingetPath = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe"
    if (Test-Path $KnownWingetPath) {
        $GccPath = $KnownWingetPath
    }
}

if (-not $GccPath) {
    Write-Host "[ERROR] GCC compiler not found in PATH or standard WinLibs location." -ForegroundColor Red
    exit 1
}

$GccBinDir = Split-Path -Parent $GccPath
if ($env:PATH -notlike "*$GccBinDir*") {
    $env:PATH = "$GccBinDir;$env:PATH"
}

# 4. Source files for test binary
$TestSources = @(
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/tests/test_duplicate_detector.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/duplicate_detector.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/hashtable.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/duplicate_group.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_crypto.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_compare.c")
)

Write-Host "`n[INFO] Compiling Part 4 verification test suite..." -ForegroundColor Cyan
& "$GccPath" -std=c11 -Wall -Wextra -Wpedantic -municode "-I$IncludeDir" $TestSources -o "$TestExe" -lbcrypt
$compileExit = $LASTEXITCODE

if ($compileExit -ne 0 -or -not (Test-Path $TestExe)) {
    Write-Host "[ERROR] Test compilation failed with exit code $compileExit." -ForegroundColor Red
    exit 1
}

Write-Host "[SUCCESS] Test binary compiled: $TestExe" -ForegroundColor Green
Write-Host "`n[INFO] Launching Part 4 verification test executable...`n" -ForegroundColor Cyan

# 5. Execute test binary and capture runtime exit code
$testProcess = Start-Process -FilePath "$TestExe" -Wait -NoNewWindow -PassThru -ErrorAction SilentlyContinue

if ($null -eq $testProcess) {
    # Fallback to direct call operator in case Start-Process encounters environment restrictions
    try {
        & "$TestExe"
        $testExit = $LASTEXITCODE
    } catch {
        Write-Host "`n[ERROR] Failed to execute test binary: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "[BLOCKED] Execution blocked by host operating system security (e.g., Windows Application Control)." -ForegroundColor Yellow
        exit 126
    }
} else {
    $testExit = $testProcess.ExitCode
}

if ($testExit -ne 0) {
    Write-Host "`n[FAILURE] Test suite failed with exit code $testExit." -ForegroundColor Red
    exit $testExit
}

Write-Host "`n[SUCCESS] All test suite assertions passed successfully (exit code $testExit)." -ForegroundColor Green
exit 0
