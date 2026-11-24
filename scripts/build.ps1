# scripts/build.ps1
# Robust C11 Build Orchestrator for DUPLICATE-X (Team DataVault)

[CmdletBinding()]
param()

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X C11 Build Orchestrator" -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Resolve workspace root and absolute directories
$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$IncludeDir    = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/include")
$OutputDir     = [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/build")
$TargetExe     = [System.IO.Path]::GetFullPath("$OutputDir/duplicate-x.exe")

$SrcFiles = @(
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/main.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/engine.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/core/duplicate_detector.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/hashtable.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/duplicate_group.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/minheap.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/ds/trie.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_server.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_router.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/api/api_response.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_scanner.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_crypto.c"),
    [System.IO.Path]::GetFullPath("$WorkspaceRoot/backend/src/platform/windows/win_file_compare.c")
)

# 2. Locate GCC compiler (via Get-Command or verified Winget MinGW-w64 path)
$GccPath = $null
$GccCmd = Get-Command gcc -ErrorAction SilentlyContinue
if ($GccCmd -and $GccCmd.Source) {
    $GccPath = $GccCmd.Source
} else {
    $KnownWingetPath = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\gcc.exe"
    if (Test-Path $KnownWingetPath) {
        $GccPath = $KnownWingetPath
    } else {
        $WingetSearch = Get-ChildItem -Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter "gcc.exe" -Recurse -Depth 4 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($WingetSearch) {
            $GccPath = $WingetSearch.FullName
        }
    }
}

if (-not $GccPath -or -not (Test-Path $GccPath)) {
    Write-Host "`n[ERROR] GCC compiler could not be located in PATH or WinGet packages." -ForegroundColor Red
    Write-Host "Please install MinGW-w64 GCC:" -ForegroundColor Yellow
    Write-Host "  winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT" -ForegroundColor White
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}

# 3. Ensure GCC's bin directory is present in the current session's PATH
$GccBinDir = Split-Path -Parent $GccPath
if ($env:PATH -notlike "*$GccBinDir*") {
    $env:PATH = "$GccBinDir;$env:PATH"
    Write-Host "[INFO] Appended GCC toolchain bin to PATH: $GccBinDir" -ForegroundColor Gray
}

Write-Host "`n[INFO] Using GCC: $GccPath" -ForegroundColor Green
& "$GccPath" --version | Select-Object -First 1

# 4. Verify source files existence
foreach ($src in $SrcFiles) {
    if (-not (Test-Path $src)) {
        Write-Host "[ERROR] Missing source file: $src" -ForegroundColor Red
        Write-Host "BUILD FAILED" -ForegroundColor Red
        exit 1
    }
}

# 5. Ensure output directory exists
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# 6. Configure compiler arguments
$CompilerFlags = @(
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-municode",
    "-I$IncludeDir"
)

$LinkerFlags = @(
    "-lbcrypt",
    "-lws2_32"
)

Write-Host "[INFO] Compiling backend sources:" -ForegroundColor Cyan
foreach ($f in $SrcFiles) {
    Write-Host "  - $(Split-Path $f -Leaf)" -ForegroundColor Gray
}

# 7. Invoke GCC directly and capture output
Write-Host "[INFO] Executing compilation with BCrypt linking & Unicode entry point..." -ForegroundColor Cyan

$origEap = $ErrorActionPreference
$ErrorActionPreference = "Continue"

& "$GccPath" $CompilerFlags $SrcFiles -o "$TargetExe" $LinkerFlags
$compileExitCode = $LASTEXITCODE

$ErrorActionPreference = $origEap

Write-Host "[INFO] GCC Exit Code: $compileExitCode" -ForegroundColor Gray

# 8. Verify compilation results and executable validity
if ($compileExitCode -eq 0 -and (Test-Path $TargetExe)) {
    $exeItem = Get-Item $TargetExe
    if ($exeItem.Length -gt 0) {
        Write-Host "`n[SUCCESS] Compilation completed successfully." -ForegroundColor Green
        Write-Host "Executable generated: $TargetExe ($($exeItem.Length) bytes)" -ForegroundColor Green
        Write-Host "BUILD SUCCESS" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "`n[ERROR] Output binary was generated but has 0 bytes size." -ForegroundColor Red
        Write-Host "BUILD FAILED" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "`n[ERROR] GCC compilation failed with exit code $compileExitCode." -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}
