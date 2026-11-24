# scripts/test_api.ps1
# Automated PowerShell runner for DUPLICATE-X Part 6A/6B API & Frontend test suite

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$IncludeDir    = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/include")
$OutputDir     = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/build")
$ApiTestExe    = [System.IO.Path]::GetFullPath("$OutputDir/test_api.exe")

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Part 6A/6B API & UI Test Runner" -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Locate GCC
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
    Write-Host "[ERROR] GCC compiler not found in PATH or standard location." -ForegroundColor Red
    exit 1
}

$GccBinDir = Split-Path -Parent $GccPath
if ($env:PATH -notlike "*$GccBinDir*") {
    $env:PATH = "$GccBinDir;$env:PATH"
}

# 2. Compile API test executable
$ApiTestSources = @(
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/tests/test_api.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_server.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_router.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_response.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/duplicate_detector.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/quarantine.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/quarantine_manifest.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/hashtable.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/duplicate_group.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/minheap.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/trie.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_crypto.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_compare.c")
)

Write-Host "`n[INFO] Compiling Part 6A API test suite..." -ForegroundColor Cyan
& "$GccPath" -std=c11 -Wall -Wextra -Wpedantic "-I$IncludeDir" $ApiTestSources -o "$ApiTestExe" -lbcrypt -lws2_32 -lole32 -lshell32 -luuid
$compileExit = $LASTEXITCODE

if ($compileExit -ne 0 -or -not (Test-Path $ApiTestExe)) {
    Write-Host "[ERROR] API test compilation failed with exit code $compileExit." -ForegroundColor Red
    exit 1
}

Write-Host "[SUCCESS] API test binary compiled: $ApiTestExe" -ForegroundColor Green
Write-Host "`n[INFO] Launching Part 6A API test executable...`n" -ForegroundColor Cyan

$testProcess = Start-Process -FilePath "$ApiTestExe" -Wait -NoNewWindow -PassThru -ErrorAction SilentlyContinue

if ($null -eq $testProcess) {
    try {
        & "$ApiTestExe"
        $testExit = $LASTEXITCODE
    } catch {
        Write-Host "`n[ERROR] Failed to execute API test binary: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "[BLOCKED] Execution blocked by host operating system security." -ForegroundColor Yellow
        exit 126
    }
} else {
    $testExit = $testProcess.ExitCode
}

if ($testExit -ne 0) {
    Write-Host "`n[FAILURE] Part 6A API test suite failed with exit code $testExit." -ForegroundColor Red
    exit $testExit
}

Write-Host "`n[SUCCESS] All Part 6A API test assertions passed successfully (exit code 0)." -ForegroundColor Green
exit 0

