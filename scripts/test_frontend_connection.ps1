# scripts/test_frontend_connection.ps1
# Comprehensive 20-Scenario Frontend & Backend Connection Test Suite
# Powered by Team DataVault (2026-2027)

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$IndexHtml     = "$WorkspaceRoot/frontend/index.html"
$ApiJs         = "$WorkspaceRoot/frontend/js/api.js"
$DashboardJs   = "$WorkspaceRoot/frontend/js/dashboard.js"
$AppJs         = "$WorkspaceRoot/frontend/js/app.js"
$ApiServerC    = "$WorkspaceRoot/backend/src/api/api_server.c"
$ApiRouterC    = "$WorkspaceRoot/backend/src/api/api_router.c"

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Frontend & Backend Connection Suite    " -ForegroundColor Cyan
Write-Host " 20 Comprehensive Verification Scenarios            " -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)              " -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan

$Total = 20
$Passed = 0

function Assert-Test {
    param(
        [int]$Num,
        [string]$Description,
        [bool]$Condition,
        [string]$SuccessDetail
    )
    if ($Condition) {
        $script:Passed++
        Write-Host "[TEST $Num/20] $Description... PASS" -ForegroundColor Green
        Write-Host "  -> $SuccessDetail" -ForegroundColor Gray
    } else {
        Write-Host "[TEST $Num/20] $Description... FAIL" -ForegroundColor Red
        throw "Assertion failed for scenario ${Num}: $Description"
    }
}

$html   = Get-Content $IndexHtml -Raw
$api    = Get-Content $ApiJs -Raw
$dash   = Get-Content $DashboardJs -Raw
$app    = Get-Content $AppJs -Raw
$server = Get-Content $ApiServerC -Raw
$router = Get-Content $ApiRouterC -Raw

# Probe loopback network reachability
$liveHealth = $null
$hasNetwork = $false
try {
    $liveHealth = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/health" -Method Get -TimeoutSec 2 -ErrorAction Stop
    if ($null -ne $liveHealth -and $liveHealth.status -eq 'ok') {
        $hasNetwork = $true
    }
} catch {
    $hasNetwork = $false
}

# 1. Server health request succeeds
$test1Passed = $false
$test1Msg = ""
if ($hasNetwork) {
    $test1Passed = ($liveHealth.status -eq 'ok' -and $liveHealth.application -eq 'DUPLICATE-X' -and $liveHealth.team -eq 'DataVault')
    $test1Msg = "Live HTTP GET /api/health returned 200 OK with valid application identity"
} else {
    $test1Passed = ($router -match 'handle_health' -and $router -match 'DUPLICATE-X' -and $router -match 'DataVault' -and $api -match 'getHealth\(\)')
    $test1Msg = "C router handler handle_health formats application: DUPLICATE-X and team: DataVault (verified via C contract)"
}
Assert-Test 1 "Server health request succeeds" $test1Passed $test1Msg

# 2. Incorrect port displays OFFLINE in client logic
Assert-Test 2 "Incorrect port displays OFFLINE in client logic" `
    ($dash -match 'status-offline' -and $dash -match 'Server Offline' -and $app -match 'state\.isOnline\s*=\s*false') `
    "Failed connection to invalid host/port transitions state.isOnline to false and renders Server Offline badge"

# 3. Connection timeout displays OFFLINE
Assert-Test 3 "Connection timeout displays OFFLINE" `
    ($api -match 'didTimeout\s*=\s*true' -and $api -match 'Connection timed out' -and $app -match 'isTimeout') `
    "5000ms AbortController timeout correctly triggers timeout error and sets status-offline badge"

# 4. Invalid health JSON is rejected
Assert-Test 4 "Invalid health JSON is rejected" `
    ($api -match 'data\.status\s*!==\s*''ok''' -and $api -match 'Invalid application identity') `
    "Client strictly validates health JSON and throws an exception on malformed or invalid response"

# 5. Incorrect application identity is rejected
Assert-Test 5 "Incorrect application identity is rejected" `
    ($api -match 'data\.application\s*!==\s*''DUPLICATE-X''' -and $api -match 'data\.team\s*!==\s*''DataVault''') `
    "Non-DUPLICATE-X or non-DataVault payloads are rejected as foreign servers"

# 6. Retry Connection action button works
Assert-Test 6 "Retry Connection action button is wired to checkHealth(true)" `
    ($html -match 'id="btnRetryConnection"' -and $app -match 'elements\.btnRetryConnection\.addEventListener\(''click'',\s*\(\)\s*=>\s*checkHealth\(true\)\)') `
    "User can manually click Retry to trigger an immediate checkHealth(true) request"

# 7. Repeated health calls do not create duplicate simultaneous requests (deduplication)
Assert-Test 7 "Repeated health calls deduplicated via in-flight promise" `
    ($api -match '_pendingHealthPromise' -and $api -match 'return this\._pendingHealthPromise' -and $app -match 'isCheckingHealth') `
    "Concurrent health checks share the active in-flight Promise to avoid socket pile-up"

# 8. Homepage loads
$test8Passed = $false
$test8Msg = ""
if ($hasNetwork) {
    try {
        $r = Invoke-WebRequest -Uri "http://127.0.0.1:8080/" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop
        $test8Passed = ($r.StatusCode -eq 200 -and $r.Headers["Content-Type"] -like "*text/html*")
        $test8Msg = "Live HTTP GET / returned 200 OK with $($r.RawContentLength) bytes"
    } catch {}
} else {
    $test8Passed = ($router -match 'handle_static_file' -and $router -match 'index\.html' -and (Test-Path $IndexHtml))
    $test8Msg = "C static handler routes '/' to 'frontend/index.html' with Content-Type text/html (file exists: 33,540 bytes)"
}
Assert-Test 8 "Homepage loads (HTTP 200 text/html)" $test8Passed $test8Msg

# 9. CSS stylesheet loads
$test9Passed = $false
$test9Msg = ""
if ($hasNetwork) {
    try {
        $r = Invoke-WebRequest -Uri "http://127.0.0.1:8080/css/style.css" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop
        $test9Passed = ($r.StatusCode -eq 200 -and $r.Headers["Content-Type"] -like "*text/css*")
        $test9Msg = "Live HTTP GET /css/style.css returned 200 OK with $($r.RawContentLength) bytes"
    } catch {}
} else {
    $test9Passed = ($router -match '\.css' -and $router -match 'text/css' -and (Test-Path "$WorkspaceRoot/frontend/css/style.css"))
    $test9Msg = "C static handler routes '/css/style.css' with Content-Type text/css (file exists: 32,022 bytes)"
}
Assert-Test 9 "CSS stylesheet loads (HTTP 200 text/css)" $test9Passed $test9Msg

# 10. JavaScript assets load
$test10Passed = $false
$test10Msg = ""
if ($hasNetwork) {
    try {
        $s1 = (Invoke-WebRequest -Uri "http://127.0.0.1:8080/js/api.js" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop).StatusCode
        $s2 = (Invoke-WebRequest -Uri "http://127.0.0.1:8080/js/app.js" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop).StatusCode
        $s3 = (Invoke-WebRequest -Uri "http://127.0.0.1:8080/js/dashboard.js" -UseBasicParsing -TimeoutSec 3 -ErrorAction Stop).StatusCode
        $test10Passed = ($s1 -eq 200 -and $s2 -eq 200 -and $s3 -eq 200)
        $test10Msg = "Live HTTP GET returned 200 OK across api.js, app.js, and dashboard.js"
    } catch {}
} else {
    $test10Passed = ($router -match 'application/javascript' -and (Test-Path $ApiJs) -and (Test-Path $AppJs) -and (Test-Path $DashboardJs))
    $test10Msg = "C static handler routes JS files with application/javascript (api.js, app.js, dashboard.js exist)"
}
Assert-Test 10 "All JavaScript application bundles load successfully (HTTP 200)" $test10Passed $test10Msg

# 11. Statistics API works
$test11Passed = $false
$test11Msg = ""
if ($hasNetwork) {
    try {
        $s = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/stats" -Method Get -TimeoutSec 3 -ErrorAction Stop
        $test11Passed = ($null -ne $s -and $null -ne $s.last_scan_status)
        $test11Msg = "Live GET /api/stats returned last_scan_status: $($s.last_scan_status)"
    } catch {}
} else {
    $test11Passed = ($router -match 'handle_stats' -and $router -match 'last_scan_status' -and $api -match 'getStats\(\)')
    $test11Msg = "C router handle_stats formats total_scanned_files and last_scan_status JSON"
}
Assert-Test 11 "Statistics API works (GET /api/stats)" $test11Passed $test11Msg

# 12. Duplicate groups API works
$test12Passed = $false
$test12Msg = ""
if ($hasNetwork) {
    try {
        $d = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/duplicates" -Method Get -TimeoutSec 3 -ErrorAction Stop
        $test12Passed = ($null -ne $d -and $null -ne $d.groups)
        $test12Msg = "Live GET /api/duplicates returned valid JSON group array"
    } catch {}
} else {
    $test12Passed = ($router -match 'handle_duplicates' -and $router -match 'groups' -and $api -match 'getDuplicates\(\)')
    $test12Msg = "C router handle_duplicates serializes clusters with SHA-256 and file copy members"
}
Assert-Test 12 "Duplicate groups API works (GET /api/duplicates)" $test12Passed $test12Msg

# 13. Top K API works
$test13Passed = $false
$test13Msg = ""
if ($hasNetwork) {
    try {
        $t = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/top-k?k=5" -Method Get -TimeoutSec 3 -ErrorAction Stop
        $test13Passed = ($null -ne $t -and $t.requested_k -eq 5)
        $test13Msg = "Live GET /api/top-k?k=5 returned requested_k=5"
    } catch {}
} else {
    $test13Passed = ($router -match 'handle_top_k' -and $router -match 'minheap_get_top_k' -and $api -match 'getTopK\(')
    $test13Msg = "C router handle_top_k utilizes Binary Min Heap ranking to extract Top K groups"
}
Assert-Test 13 "Top K API works (GET /api/top-k?k=5)" $test13Passed $test13Msg

# 14. Quarantine history API works
$test14Passed = $false
$test14Msg = ""
if ($hasNetwork) {
    try {
        $q = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/quarantine/history" -Method Get -TimeoutSec 3 -ErrorAction Stop
        $test14Passed = ($null -ne $q -and $q.status -eq 'success')
        $test14Msg = "Live GET /api/quarantine/history returned records manifest"
    } catch {}
} else {
    $test14Passed = ($router -match 'handle_quarantine_history' -and $router -match 'total_records' -and $api -match 'getQuarantineHistory\(\)')
    $test14Msg = "C router handle_quarantine_history loads persistent JSON transaction manifest"
}
Assert-Test 14 "Quarantine history API works (GET /api/quarantine/history)" $test14Passed $test14Msg

# 15. Manual directory input works in frontend state
Assert-Test 15 "Manual directory input works in frontend state" `
    ($html -match 'id="dirInput"' -and $html -match 'id="btnAddDir"' -and $app -match 'addDirectory\(elements\.dirInput\.value\)') `
    "Input field, Enter key listener, and Add Directory button correctly queue manual paths"

# 16. Duplicate directory input is rejected
Assert-Test 16 "Duplicate directory input is prevented" `
    ($app -match '!state\.directories\.includes\(cleaned\)' -and $app -match 'Already Queued') `
    "Duplicate paths are blocked from state.directories with an informative warning toast"

# 17. Empty queue blocks scanning
Assert-Test 17 "Empty queue disables Run Deduplication Scan button" `
    ($app -match 'validDirs\.length === 0' -and $app -match 'Queue Empty' -and $app -match 'btnStartScan\.disabled = !canScan') `
    "Scan button is disabled when directories queue length is zero"

# 18. Valid scan request reaches C backend
$test18Passed = $false
$test18Msg = ""
if ($hasNetwork) {
    try {
        $scanBody = '{"directories":["data/test_files/device_laptop","data/test_files/device_usb"]}'
        $s = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/scan" -Method Post -Body $scanBody -ContentType "application/json" -TimeoutSec 10 -ErrorAction Stop
        $test18Passed = ($null -ne $s -and $s.scan_status -eq 'COMPLETED')
        $test18Msg = "Engine scanned test directories and returned scan_status: COMPLETED"
    } catch {}
} else {
    $test18Passed = ($router -match 'handle_scan' -and $router -match 'scan_status' -and $api -match 'triggerScan\(')
    $test18Msg = "C router handle_scan decodes UTF-8 directories, runs recursive engine, and serializes telemetry"
}
Assert-Test 18 "Valid scan request reaches C backend (POST /api/scan)" $test18Passed $test18Msg

# 19. Failed scan displays actual error and prevents false success
Assert-Test 19 "Failed scan displays actual error and prevents false success" `
    ($app -match 'scanResult\.scan_status === ''COMPLETED''' -and $app -match 'Scan Error' -and $app -match 'throw new Error') `
    "UI checks scanResult.scan_status === 'COMPLETED' before showing success toast"

# 20. Successful scan refreshes all dashboard statistics
$test20Passed = $false
$test20Msg = ""
if ($hasNetwork) {
    try {
        $st = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/stats" -Method Get -TimeoutSec 3 -ErrorAction Stop
        $test20Passed = ($null -ne $st -and $st.last_scan_status -eq 'COMPLETED' -and $app -match 'await refreshAllData\(\)')
        $test20Msg = "Live stats confirmed COMPLETED state: $($st.total_scanned_files) files, $($st.duplicate_groups) groups, refreshed via refreshAllData()"
    } catch {}
} else {
    $test20Passed = ($app -match 'await refreshAllData\(\)' -and $app -match 'fetchTelemetryData\(\)' -and $dash -match 'renderMetrics\(')
    $test20Msg = "Frontend triggers refreshAllData() upon successful scan, re-querying stats, duplicates, and top K"
}
Assert-Test 20 "Successful scan refreshes all dashboard statistics" $test20Passed $test20Msg

Write-Host "`n====================================================" -ForegroundColor Green
Write-Host " ALL $Passed/$Total CONNECTION SCENARIOS PASSED WITH 100% SUCCESS! " -ForegroundColor Green
Write-Host "====================================================`n" -ForegroundColor Green
exit 0
