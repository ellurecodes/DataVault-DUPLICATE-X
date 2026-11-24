/**
 * @file app.js
 * @brief Application lifecycle controller, state management, and event orchestration.
 * @author DataVault Team (Academic Year 2026-2027)
 */

document.addEventListener('DOMContentLoaded', () => {
    // Application State
    const state = {
        isOnline: false,
        activeTab: 'duplicates', // 'duplicates' | 'quarantine'
        directories: [],
        stats: null,
        topKGroups: [],
        duplicateGroups: [],
        quarantineRecords: [],
        quarantineFilter: 'ALL',
        quarantineSearch: '',
        isScanning: false,
        isQuarantining: false,
        isRestoring: false,
        selectedK: 5,
        activeQuarantinePreview: null,
        activeRestorePreview: null,
        quarantineTimerId: null
    };

    // DOM Elements
    const elements = {
        btnRefresh: document.getElementById('btnRefresh'),
        navTabDuplicates: document.getElementById('navTabDuplicates'),
        navTabQuarantine: document.getElementById('navTabQuarantine'),
        navQuarantineCount: document.getElementById('navQuarantineCount'),
        viewDuplicates: document.getElementById('viewDuplicates'),
        viewQuarantine: document.getElementById('viewQuarantine'),

        dirInput: document.getElementById('dirInput'),
        btnAddDir: document.getElementById('btnAddDir'),
        btnClearAllDirs: document.getElementById('btnClearAllDirs'),
        btnStartScan: document.getElementById('btnStartScan'),
        scanBtnText: document.getElementById('scanBtnText'),
        scanStatusIndicator: document.getElementById('scanStatusIndicator'),
        scanStatusMsg: document.getElementById('scanStatusMsg'),
        topKSelect: document.getElementById('topKSelect'),
        duplicateFilterInput: document.getElementById('duplicateFilterInput'),
        prefixInput: document.getElementById('prefixInput'),
        btnSearchPrefix: document.getElementById('btnSearchPrefix'),

        // Quarantine History Elements
        quarantineSearchInput: document.getElementById('quarantineSearchInput'),
        quarantineStatusFilters: document.getElementById('quarantineStatusFilters'),
        btnRefreshHistory: document.getElementById('btnRefreshHistory'),

        // Quarantine Modal Elements
        quarantineModal: document.getElementById('quarantineModal'),
        btnCloseQuarantineModal: document.getElementById('btnCloseQuarantineModal'),
        btnCancelQuarantine: document.getElementById('btnCancelQuarantine'),
        btnConfirmQuarantine: document.getElementById('btnConfirmQuarantine'),
        btnConfirmQuarantineText: document.getElementById('btnConfirmQuarantineText'),
        qModalGroupId: document.getElementById('qModalGroupId'),
        qModalRemaining: document.getElementById('qModalRemaining'),
        qModalRecoverable: document.getElementById('qModalRecoverable'),
        qModalExpiry: document.getElementById('qModalExpiry'),
        qModalFileCount: document.getElementById('qModalFileCount'),
        qModalFileList: document.getElementById('qModalFileList'),
        qModalError: document.getElementById('qModalError'),

        // Restore Modal Elements
        restoreModal: document.getElementById('restoreModal'),
        btnCloseRestoreModal: document.getElementById('btnCloseRestoreModal'),
        btnCancelRestore: document.getElementById('btnCancelRestore'),
        btnConfirmRestore: document.getElementById('btnConfirmRestore'),
        btnConfirmRestoreText: document.getElementById('btnConfirmRestoreText'),
        restoreConflictAlert: document.getElementById('restoreConflictAlert'),
        restoreConflictMsg: document.getElementById('restoreConflictMsg'),
        restoreInfoAlert: document.getElementById('restoreInfoAlert'),
        rModalTxId: document.getElementById('rModalTxId'),
        rModalFileSize: document.getElementById('rModalFileSize'),
        rModalOrigPath: document.getElementById('rModalOrigPath'),
        rModalError: document.getElementById('rModalError')
    };

    /**
     * Switch view tabs between Duplicate Management and Quarantine & Recovery
     */
    function switchTab(tabName) {
        state.activeTab = tabName;
        if (tabName === 'duplicates') {
            if (elements.navTabDuplicates) elements.navTabDuplicates.classList.add('active');
            if (elements.navTabQuarantine) elements.navTabQuarantine.classList.remove('active');
            if (elements.viewDuplicates) elements.viewDuplicates.style.display = 'block';
            if (elements.viewQuarantine) elements.viewQuarantine.style.display = 'none';
        } else {
            if (elements.navTabDuplicates) elements.navTabDuplicates.classList.remove('active');
            if (elements.navTabQuarantine) elements.navTabQuarantine.classList.add('active');
            if (elements.viewDuplicates) elements.viewDuplicates.style.display = 'none';
            if (elements.viewQuarantine) elements.viewQuarantine.style.display = 'block';
            refreshQuarantineHistory();
        }
    }

    /**
     * Check backend health and sync state
     */
    async function checkHealth() {
        try {
            const health = await DuplicateXApi.getHealth();
            state.isOnline = (health && health.status === 'ok');
            DashboardUI.renderConnectionStatus(state.isOnline, '127.0.0.1:8080');
        } catch (err) {
            state.isOnline = false;
            DashboardUI.renderConnectionStatus(false);
        }
    }

    /**
     * Refresh all telemetry and data from C backend
     */
    async function refreshAllData() {
        DashboardUI.renderMetricsLoading();
        await checkHealth();
        if (!state.isOnline) {
            DashboardUI.renderMetricsError('Server offline (127.0.0.1:8080)');
            return;
        }

        try {
            // Fetch fresh stats from GET /api/stats
            const stats = await DuplicateXApi.getStats();
            state.stats = stats;
            DashboardUI.renderMetrics(stats);

            // Also refresh quarantine history stats in background
            refreshQuarantineHistory(false);

            // Check if scan has completed or if there are duplicate groups / scanned files
            const hasCompleted = stats && (
                stats.last_scan_status === 'COMPLETED' ||
                (stats.total_scanned_files !== undefined && stats.total_scanned_files > 0) ||
                (stats.duplicate_groups !== undefined && stats.duplicate_groups > 0)
            );

            if (hasCompleted) {
                // Fetch fresh duplicate groups from GET /api/duplicates
                const dupData = await DuplicateXApi.getDuplicates();
                state.duplicateGroups = dupData.groups || [];
                DashboardUI.renderDuplicates(
                    state.duplicateGroups, 
                    elements.duplicateFilterInput ? elements.duplicateFilterInput.value : '',
                    handleQuarantinePreviewRequest
                );

                // Fetch fresh Top K from GET /api/top-k?k=...
                const topKData = await DuplicateXApi.getTopK(state.selectedK);
                state.topKGroups = topKData.groups || [];
                DashboardUI.renderTopK(state.topKGroups);
            } else {
                state.duplicateGroups = [];
                state.topKGroups = [];
                DashboardUI.renderDuplicates([], '', handleQuarantinePreviewRequest);
                DashboardUI.renderTopK([]);
            }
        } catch (err) {
            console.error('[APP] Error refreshing dashboard data:', err);
            DashboardUI.renderMetricsError(err.message || 'Failed to fetch statistics');
        }
    }

    /**
     * Fetch quarantine transaction history manifest from C backend
     */
    async function refreshQuarantineHistory(showLoading = true) {
        const container = document.getElementById('quarantineTableContainer');
        if (showLoading && container) {
            container.innerHTML = '<div class="empty-state">Loading quarantine records from server...</div>';
        }

        try {
            const data = await DuplicateXApi.getQuarantineHistory();
            state.quarantineRecords = data.records || [];
            DashboardUI.renderQuarantineHistory(
                state.quarantineRecords,
                state.quarantineFilter,
                state.quarantineSearch,
                handleRestorePreviewRequest
            );
        } catch (err) {
            console.error('[APP] Error fetching quarantine history:', err);
            if (container) {
                container.innerHTML = `<div class="empty-state" style="color: var(--accent-red);">Failed to load quarantine history: ${DashboardUI.escapeHtml(err.message)}</div>`;
            }
        }
    }

    /**
     * STAGE 1: Request Quarantine Preview for selected files
     */
    async function handleQuarantinePreviewRequest(groupId, fileIds, selectedFiles, group) {
        if (!fileIds || fileIds.length === 0) {
            DashboardUI.showToast('Selection Required', 'Please select at least one duplicate file to quarantine.', 'warning');
            return;
        }

        try {
            const preview = await DuplicateXApi.quarantinePreview(groupId, fileIds);
            state.activeQuarantinePreview = preview;

            // Populate Modal
            if (elements.qModalGroupId) elements.qModalGroupId.textContent = `GRP #${preview.group_id}`;
            if (elements.qModalRemaining) {
                elements.qModalRemaining.textContent = `${preview.remaining_copies} valid copy(ies) remain safe`;
            }
            if (elements.qModalRecoverable) {
                elements.qModalRecoverable.textContent = DashboardUI.formatBytes(preview.estimated_recoverable_bytes);
            }
            if (elements.qModalFileCount) {
                elements.qModalFileCount.textContent = preview.file_count;
            }

            // Populate files list safely
            if (elements.qModalFileList) {
                let html = '<table class="file-table" style="margin: 0;"><tbody>';
                const filesToShow = preview.files || selectedFiles || [];
                filesToShow.forEach(f => {
                    html += `
                        <tr>
                            <td>
                                <div style="font-weight: 600;">${DashboardUI.escapeHtml(f.filename || 'File')}</div>
                                <div class="file-path-cell">${DashboardUI.escapeHtml(f.filepath || '')}</div>
                            </td>
                            <td style="text-align: right; font-weight: 600; width: 90px;">
                                ${DashboardUI.formatBytes(f.file_size)}
                            </td>
                        </tr>
                    `;
                });
                html += '</tbody></table>';
                elements.qModalFileList.innerHTML = html;
            }

            // Expiry Countdown Timer (300 seconds)
            if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
            let secondsLeft = preview.expires_in_seconds || 300;
            updateExpiryDisplay(secondsLeft);
            state.quarantineTimerId = setInterval(() => {
                secondsLeft--;
                if (secondsLeft <= 0) {
                    clearInterval(state.quarantineTimerId);
                    if (elements.qModalExpiry) elements.qModalExpiry.textContent = 'Expired (re-selection required)';
                    if (elements.btnConfirmQuarantine) elements.btnConfirmQuarantine.disabled = true;
                } else {
                    updateExpiryDisplay(secondsLeft);
                }
            }, 1000);

            function updateExpiryDisplay(secs) {
                const m = Math.floor(secs / 60);
                const s = secs % 60;
                if (elements.qModalExpiry) {
                    elements.qModalExpiry.textContent = `${m}:${s < 10 ? '0' : ''}${s} remaining`;
                }
            }

            // Reset error & buttons
            if (elements.qModalError) elements.qModalError.style.display = 'none';
            if (elements.btnConfirmQuarantine) {
                elements.btnConfirmQuarantine.disabled = false;
                elements.btnConfirmQuarantineText.textContent = 'Confirm & Quarantine';
            }

            // Show Modal
            if (elements.quarantineModal) elements.quarantineModal.style.display = 'flex';

        } catch (err) {
            console.error('[APP] Quarantine preview rejected:', err);
            DashboardUI.showToast('Quarantine Rejected', err.message || 'Operation rejected by engine safety checks', 'error');
        }
    }

    /**
     * STAGE 2: Execute Confirmed Quarantine Relocation
     */
    async function executeQuarantine() {
        if (!state.activeQuarantinePreview || !state.activeQuarantinePreview.preview_id) {
            DashboardUI.showToast('Invalid Preview', 'No valid quarantine preview found. Please reselect.', 'error');
            return;
        }

        if (state.isQuarantining) return;
        state.isQuarantining = true;

        if (elements.btnConfirmQuarantine) elements.btnConfirmQuarantine.disabled = true;
        if (elements.btnConfirmQuarantineText) elements.btnConfirmQuarantineText.textContent = 'Relocating to Vault...';
        if (elements.qModalError) elements.qModalError.style.display = 'none';

        try {
            const res = await DuplicateXApi.quarantineExecute(state.activeQuarantinePreview.preview_id, 'QUARANTINE');

            // Success: Clean up timer and close modal
            if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
            state.activeQuarantinePreview = null;
            if (elements.quarantineModal) elements.quarantineModal.style.display = 'none';

            DashboardUI.showToast(
                'Quarantine Successful',
                `Successfully relocated ${res.quarantined_count || 1} file(s) into .datavault_quarantine vault.`,
                'success'
            );

            // Refresh UI
            await refreshAllData();
            await refreshQuarantineHistory(false);

        } catch (err) {
            console.error('[APP] Quarantine execution failed:', err);
            if (elements.qModalError) {
                elements.qModalError.textContent = `Quarantine failed: ${err.message || 'Engine execution error'}`;
                elements.qModalError.style.display = 'flex';
            }
            if (elements.btnConfirmQuarantine) elements.btnConfirmQuarantine.disabled = false;
            if (elements.btnConfirmQuarantineText) elements.btnConfirmQuarantineText.textContent = 'Retry Quarantine';
        } finally {
            state.isQuarantining = false;
        }
    }

    /**
     * STAGE 1: Request Restore Preview for a transaction
     */
    async function handleRestorePreviewRequest(transactionId) {
        if (!transactionId) return;

        try {
            const preview = await DuplicateXApi.restorePreview(transactionId);
            state.activeRestorePreview = preview;

            // Populate Restore Modal
            if (elements.rModalTxId) elements.rModalTxId.textContent = preview.transaction_id || transactionId;
            if (elements.rModalFileSize) elements.rModalFileSize.textContent = DashboardUI.formatBytes(preview.file_size);
            if (elements.rModalOrigPath) elements.rModalOrigPath.textContent = preview.original_path || '--';

            // Clean state
            if (elements.restoreConflictAlert) elements.restoreConflictAlert.style.display = 'none';
            if (elements.restoreInfoAlert) elements.restoreInfoAlert.style.display = 'flex';
            if (elements.rModalError) elements.rModalError.style.display = 'none';
            if (elements.btnConfirmRestore) {
                elements.btnConfirmRestore.disabled = false;
                elements.btnConfirmRestoreText.textContent = 'Confirm & Restore';
            }

            if (elements.restoreModal) elements.restoreModal.style.display = 'flex';

        } catch (err) {
            console.error('[APP] Restore preview error:', err);

            // Handle destination conflict (HTTP 409) or tamper errors
            const isConflict = err.status === 409 || (err.message && err.message.toLowerCase().includes('already exists'));
            const isTampered = (err.message && err.message.toLowerCase().includes('integrity'));

            if (isConflict || isTampered) {
                if (elements.rModalTxId) elements.rModalTxId.textContent = transactionId;
                if (elements.rModalOrigPath) elements.rModalOrigPath.textContent = 'Destination Conflict / Tamper Guard';
                if (elements.restoreInfoAlert) elements.restoreInfoAlert.style.display = 'none';
                if (elements.restoreConflictAlert) {
                    elements.restoreConflictAlert.style.display = 'flex';
                    if (elements.restoreConflictMsg) {
                        elements.restoreConflictMsg.textContent = err.message;
                    }
                }
                if (elements.btnConfirmRestore) elements.btnConfirmRestore.disabled = true;
                if (elements.restoreModal) elements.restoreModal.style.display = 'flex';
            } else {
                DashboardUI.showToast('Restore Preview Failed', err.message || 'Failed to initialize restore preview', 'error');
            }
        }
    }

    /**
     * STAGE 2: Execute Confirmed Restoration
     */
    async function executeRestore() {
        if (!state.activeRestorePreview || !state.activeRestorePreview.preview_id) {
            DashboardUI.showToast('Invalid Preview', 'No valid restore preview token found.', 'error');
            return;
        }

        if (state.isRestoring) return;
        state.isRestoring = true;

        if (elements.btnConfirmRestore) elements.btnConfirmRestore.disabled = true;
        if (elements.btnConfirmRestoreText) elements.btnConfirmRestoreText.textContent = 'Restoring File...';
        if (elements.rModalError) elements.rModalError.style.display = 'none';

        try {
            const res = await DuplicateXApi.restoreExecute(state.activeRestorePreview.preview_id, 'RESTORE');

            state.activeRestorePreview = null;
            if (elements.restoreModal) elements.restoreModal.style.display = 'none';

            DashboardUI.showToast(
                'File Restored Successfully',
                `Restored back to ${res.restored_path || 'original path'}.`,
                'success'
            );

            // Refresh quarantine history and metrics
            await refreshQuarantineHistory(false);
            await refreshAllData();

        } catch (err) {
            console.error('[APP] Restore execution failed:', err);
            if (elements.rModalError) {
                elements.rModalError.textContent = `Restore failed: ${err.message || 'Engine execution error'}`;
                elements.rModalError.style.display = 'flex';
            }
            if (elements.btnConfirmRestore) elements.btnConfirmRestore.disabled = false;
            if (elements.btnConfirmRestoreText) elements.btnConfirmRestoreText.textContent = 'Retry Restore';
        } finally {
            state.isRestoring = false;
        }
    }

    /**
     * Add directory to queue
     */
    function addDirectory(dirPath) {
        if (!dirPath) return;
        const trimmed = dirPath.trim();
        if (!trimmed) return;

        // Prevent duplicate directory entries
        if (!state.directories.includes(trimmed)) {
            state.directories.push(trimmed);
            DashboardUI.renderConfiguredDirs(state.directories, removeDirectory);
        }
        elements.dirInput.value = '';
        elements.dirInput.focus();
    }

    /**
     * Remove directory from queue
     */
    function removeDirectory(index) {
        if (index >= 0 && index < state.directories.length) {
            state.directories.splice(index, 1);
            DashboardUI.renderConfiguredDirs(state.directories, removeDirectory);
        }
    }

    /**
     * Execute full filesystem deduplication scan via C REST API
     */
    async function startScan() {
        if (state.isScanning) return;
        if (state.directories.length === 0) {
            DashboardUI.showToast('Queue Empty', 'Please enter at least one directory path before scanning.', 'warning');
            elements.dirInput.focus();
            return;
        }

        state.isScanning = true;
        elements.btnStartScan.disabled = true;
        elements.scanBtnText.textContent = 'Scanning Storage...';
        elements.scanStatusIndicator.style.display = 'flex';
        elements.scanStatusMsg.textContent = 'Traversing directories, computing SHA-256 signatures, and detecting duplicates...';

        try {
            const scanResult = await DuplicateXApi.triggerScan(state.directories);
            elements.scanStatusMsg.textContent = 'Scan completed successfully! Updating dashboard...';
            DashboardUI.showToast('Scan Completed', `Discovered ${scanResult.files_discovered || 0} files and ${scanResult.duplicate_groups || 0} duplicate groups.`, 'success');

            // Reload all telemetry, top K, duplicates, and quarantine history
            await refreshAllData();
        } catch (err) {
            DashboardUI.showToast('Scan Error', err.message, 'error');
            elements.scanStatusMsg.textContent = `Scan failed: ${err.message}`;
        } finally {
            state.isScanning = false;
            elements.btnStartScan.disabled = false;
            elements.scanBtnText.textContent = 'Run Deduplication Scan';
            setTimeout(() => {
                if (!state.isScanning) {
                    elements.scanStatusIndicator.style.display = 'none';
                }
            }, 3000);
        }
    }

    /**
     * Perform Prefix Trie query
     */
    async function searchPrefix() {
        const prefix = elements.prefixInput.value.trim();
        if (!prefix) {
            DashboardUI.showToast('Prefix Missing', 'Please enter a path prefix to query the Prefix Trie.', 'warning');
            elements.prefixInput.focus();
            return;
        }

        try {
            const data = await DuplicateXApi.searchPrefix(prefix);
            DashboardUI.renderPrefixResults(data.matches || data.results || [], prefix);
        } catch (err) {
            DashboardUI.showToast('Trie Query Error', err.message, 'error');
        }
    }

    // =========================================================================
    // EVENT BINDINGS
    // =========================================================================

    // Tab Navigation
    if (elements.navTabDuplicates) {
        elements.navTabDuplicates.addEventListener('click', () => switchTab('duplicates'));
    }
    if (elements.navTabQuarantine) {
        elements.navTabQuarantine.addEventListener('click', () => switchTab('quarantine'));
    }

    // Header Refresh
    if (elements.btnRefresh) {
        elements.btnRefresh.addEventListener('click', async () => {
            elements.btnRefresh.classList.add('rotating');
            await refreshAllData();
            setTimeout(() => elements.btnRefresh.classList.remove('rotating'), 500);
        });
    }

    // Directory Queue
    if (elements.btnAddDir) {
        elements.btnAddDir.addEventListener('click', () => {
            addDirectory(elements.dirInput.value);
        });
    }

    if (elements.dirInput) {
        elements.dirInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                addDirectory(elements.dirInput.value);
            }
        });
    }

    if (elements.btnClearAllDirs) {
        elements.btnClearAllDirs.addEventListener('click', () => {
            state.directories = [];
            DashboardUI.renderConfiguredDirs(state.directories, removeDirectory);
        });
    }

    // Scan
    if (elements.btnStartScan) {
        elements.btnStartScan.addEventListener('click', startScan);
    }

    // Top K Select
    if (elements.topKSelect) {
        elements.topKSelect.addEventListener('change', async (e) => {
            state.selectedK = parseInt(e.target.value, 10) || 5;
            const hasCompleted = state.stats && (
                state.stats.last_scan_status === 'COMPLETED' ||
                (state.stats.total_scanned_files !== undefined && state.stats.total_scanned_files > 0) ||
                (state.stats.duplicate_groups !== undefined && state.stats.duplicate_groups > 0)
            );
            if (hasCompleted) {
                try {
                    const topKData = await DuplicateXApi.getTopK(state.selectedK);
                    state.topKGroups = topKData.groups || topKData.top_groups || [];
                    DashboardUI.renderTopK(state.topKGroups);
                } catch (err) {
                    console.error('[APP] Failed to update Top K:', err);
                }
            }
        });
    }

    // Duplicate Filter
    if (elements.duplicateFilterInput) {
        elements.duplicateFilterInput.addEventListener('input', (e) => {
            DashboardUI.renderDuplicates(state.duplicateGroups, e.target.value, handleQuarantinePreviewRequest);
        });
    }

    // Prefix Search
    if (elements.btnSearchPrefix) {
        elements.btnSearchPrefix.addEventListener('click', searchPrefix);
    }

    if (elements.prefixInput) {
        elements.prefixInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                searchPrefix();
            }
        });
    }

    // Quarantine History Toolbar
    if (elements.quarantineSearchInput) {
        elements.quarantineSearchInput.addEventListener('input', (e) => {
            state.quarantineSearch = e.target.value;
            DashboardUI.renderQuarantineHistory(
                state.quarantineRecords,
                state.quarantineFilter,
                state.quarantineSearch,
                handleRestorePreviewRequest
            );
        });
    }

    if (elements.quarantineStatusFilters) {
        elements.quarantineStatusFilters.querySelectorAll('.filter-pill').forEach(btn => {
            btn.addEventListener('click', (e) => {
                elements.quarantineStatusFilters.querySelectorAll('.filter-pill').forEach(b => b.classList.remove('active'));
                const target = e.currentTarget;
                target.classList.add('active');
                state.quarantineFilter = target.dataset.status || 'ALL';
                DashboardUI.renderQuarantineHistory(
                    state.quarantineRecords,
                    state.quarantineFilter,
                    state.quarantineSearch,
                    handleRestorePreviewRequest
                );
            });
        });
    }

    if (elements.btnRefreshHistory) {
        elements.btnRefreshHistory.addEventListener('click', () => refreshQuarantineHistory(true));
    }

    // Quarantine Modal Buttons
    if (elements.btnCloseQuarantineModal) {
        elements.btnCloseQuarantineModal.addEventListener('click', () => {
            if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
            state.activeQuarantinePreview = null;
            if (elements.quarantineModal) elements.quarantineModal.style.display = 'none';
        });
    }

    if (elements.btnCancelQuarantine) {
        elements.btnCancelQuarantine.addEventListener('click', () => {
            if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
            state.activeQuarantinePreview = null;
            if (elements.quarantineModal) elements.quarantineModal.style.display = 'none';
        });
    }

    if (elements.btnConfirmQuarantine) {
        elements.btnConfirmQuarantine.addEventListener('click', executeQuarantine);
    }

    // Restore Modal Buttons
    if (elements.btnCloseRestoreModal) {
        elements.btnCloseRestoreModal.addEventListener('click', () => {
            state.activeRestorePreview = null;
            if (elements.restoreModal) elements.restoreModal.style.display = 'none';
        });
    }

    if (elements.btnCancelRestore) {
        elements.btnCancelRestore.addEventListener('click', () => {
            state.activeRestorePreview = null;
            if (elements.restoreModal) elements.restoreModal.style.display = 'none';
        });
    }

    if (elements.btnConfirmRestore) {
        elements.btnConfirmRestore.addEventListener('click', executeRestore);
    }

    // Close modals on clicking outside backdrop
    window.addEventListener('click', (e) => {
        if (e.target === elements.quarantineModal) {
            if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
            state.activeQuarantinePreview = null;
            elements.quarantineModal.style.display = 'none';
        }
        if (e.target === elements.restoreModal) {
            state.activeRestorePreview = null;
            elements.restoreModal.style.display = 'none';
        }
    });

    // Close modals on Escape key
    window.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            if (elements.quarantineModal && elements.quarantineModal.style.display === 'flex') {
                if (state.quarantineTimerId) clearInterval(state.quarantineTimerId);
                state.activeQuarantinePreview = null;
                elements.quarantineModal.style.display = 'none';
            }
            if (elements.restoreModal && elements.restoreModal.style.display === 'flex') {
                state.activeRestorePreview = null;
                elements.restoreModal.style.display = 'none';
            }
        }
    });

    // Initial load: test connection & fetch initial stats + history
    refreshAllData();

    // Periodic heartbeat every 10 seconds to maintain connection status
    setInterval(checkHealth, 10000);
});

