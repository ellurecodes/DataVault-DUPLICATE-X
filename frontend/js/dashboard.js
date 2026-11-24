/**
 * @file dashboard.js
 * @brief Pure UI rendering functions, text escaping, and DOM builders.
 * @author DataVault Team (Academic Year 2026-2027)
 */

const DashboardUI = {
    /**
     * Format byte values into human-readable units (B, KB, MB, GB, TB)
     */
    formatBytes(bytes) {
        if (bytes === undefined || bytes === null || isNaN(bytes)) return '0 B';
        const num = Number(bytes);
        if (num === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
        const i = Math.floor(Math.log(num) / Math.log(k));
        const idx = Math.min(i, sizes.length - 1);
        const formatted = (num / Math.pow(k, idx)).toFixed(2);
        return `${parseFloat(formatted)} ${sizes[idx]}`;
    },

    /**
     * Format number with comma separators
     */
    formatNumber(num) {
        if (num === undefined || num === null || isNaN(num)) return '0';
        return Number(num).toLocaleString();
    },

    /**
     * Truncate SHA-256 hash for display
     */
    truncateHash(hash) {
        if (!hash || typeof hash !== 'string') return '';
        if (hash.length <= 16) return hash;
        return `${hash.substring(0, 8)}...${hash.substring(hash.length - 8)}`;
    },

    /**
     * Safe HTML string escaper to prevent XSS
     */
    escapeHtml(str) {
        if (str === null || str === undefined) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    },

    /**
     * Render server connection status pill
     */
    renderConnectionStatus(isOnline, hostText = '127.0.0.1:8080') {
        const el = document.getElementById('connectionStatus');
        if (!el) return;

        el.className = 'status-pill ' + (isOnline ? 'status-online' : 'status-offline');
        el.innerHTML = `
            <span class="status-dot"></span>
            <span class="status-text">${isOnline ? `Online (${hostText})` : 'Server Offline'}</span>
        `;
    },

    /**
     * Display loading state on the 4 telemetry metric cards while fetching API stats
     */
    renderMetricsLoading() {
        const totalFiles = document.getElementById('metricTotalFiles');
        const hashedFiles = document.getElementById('metricHashedFiles');
        const dupGroups = document.getElementById('metricDuplicateGroups');
        const recStorage = document.getElementById('metricRecoverableStorage');

        if (totalFiles) totalFiles.innerHTML = '<span class="metric-loading">...</span>';
        if (hashedFiles) hashedFiles.innerHTML = '<span class="metric-loading">...</span>';
        if (dupGroups) dupGroups.innerHTML = '<span class="metric-loading">...</span>';
        if (recStorage) recStorage.innerHTML = '<span class="metric-loading">...</span>';
    },

    /**
     * Display meaningful error state on metric cards if the API server is unreachable
     */
    renderMetricsError(errorMessage) {
        const totalFiles = document.getElementById('metricTotalFiles');
        const hashedFiles = document.getElementById('metricHashedFiles');
        const dupGroups = document.getElementById('metricDuplicateGroups');
        const recStorage = document.getElementById('metricRecoverableStorage');
        const dupCopies = document.getElementById('metricDuplicateCopies');

        const errorHtml = `<span class="metric-error" title="${this.escapeHtml(errorMessage)}">--</span>`;
        if (totalFiles) totalFiles.innerHTML = errorHtml;
        if (hashedFiles) hashedFiles.innerHTML = errorHtml;
        if (dupGroups) dupGroups.innerHTML = errorHtml;
        if (recStorage) recStorage.innerHTML = errorHtml;
        if (dupCopies) dupCopies.textContent = errorMessage || 'API server unavailable';
    },

    /**
     * Render the 4 summary telemetry cards using actual values from GET /api/stats
     *
     * Mapping:
     * - total_scanned_files -> Total Files Scanned
     * - successfully_hashed_files -> Successfully Hashed
     * - duplicate_groups -> Duplicate Groups
     * - duplicate_copies -> Redundant Duplicate Copies Subtitle
     * - recoverable_bytes -> Recoverable Storage
     */
    renderMetrics(stats) {
        if (!stats) return;

        const totalFiles = document.getElementById('metricTotalFiles');
        const hashedFiles = document.getElementById('metricHashedFiles');
        const dupGroups = document.getElementById('metricDuplicateGroups');
        const dupCopies = document.getElementById('metricDuplicateCopies');
        const recStorage = document.getElementById('metricRecoverableStorage');

        // Extract values supporting GET /api/stats and fallback POST /api/scan response shapes
        const totalScanned = (stats.total_scanned_files !== undefined)
            ? stats.total_scanned_files
            : (stats.files_discovered !== undefined ? stats.files_discovered : stats.total_files_discovered);

        const successfullyHashed = (stats.successfully_hashed_files !== undefined)
            ? stats.successfully_hashed_files
            : (stats.files_successfully_hashed !== undefined ? stats.files_successfully_hashed : stats.successfully_hashed);

        const duplicateGroups = stats.duplicate_groups;
        const duplicateCopies = stats.duplicate_copies;
        const recoverableBytes = stats.recoverable_bytes;

        if (totalFiles) {
            totalFiles.textContent = this.formatNumber(totalScanned !== undefined ? totalScanned : 0);
        }
        if (hashedFiles) {
            hashedFiles.textContent = this.formatNumber(successfullyHashed !== undefined ? successfullyHashed : 0);
        }
        if (dupGroups) {
            dupGroups.textContent = this.formatNumber(duplicateGroups !== undefined ? duplicateGroups : 0);
        }
        if (dupCopies) {
            const count = duplicateCopies !== undefined ? duplicateCopies : 0;
            dupCopies.textContent = `${this.formatNumber(count)} redundant duplicate copies`;
        }
        if (recStorage) {
            recStorage.textContent = this.formatBytes(recoverableBytes !== undefined ? recoverableBytes : 0);
        }
    },

    /**
     * Render configured directory badge chips
     */
    renderConfiguredDirs(directories, onRemoveCallback) {
        const container = document.getElementById('configuredDirsList');
        const countEl = document.getElementById('configuredDirsCount');
        const clearBtn = document.getElementById('btnClearAllDirs');
        if (!container) return;

        if (countEl) countEl.textContent = directories.length;
        if (clearBtn) clearBtn.style.display = directories.length > 0 ? 'inline-block' : 'none';

        if (directories.length === 0) {
            container.innerHTML = `<div class="empty-dirs-hint">No directories queued. Enter a Windows directory path above to begin scanning.</div>`;
            return;
        }

        container.innerHTML = '';
        directories.forEach((dir, index) => {
            const chip = document.createElement('div');
            chip.className = 'dir-chip';

            const textSpan = document.createElement('span');
            textSpan.textContent = dir;

            const removeBtn = document.createElement('button');
            removeBtn.className = 'dir-chip-remove';
            removeBtn.innerHTML = '&times;';
            removeBtn.title = 'Remove directory';
            removeBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                onRemoveCallback(index);
            });

            chip.appendChild(textSpan);
            chip.appendChild(removeBtn);
            container.appendChild(chip);
        });
    },

    /**
     * Render Top K Storage Ranking items with visual waste bars
     */
    renderTopK(groups) {
        const container = document.getElementById('topKContainer');
        if (!container) return;

        if (!groups || groups.length === 0) {
            container.innerHTML = `<div class="empty-state">No duplicate storage waste detected in current scan.</div>`;
            return;
        }

        // Find max recoverable bytes for relative bar calculation
        const maxWaste = Math.max(...groups.map(g => Number(g.recoverable_bytes || 0)), 1);

        container.innerHTML = '';
        groups.forEach((group, index) => {
            const rank = index + 1;
            const rankClass = rank === 1 ? 'rank-1' : rank === 2 ? 'rank-2' : rank === 3 ? 'rank-3' : 'rank-other';
            const recoverable = Number(group.recoverable_bytes || 0);
            const percent = Math.min(100, Math.max(5, Math.round((recoverable / maxWaste) * 100)));

            const item = document.createElement('div');
            item.className = 'topk-item';

            item.innerHTML = `
                <div class="topk-rank ${rankClass}">#${rank}</div>
                <div class="topk-info">
                    <div class="topk-name" title="${this.escapeHtml(group.primary_filename || 'Unknown')}">
                        ${this.escapeHtml(group.primary_filename || 'Unknown')}
                    </div>
                    <div class="topk-meta">
                        <span>${this.formatNumber(group.file_count || 0)} copies</span>
                        <span>Group ID: #${this.escapeHtml(group.group_id)}</span>
                        <span class="topk-hash" title="SHA-256: ${this.escapeHtml(group.sha256)}">${this.truncateHash(group.sha256)}</span>
                    </div>
                </div>
                <div class="topk-waste-wrap">
                    <span class="topk-waste-val">+${this.formatBytes(recoverable)}</span>
                    <div class="topk-bar-bg">
                        <div class="topk-bar-fill" style="width: ${percent}%;"></div>
                    </div>
                </div>
            `;
            container.appendChild(item);
        });
    },

    /**
     * Format timestamp to readable date/time
     */
    formatDate(epochSeconds) {
        if (!epochSeconds || epochSeconds <= 0) return '--';
        try {
            return new Date(Number(epochSeconds) * 1000).toLocaleString();
        } catch (e) {
            return '--';
        }
    },

    /**
     * Render Duplicate File Explorer groups in an expandable accordion
     */
    renderDuplicates(groups, filterText = '', onQuarantineClick = null) {
        const container = document.getElementById('duplicatesAccordion');
        const countGroupsEl = document.getElementById('displayedGroupsCount');
        const countCopiesEl = document.getElementById('displayedCopiesCount');
        if (!container) return;

        const query = filterText.trim().toLowerCase();

        // Filter groups based on query matching filename, sha256, or group id
        const filtered = (groups || []).filter(grp => {
            if (!query) return true;
            if (grp.primary_filename && grp.primary_filename.toLowerCase().includes(query)) return true;
            if (grp.sha256 && grp.sha256.toLowerCase().includes(query)) return true;
            if (String(grp.group_id).includes(query)) return true;
            if (grp.files && grp.files.some(f => (f.filepath && f.filepath.toLowerCase().includes(query)) || (f.filename && f.filename.toLowerCase().includes(query)))) {
                return true;
            }
            return false;
        });

        const totalCopies = filtered.reduce((acc, g) => acc + (Number(g.file_count) || 0), 0);

        if (countGroupsEl) countGroupsEl.textContent = this.formatNumber(filtered.length);
        if (countCopiesEl) countCopiesEl.textContent = this.formatNumber(totalCopies);

        if (filtered.length === 0) {
            container.innerHTML = query
                ? `<div class="empty-state">No duplicate groups match filter "${this.escapeHtml(query)}".</div>`
                : `<div class="empty-state">No duplicate groups found. Queue directories above and run a scan.</div>`;
            return;
        }

        container.innerHTML = '';
        filtered.forEach((grp, idx) => {
            const card = document.createElement('div');
            card.className = 'accordion-group' + (idx === 0 ? ' open' : ''); // Expand first group by default

            // Accordion Header
            const header = document.createElement('div');
            header.className = 'accordion-header';
            header.innerHTML = `
                <div class="accordion-title-group">
                    <span class="group-badge">GRP #${this.escapeHtml(grp.group_id)}</span>
                    <span class="group-primary-file" title="${this.escapeHtml(grp.primary_filename)}">${this.escapeHtml(grp.primary_filename)}</span>
                    <span class="group-hash-chip" title="Cryptographic SHA-256 Fingerprint">
                        <svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2">
                            <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
                            <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
                        </svg>
                        ${this.truncateHash(grp.sha256)}
                    </span>
                </div>
                <div class="accordion-meta-group">
                    <span class="waste-pill" title="Recoverable space if duplicates are cleaned">+${this.formatBytes(grp.recoverable_bytes)} waste</span>
                    <span class="copies-pill">${this.formatNumber(grp.file_count)} files</span>
                    <svg class="accordion-chevron" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2">
                        <polyline points="6 9 12 15 18 9"></polyline>
                    </svg>
                </div>
            `;

            header.addEventListener('click', () => {
                card.classList.toggle('open');
            });

            // Accordion Content (Table of files in group + selection action bar)
            const content = document.createElement('div');
            content.className = 'accordion-content';

            // Group Action Toolbar for Quarantine Selection
            const actionBar = document.createElement('div');
            actionBar.className = 'group-action-bar';
            actionBar.innerHTML = `
                <div class="group-action-left">
                    <span class="selection-summary">
                        Selected: <strong class="sel-count">0</strong> redundant copies (<strong class="sel-bytes">0 B</strong>)
                    </span>
                    <button type="button" class="btn-link btn-select-redundant">Select Redundant</button>
                    <button type="button" class="btn-link btn-clear-selection" style="display: none;">Clear</button>
                </div>
                <div class="group-action-right">
                    <span style="font-size: 0.74rem; color: var(--text-muted);">1 valid original copy must remain</span>
                    <button type="button" class="btn btn-danger btn-quarantine-submit" disabled>
                        <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2">
                            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"></path>
                            <path d="M12 8v4"></path>
                            <path d="M12 16h.01"></path>
                        </svg>
                        Quarantine Selected
                    </button>
                </div>
            `;
            content.appendChild(actionBar);

            const selCountEl = actionBar.querySelector('.sel-count');
            const selBytesEl = actionBar.querySelector('.sel-bytes');
            const btnQuarantine = actionBar.querySelector('.btn-quarantine-submit');
            const btnSelectAll = actionBar.querySelector('.btn-select-redundant');
            const btnClear = actionBar.querySelector('.btn-clear-selection');

            // Table of Files in Group
            const tableWrap = document.createElement('div');
            let tableHtml = `
                <table class="file-table">
                    <thead>
                        <tr>
                            <th style="width: 44px; text-align: center;">Select</th>
                            <th style="width: 120px;">Role</th>
                            <th style="width: 110px;">Device / Volume</th>
                            <th>Filename &amp; Path</th>
                            <th style="width: 100px; text-align: right;">Size</th>
                        </tr>
                    </thead>
                    <tbody>
            `;

            (grp.files || []).forEach((f, fileIdx) => {
                const isOriginal = fileIdx === 0;
                const fileId = f.file_id !== undefined ? f.file_id : (fileIdx + 1);
                const fileSize = Number(f.file_size || 0);

                tableHtml += `
                    <tr>
                        <td style="text-align: center;">
                            <input type="checkbox" class="file-cb group-file-cb" 
                                   data-group-id="${this.escapeHtml(grp.group_id)}"
                                   data-file-id="${this.escapeHtml(fileId)}"
                                   data-size="${fileSize}"
                                   data-path="${this.escapeHtml(f.filepath || '')}"
                                   data-name="${this.escapeHtml(f.filename || '')}"
                                   ${isOriginal ? 'disabled title="Original root copy: must remain safe"' : 'title="Select redundant copy for quarantine"'}>
                        </td>
                        <td>
                            <span class="file-role-badge ${isOriginal ? 'role-original' : 'role-duplicate'}">
                                ${isOriginal ? 'Root / Keep' : 'Duplicate'}
                            </span>
                        </td>
                        <td><strong>${this.escapeHtml(f.device_id || 'Local')}</strong></td>
                        <td>
                            <div style="font-weight: 600;">${this.escapeHtml(f.filename || '')}</div>
                            <div class="file-path-cell">${this.escapeHtml(f.filepath || '')}</div>
                        </td>
                        <td style="text-align: right; font-weight: 600;">${this.formatBytes(fileSize)}</td>
                    </tr>
                `;
            });

            tableHtml += `
                    </tbody>
                </table>
            `;
            tableWrap.innerHTML = tableHtml;
            content.appendChild(tableWrap);

            // Selection Event Handling for this Group
            const checkboxes = content.querySelectorAll('.group-file-cb:not(:disabled)');

            function updateGroupSelection() {
                let selectedCount = 0;
                let selectedBytes = 0;
                const selectedIds = [];
                const selectedFiles = [];

                checkboxes.forEach(cb => {
                    if (cb.checked) {
                        selectedCount++;
                        selectedBytes += Number(cb.dataset.size || 0);
                        selectedIds.push(Number(cb.dataset.fileId));
                        selectedFiles.push({
                            file_id: Number(cb.dataset.fileId),
                            filepath: cb.dataset.path,
                            filename: cb.dataset.name,
                            file_size: Number(cb.dataset.size || 0)
                        });
                    }
                });

                if (selCountEl) selCountEl.textContent = DashboardUI.formatNumber(selectedCount);
                if (selBytesEl) selBytesEl.textContent = DashboardUI.formatBytes(selectedBytes);

                if (btnClear) btnClear.style.display = selectedCount > 0 ? 'inline-block' : 'none';

                // Invariant: At least 1 copy must remain, and at least 1 must be selected
                const totalInGroup = grp.files ? grp.files.length : 0;
                const remaining = totalInGroup - selectedCount;
                const isValid = (selectedCount > 0 && remaining >= 1);

                if (btnQuarantine) {
                    btnQuarantine.disabled = !isValid;
                    if (remaining < 1 && selectedCount > 0) {
                        btnQuarantine.title = 'Cannot quarantine all copies: at least 1 copy must remain';
                    } else if (selectedCount === 0) {
                        btnQuarantine.title = 'Select at least one duplicate file to quarantine';
                    } else {
                        btnQuarantine.title = `Quarantine ${selectedCount} selected duplicate copy(ies)`;
                    }
                }

                return { selectedIds, selectedFiles, selectedBytes, remaining };
            }

            checkboxes.forEach(cb => {
                cb.addEventListener('change', updateGroupSelection);
            });

            if (btnSelectAll) {
                btnSelectAll.addEventListener('click', () => {
                    checkboxes.forEach(cb => { cb.checked = true; });
                    updateGroupSelection();
                });
            }

            if (btnClear) {
                btnClear.addEventListener('click', () => {
                    checkboxes.forEach(cb => { cb.checked = false; });
                    updateGroupSelection();
                });
            }

            if (btnQuarantine && onQuarantineClick) {
                btnQuarantine.addEventListener('click', () => {
                    const sel = updateGroupSelection();
                    if (sel.selectedIds.length > 0 && sel.remaining >= 1) {
                        onQuarantineClick(grp.group_id, sel.selectedIds, sel.selectedFiles, grp);
                    }
                });
            }

            card.appendChild(header);
            card.appendChild(content);
            container.appendChild(card);
        });
    },

    /**
     * Render Quarantine Transaction History Table
     */
    renderQuarantineHistory(records, filterStatus = 'ALL', searchQuery = '', onRestoreClick = null) {
        const container = document.getElementById('quarantineTableContainer');
        const qStatTotal = document.getElementById('qStatTotal');
        const qStatActive = document.getElementById('qStatActive');
        const qStatRestored = document.getElementById('qStatRestored');
        const qStatFailed = document.getElementById('qStatFailed');

        const qCountAll = document.getElementById('qCountAll');
        const qCountActive = document.getElementById('qCountActive');
        const qCountRestored = document.getElementById('qCountRestored');
        const qCountFailed = document.getElementById('qCountFailed');
        const navQuarantineCount = document.getElementById('navQuarantineCount');

        const allRecords = records || [];

        // Count metrics across all records
        const countTotal = allRecords.length;
        const countActive = allRecords.filter(r => r.status === 'ACTIVE').length;
        const countRestored = allRecords.filter(r => r.status === 'RESTORED').length;
        const countFailed = allRecords.filter(r => r.status === 'FAILED').length;

        if (qStatTotal) qStatTotal.textContent = this.formatNumber(countTotal);
        if (qStatActive) qStatActive.textContent = this.formatNumber(countActive);
        if (qStatRestored) qStatRestored.textContent = this.formatNumber(countRestored);
        if (qStatFailed) qStatFailed.textContent = this.formatNumber(countFailed);

        if (qCountAll) qCountAll.textContent = this.formatNumber(countTotal);
        if (qCountActive) qCountActive.textContent = this.formatNumber(countActive);
        if (qCountRestored) qCountRestored.textContent = this.formatNumber(countRestored);
        if (qCountFailed) qCountFailed.textContent = this.formatNumber(countFailed);

        // Update navigation tab badge
        if (navQuarantineCount) {
            navQuarantineCount.textContent = this.formatNumber(countActive);
            navQuarantineCount.style.display = countActive > 0 ? 'inline-block' : 'none';
        }

        if (!container) return;

        // Apply filters
        const q = searchQuery.trim().toLowerCase();
        const filtered = allRecords.filter(r => {
            // Status filter
            if (filterStatus !== 'ALL' && r.status !== filterStatus) return false;
            // Search filter
            if (q) {
                const fn = (r.original_filename || '').toLowerCase();
                const op = (r.original_path || '').toLowerCase();
                const qp = (r.quarantine_path || '').toLowerCase();
                const tx = (r.transaction_id || '').toLowerCase();
                const sh = (r.sha256_hex || '').toLowerCase();
                if (!fn.includes(q) && !op.includes(q) && !qp.includes(q) && !tx.includes(q) && !sh.includes(q)) {
                    return false;
                }
            }
            return true;
        });

        if (filtered.length === 0) {
            container.innerHTML = (q || filterStatus !== 'ALL')
                ? `<div class="empty-state">No quarantine transactions match current filter/search.</div>`
                : `<div class="empty-state">No files have been quarantined yet. In Duplicate Management, select files and click "Quarantine Selected".</div>`;
            return;
        }

        let tableHtml = `
            <table class="file-table">
                <thead>
                    <tr>
                        <th style="width: 100px;">Status</th>
                        <th style="width: 130px;">Transaction ID</th>
                        <th>Original Filename &amp; Path</th>
                        <th>Quarantine Location</th>
                        <th style="width: 85px; text-align: right;">Size</th>
                        <th style="width: 140px;">Quarantine Date</th>
                        <th style="width: 100px; text-align: center;">Action</th>
                    </tr>
                </thead>
                <tbody>
        `;

        filtered.forEach(r => {
            const statusClass = (r.status === 'ACTIVE') ? 'badge-active' :
                                (r.status === 'RESTORED') ? 'badge-restored' : 'badge-failed';
            const isActive = (r.status === 'ACTIVE');

            tableHtml += `
                <tr>
                    <td><span class="status-badge ${statusClass}">${this.escapeHtml(r.status)}</span></td>
                    <td><span class="tx-chip" title="${this.escapeHtml(r.transaction_id)}">${this.truncateHash(r.transaction_id)}</span></td>
                    <td>
                        <div style="font-weight: 600;">${this.escapeHtml(r.original_filename || '')}</div>
                        <div class="file-path-cell" title="${this.escapeHtml(r.original_path || '')}">${this.escapeHtml(r.original_path || '')}</div>
                    </td>
                    <td>
                        <div class="file-path-cell" title="${this.escapeHtml(r.quarantine_path || '')}">${this.escapeHtml(r.quarantine_path || '')}</div>
                    </td>
                    <td style="text-align: right; font-weight: 600;">${this.formatBytes(r.file_size)}</td>
                    <td style="font-size: 0.76rem; color: var(--text-secondary);">${this.formatDate(r.quarantine_timestamp)}</td>
                    <td style="text-align: center;">
                        ${isActive ? `
                            <button class="btn btn-secondary btn-sm btn-restore-tx" data-tx="${this.escapeHtml(r.transaction_id)}" title="Safely restore this file to original location">
                                <svg viewBox="0 0 24 24" width="13" height="13" fill="none" stroke="currentColor" stroke-width="2">
                                    <polyline points="23 4 23 10 17 10"></polyline>
                                    <path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"></path>
                                </svg>
                                Restore
                            </button>
                        ` : `
                            <span style="color: var(--text-muted); font-size: 0.78rem;">${r.status === 'RESTORED' ? 'Restored' : '--'}</span>
                        `}
                    </td>
                </tr>
            `;
        });

        tableHtml += `
                </tbody>
            </table>
        `;

        container.innerHTML = tableHtml;

        // Attach restore button click listeners
        if (onRestoreClick) {
            container.querySelectorAll('.btn-restore-tx').forEach(btn => {
                btn.addEventListener('click', (e) => {
                    const txId = e.currentTarget.dataset.tx;
                    if (txId) onRestoreClick(txId);
                });
            });
        }
    },

    /**
     * Show non-intrusive toast notification
     */
    showToast(title, message, type = 'info', duration = 4000) {
        const container = document.getElementById('toastContainer');
        if (!container) return;

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.innerHTML = `
            <div style="flex: 1;">
                <div class="toast-title">${this.escapeHtml(title)}</div>
                <div class="toast-msg">${this.escapeHtml(message)}</div>
            </div>
        `;
        container.appendChild(toast);

        setTimeout(() => {
            toast.style.opacity = '0';
            toast.style.transform = 'translateX(100%)';
            setTimeout(() => { if (toast.parentNode) toast.remove(); }, 300);
        }, duration);
    }

    /**
     * Render Prefix Trie Search Results
     */
    renderPrefixResults(results, queryPrefix) {
        const container = document.getElementById('prefixResultsContainer');
        if (!container) return;

        if (!results || results.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    No files found matching prefix "<strong>${this.escapeHtml(queryPrefix)}</strong>" in Prefix Trie.
                </div>
            `;
            return;
        }

        let html = `
            <div style="margin-bottom: 12px; font-size: 0.8rem; color: var(--text-secondary);">
                Found <strong style="color: var(--accent-cyan);">${this.formatNumber(results.length)}</strong> matching file path(s) under prefix "<strong>${this.escapeHtml(queryPrefix)}</strong>":
            </div>
            <table class="file-table">
                <thead>
                    <tr>
                        <th style="width: 120px;">Device</th>
                        <th>Matched File Path</th>
                        <th style="width: 100px; text-align: right;">Size</th>
                    </tr>
                </thead>
                <tbody>
        `;

        results.forEach(res => {
            html += `
                <tr>
                    <td><strong>${this.escapeHtml(res.device_id || 'Drive')}</strong></td>
                    <td>
                        <div style="font-weight: 600;">${this.escapeHtml(res.filename || '')}</div>
                        <div class="file-path-cell">${this.escapeHtml(res.filepath || '')}</div>
                    </td>
                    <td style="text-align: right; font-weight: 600;">${this.formatBytes(res.file_size)}</td>
                </tr>
            `;
        });

        html += `
                </tbody>
            </table>
        `;

        container.innerHTML = html;
    }
};

