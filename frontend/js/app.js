/**
 * @file app.js
 * @brief Application lifecycle controller, state management, and event orchestration.
 * @author DataVault Team (Academic Year 2026-2027)
 */

document.addEventListener('DOMContentLoaded', () => {
    // Application State
    const state = {
        isOnline: false,
        directories: [],
        stats: null,
        topKGroups: [],
        duplicateGroups: [],
        isScanning: false,
        selectedK: 5
    };

    // DOM Elements
    const elements = {
        btnRefresh: document.getElementById('btnRefresh'),
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
        btnSearchPrefix: document.getElementById('btnSearchPrefix')
    };

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
                DashboardUI.renderDuplicates(state.duplicateGroups, elements.duplicateFilterInput ? elements.duplicateFilterInput.value : '');

                // Fetch fresh Top K from GET /api/top-k?k=...
                const topKData = await DuplicateXApi.getTopK(state.selectedK);
                state.topKGroups = topKData.groups || [];
                DashboardUI.renderTopK(state.topKGroups);
            } else {
                state.duplicateGroups = [];
                state.topKGroups = [];
                DashboardUI.renderDuplicates([], '');
                DashboardUI.renderTopK([]);
            }
        } catch (err) {
            console.error('[APP] Error refreshing dashboard data:', err);
            DashboardUI.renderMetricsError(err.message || 'Failed to fetch statistics');
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
            alert('Please add at least one directory path to the scan queue before scanning.');
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
            
            // Reload all telemetry, top K, and duplicates
            await refreshAllData();
        } catch (err) {
            alert(`Scan error: ${err.message}`);
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
            alert('Please enter a directory path prefix to query the Prefix Trie.');
            elements.prefixInput.focus();
            return;
        }

        try {
            const data = await DuplicateXApi.searchPrefix(prefix);
            DashboardUI.renderPrefixResults(data.results || [], prefix);
        } catch (err) {
            alert(`Trie search error: ${err.message}`);
        }
    }

    // Event Bindings
    if (elements.btnRefresh) {
        elements.btnRefresh.addEventListener('click', async () => {
            elements.btnRefresh.classList.add('rotating');
            await refreshAllData();
            setTimeout(() => elements.btnRefresh.classList.remove('rotating'), 500);
        });
    }

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

    if (elements.btnStartScan) {
        elements.btnStartScan.addEventListener('click', startScan);
    }

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
                    state.topKGroups = topKData.groups || [];
                    DashboardUI.renderTopK(state.topKGroups);
                } catch (err) {
                    console.error('[APP] Failed to update Top K:', err);
                }
            }
        });
    }

    if (elements.duplicateFilterInput) {
        elements.duplicateFilterInput.addEventListener('input', (e) => {
            DashboardUI.renderDuplicates(state.duplicateGroups, e.target.value);
        });
    }

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

    // Initial load: test connection & fetch initial stats
    refreshAllData();

    // Periodic heartbeat every 10 seconds to maintain connection status
    setInterval(checkHealth, 10000);
});

