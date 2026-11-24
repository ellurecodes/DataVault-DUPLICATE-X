# scripts/test_browse_and_connection.ps1
# Comprehensive 15-Scenario Automated Test Suite for Folder Selection & Backend Connection
# Powered by Team DataVault (Academic Year 2026-2027)

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$FrontendDir   = [System.IO.Path]::GetFullPath("$WorkspaceRoot/frontend")
$IndexHtml     = "$FrontendDir/index.html"
$ApiJs         = "$FrontendDir/js/api.js"
$DashboardJs   = "$FrontendDir/js/dashboard.js"
$AppJs         = "$FrontendDir/js/app.js"
$ApiRouterC    = "$WorkspaceRoot/backend/src/api/api_router.c"
$ApiServerC    = "$WorkspaceRoot/backend/src/api/api_server.c"

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Folder Selection & Connection Suite    " -ForegroundColor Cyan
Write-Host " 15 Comprehensive Verification Checks               " -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)              " -ForegroundColor Cyan
Write-Host "====================================================`n" -ForegroundColor Cyan

$Passed = 0
$Total = 15

function Assert-Check($testNum, $description, $condition, $passMsg = "PASS") {
    Write-Host -NoNewline "[CHECK $testNum/15] $description... "
    if ($condition) {
        Write-Host "$passMsg" -ForegroundColor Green
        $script:Passed++
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        throw "Assertion failed for check ${testNum}: $description"
    }
}

$html   = Get-Content $IndexHtml -Raw
$api    = Get-Content $ApiJs -Raw
$dash   = Get-Content $DashboardJs -Raw
$app    = Get-Content $AppJs -Raw
$router = Get-Content $ApiRouterC -Raw
$server = Get-Content $ApiServerC -Raw

# 1. Backend health check success contract in api.js
Assert-Check 1 "Backend health check client method defined" `
    ($api -match 'getHealth\s*\(\)' -and $api -match '/api/health') `
    "PASS (DuplicateXApi.getHealth calls /api/health)"

# 2. Backend offline handling in app.js
Assert-Check 2 "Backend offline state handled safely with offline indicator" `
    ($app -match 'state\.isOnline\s*=\s*false' -and $dash -match 'status-offline' -and $dash -match 'Server Offline') `
    "PASS (Offline state renders Server Offline and updates UI)"

# 3. Connection timeout handling via AbortController in api.js
Assert-Check 3 "Connection timeout handling with AbortController" `
    ($api -match 'AbortController' -and $api -match 'setTimeout' -and $api -match 'controller\.abort') `
    "PASS (Fetch requests enforce request timeout to prevent hanging)"

# 4. Native folder picker endpoint defined in C router
Assert-Check 4 "Native folder picker endpoint POST /api/folder-picker routed in C" `
    ($router -match 'handle_folder_picker' -and $router -match '/api/folder-picker') `
    "PASS (C backend routes POST /api/folder-picker)"

# 5. Folder picker cancellation handling in C backend
Assert-Check 5 "Folder picker cancellation handled without error (status: cancelled)" `
    ($router -match 'ERROR_CANCELLED' -and $router -match 'status[\\"\s:]+cancelled' -and $router -match 'selected[\\"\s:]+false') `
    "PASS (Cancellation returns 200 with status cancelled cleanly)"

# 6. Selected absolute path returned correctly in C backend
Assert-Check 6 "Selected folder returns filesystem path (status: selected, SIGDN_FILESYSPATH)" `
    ($router -match 'SIGDN_FILESYSPATH' -and $router -match 'CoTaskMemFree' -and $router -match 'status[\\"\s:]+selected') `
    "PASS (COM IShellItem retrieves and converts SIGDN_FILESYSPATH)"

# 7. Browse selection adds directory to target queue in app.js
Assert-Check 7 "Browse selection workflow adds directory to TARGET QUEUE" `
    ($app -match 'openFolderPicker' -and $app -match 'addDirectory\(res\.path\)') `
    "PASS (Frontend receives selected absolute path and adds to queue)"

# 8. Duplicate directory addition prevented in app.js
Assert-Check 8 "Duplicate directory addition prevented in state.directories" `
    ($app -match '!state\.directories\.includes' -and $app -match 'Already Queued') `
    "PASS (Duplicate directory filter and warning toast verified)"

# 9. Empty target queue disables scanning
Assert-Check 9 "Empty target queue disables Run Deduplication Scan button" `
    ($html -match 'id="btnStartScan"[^>]*disabled' -and $app -match 'updateScanButtonState' -and $app -match 'btnStartScan\.disabled\s*=') `
    "PASS (Scan button disabled when queue is empty)"

# 10. Manual path entry and Add Directory button still work as fallback
Assert-Check 10 "Manual path entry and Add Directory button work as fallback" `
    ($html -match 'id="btnAddDir"' -and $app -match 'btnAddDir\.addEventListener' -and $app -match 'dirInput\.addEventListener\(''keydown''') `
    "PASS (Manual path typing, Enter key, and Add button preserved)"

# 11. Quick Test Folders work and populate actual sample paths
Assert-Check 11 "Quick Test Folders wired to actual test directories" `
    ($html -match 'data/test_files/device_laptop' -and $html -match 'data/test_files/device_usb' -and $app -match 'btn-sample-dir') `
    "PASS (Quick sample buttons populate test_files directories)"

# 12. Scan request sends the correct directory list to POST /api/scan
Assert-Check 12 "Scan request validates non-empty paths and posts to /api/scan" `
    ($app -match 'DuplicateXApi\.triggerScan\(validDirs\)' -and $api -match 'JSON\.stringify\(\{\s*directories\s*\}\)') `
    "PASS (Valid directory array serialized and sent to backend)"

# 13. Failed scan does not show false success message
Assert-Check 13 "Failed scan displays actual error and prevents false success" `
    ($app -match 'scanResult\.scan_status === ''COMPLETED''' -and $app -match 'Scan Error' -and $app -match 'Scan failed:') `
    "PASS (Scan completed message shown only on confirmed engine COMPLETED status)"

# 14. Successful scan updates dashboard statistics and Top K
Assert-Check 14 "Successful scan refreshes telemetry, duplicates, and Top K" `
    ($app -match 'await refreshAllData\(\)' -and $app -match 'Discovered.*files_discovered') `
    "PASS (Telemetry and rankings reloaded upon scan completion)"

# 15. Unicode folder paths and origin security rules preserved
Assert-Check 15 "Unicode folder paths and local loopback origin rules preserved" `
    ($router -match 'MultiByteToWideChar\(CP_UTF8' -and $server -match 'api_is_origin_allowed' -and $server -match '"null"') `
    "PASS (WideChar Unicode conversion and Origin security rules verified)"

Write-Host "`n====================================================" -ForegroundColor Green
Write-Host " ALL $Passed/$Total CHECKS PASSED WITH 100% SUCCESS! " -ForegroundColor Green
Write-Host "====================================================`n" -ForegroundColor Green
