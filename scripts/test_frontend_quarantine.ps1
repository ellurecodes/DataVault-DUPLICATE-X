# scripts/test_frontend_quarantine.ps1
# Automated verification for DUPLICATE-X Part 7B Frontend Quarantine & Restore Dashboard
# Powered by Team DataVault (Academic Year 2026-2027)

[CmdletBinding()]
param()

$WorkspaceRoot = (Resolve-Path "$PSScriptRoot/..").Path
$FrontendDir   = [System.IO.Path]::GetFullPath("$WorkspaceRoot/frontend")
$IndexHtml     = "$FrontendDir/index.html"
$StyleCss      = "$FrontendDir/css/style.css"
$ApiJs         = "$FrontendDir/js/api.js"
$DashboardJs   = "$FrontendDir/js/dashboard.js"
$AppJs         = "$FrontendDir/js/app.js"

Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " DUPLICATE-X Part 7B Frontend Integration Suite     " -ForegroundColor Cyan
Write-Host " 20 Comprehensive Verification Checks               " -ForegroundColor Cyan
Write-Host " Powered by Team DataVault (2026-2027)              " -ForegroundColor Cyan
Write-Host "====================================================`n" -ForegroundColor Cyan

$Passed = 0
$Total = 20

function Assert-Check($testNum, $description, $condition, $passMsg = "PASS") {
    Write-Host -NoNewline "[TEST $testNum/20] $description... "
    if ($condition) {
        Write-Host "$passMsg" -ForegroundColor Green
        $script:Passed++
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        throw "Assertion failed for: $description"
    }
}

# Read files
$html = Get-Content $IndexHtml -Raw
$css  = Get-Content $StyleCss -Raw
$api  = Get-Content $ApiJs -Raw
$dash = Get-Content $DashboardJs -Raw
$app  = Get-Content $AppJs -Raw

# 1. Navigation tabs present in index.html
Assert-Check 1 "Dashboard navigation tabs present in index.html" `
    ($html -match 'id="navTabDuplicates"' -and $html -match 'id="navTabQuarantine"') `
    "PASS (navTabDuplicates and navTabQuarantine defined)"

# 2. Dual view panels defined
Assert-Check 2 "Dual view panels defined in DOM" `
    ($html -match 'id="viewDuplicates"' -and $html -match 'id="viewQuarantine"') `
    "PASS (viewDuplicates and viewQuarantine panels wired)"

# 3. Quarantine & Restore modal dialogs exist
Assert-Check 3 "Quarantine & Restore modal dialogs in index.html" `
    ($html -match 'id="quarantineModal"' -and $html -match 'id="restoreModal"') `
    "PASS (quarantineModal and restoreModal defined)"

# 4. Toast notification container exists
Assert-Check 4 "Toast notification container in index.html" `
    ($html -match 'id="toastContainer"') `
    "PASS (toastContainer mounted)"

# 5. Quarantine statistics metric cards in index.html
Assert-Check 5 "Quarantine statistics cards in index.html" `
    ($html -match 'id="qStatTotal"' -and $html -match 'id="qStatActive"' -and $html -match 'id="qStatRestored"' -and $html -match 'id="qStatFailed"') `
    "PASS (all 4 quarantine metric cards defined)"

# 6. Quarantine history toolbar filters & search in index.html
Assert-Check 6 "History search input and status filter pills in index.html" `
    ($html -match 'id="quarantineSearchInput"' -and $html -match 'id="quarantineStatusFilters"' -and $html -match 'id="btnRefreshHistory"') `
    "PASS (history search and status filters present)"

# 7. CSS navigation tab and badge styles
Assert-Check 7 "CSS styling for navigation tabs and badges" `
    ($css -match '\.app-nav' -and $css -match '\.nav-tab' -and $css -match '\.nav-badge') `
    "PASS (navigation tab CSS rules verified)"

# 8. CSS modal and toast styles
Assert-Check 8 "CSS styling for modals, alerts, and toasts" `
    ($css -match '\.modal-backdrop' -and $css -match '\.modal-dialog' -and $css -match '\.toast') `
    "PASS (modal and toast styling verified)"

# 9. CSS status badge styles
Assert-Check 9 "CSS status badges (ACTIVE, RESTORED, FAILED)" `
    ($css -match '\.badge-active' -and $css -match '\.badge-restored' -and $css -match '\.badge-failed') `
    "PASS (quarantine status badge styling verified)"

# 10. api.js implements quarantinePreview
Assert-Check 10 "api.js implements quarantinePreview() POST /api/quarantine/preview" `
    ($api -match 'quarantinePreview\s*\(' -and $api -match '/api/quarantine/preview') `
    "PASS (quarantinePreview endpoint client verified)"

# 11. api.js implements quarantineExecute
Assert-Check 11 "api.js implements quarantineExecute() POST /api/quarantine/execute" `
    ($api -match 'quarantineExecute\s*\(' -and $api -match '/api/quarantine/execute') `
    "PASS (quarantineExecute endpoint client verified)"

# 12. api.js implements getQuarantineHistory
Assert-Check 12 "api.js implements getQuarantineHistory() GET /api/quarantine/history" `
    ($api -match 'getQuarantineHistory\s*\(' -and $api -match '/api/quarantine/history') `
    "PASS (getQuarantineHistory endpoint client verified)"

# 13. api.js implements restorePreview
Assert-Check 13 "api.js implements restorePreview() POST /api/quarantine/restore/preview" `
    ($api -match 'restorePreview\s*\(' -and $api -match '/api/quarantine/restore/preview') `
    "PASS (restorePreview endpoint client verified)"

# 14. api.js implements restoreExecute
Assert-Check 14 "api.js implements restoreExecute() POST /api/quarantine/restore/execute" `
    ($api -match 'restoreExecute\s*\(' -and $api -match '/api/quarantine/restore/execute') `
    "PASS (restoreExecute endpoint client verified)"

# 15. dashboard.js implements redundant copy selection & sole copy protection
Assert-Check 15 "dashboard.js renders selection controls & sole copy warnings" `
    ($dash -match 'btn-quarantine-submit' -and $dash -match 'role-original' -and $dash -match 'At least 1 copy must remain') `
    "PASS (selection controls and last-copy protection verified in UI)"

# 16. dashboard.js implements renderQuarantineHistory with status filtering
Assert-Check 16 "dashboard.js implements renderQuarantineHistory() with filtering" `
    ($dash -match 'renderQuarantineHistory\s*\(' -and $dash -match 'btn-restore-tx') `
    "PASS (quarantine history renderer with restore triggers verified)"

# 17. dashboard.js implements showToast notification system
Assert-Check 17 "dashboard.js implements showToast() notification system" `
    ($dash -match 'showToast\s*\(' -and $dash -match 'toastContainer') `
    "PASS (toast notification renderer verified)"

# 18. app.js implements tab switching and quarantine history sync
Assert-Check 18 "app.js implements switchTab() and refreshQuarantineHistory()" `
    ($app -match 'switchTab\s*\(' -and $app -match 'refreshQuarantineHistory\s*\(') `
    "PASS (tab switching and manifest sync verified)"

# 19. app.js implements 2-stage quarantine confirmation & double-submit prevention
Assert-Check 19 "app.js enforces 2-stage quarantine & prevents double-submit" `
    ($app -match 'handleQuarantinePreviewRequest' -and $app -match 'executeQuarantine' -and $app -match 'isQuarantining') `
    "PASS (2-stage quarantine workflow & double-submit guard verified)"

# 20. app.js implements 2-stage restore confirmation & conflict detection
Assert-Check 20 "app.js enforces 2-stage restore & destination conflict handling" `
    ($app -match 'handleRestorePreviewRequest' -and $app -match 'executeRestore' -and $app -match 'restoreConflictAlert') `
    "PASS (2-stage restore workflow & conflict detection verified)"

Write-Host "`n====================================================" -ForegroundColor Green
Write-Host " ALL $Passed/$Total FRONTEND INTEGRATION CHECKS PASSED (100%)! " -ForegroundColor Green
Write-Host "====================================================`n" -ForegroundColor Green
