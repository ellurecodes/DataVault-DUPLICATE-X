/**
 * @file api.js
 * @brief REST API client wrapper communicating with local C Winsock2 server.
 * @author DataVault Team (Academic Year 2026-2027)
 */

const DuplicateXApi = {
    // If opened via file:// or another port (e.g. Live Server :5500), connect directly to local C server on port 8080
    baseUrl: (typeof window !== 'undefined' && (window.location.protocol === 'file:' || (window.location.port && window.location.port !== '8080')))
        ? 'http://127.0.0.1:8080'
        : '',

    /**
     * Helper to perform fetch requests with error handling
     */
    async _request(endpoint, options = {}) {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 4000);

        try {
            const url = `${this.baseUrl}${endpoint}`;
            let response;
            try {
                response = await fetch(url, {
                    ...options,
                    signal: options.signal || controller.signal,
                    headers: {
                        'Accept': 'application/json',
                        ...(options.headers || {})
                    }
                });
            } catch (netErr) {
                // If 127.0.0.1 failed on file:// protocol, attempt localhost fallback
                if (this.baseUrl === 'http://127.0.0.1:8080') {
                    const fallbackUrl = `http://localhost:8080${endpoint}`;
                    response = await fetch(fallbackUrl, {
                        ...options,
                        signal: options.signal || controller.signal,
                        headers: {
                            'Accept': 'application/json',
                            ...(options.headers || {})
                        }
                    });
                    this.baseUrl = 'http://localhost:8080';
                } else {
                    throw netErr;
                }
            }
            clearTimeout(timeoutId);

            const contentType = response.headers.get('content-type') || '';
            const isJson = contentType.includes('application/json');
            const data = isJson ? await response.json() : await response.text();

            if (!response.ok) {
                const errorMsg = (typeof data === 'object' && (data.error || data.message)) 
                    ? (data.error || data.message)
                    : (typeof data === 'string' && data.length > 0 ? data : `HTTP ${response.status} ${response.statusText}`);
                const err = new Error(errorMsg);
                err.status = response.status;
                err.data = data;
                throw err;
            }

            return data;
        } catch (err) {
            clearTimeout(timeoutId);
            console.error(`[API ERROR] ${options.method || 'GET'} ${endpoint}:`, err);
            throw err;
        }
    },

    /**
     * Check server health status
     * GET /api/health
     */
    async getHealth() {
        return await this._request('/api/health');
    },

    /**
     * Fetch engine telemetry and scanner statistics
     * GET /api/stats
     */
    async getStats() {
        return await this._request('/api/stats');
    },

    /**
     * Fetch all duplicate groups discovered by custom Hash Table
     * GET /api/duplicates
     */
    async getDuplicates() {
        return await this._request('/api/duplicates');
    },

    /**
     * Fetch Top K largest storage-wasting duplicate groups from Binary Min Heap
     * GET /api/top-k?k={k}
     */
    async getTopK(k = 5) {
        return await this._request(`/api/top-k?k=${encodeURIComponent(k)}`);
    },

    /**
     * Query Prefix Trie for paths matching a given prefix
     * GET /api/search/prefix?prefix={prefix}
     */
    async searchPrefix(prefix) {
        return await this._request(`/api/search/prefix?prefix=${encodeURIComponent(prefix)}`);
    },

    /**
     * Open native Windows folder selection dialog through C backend
     * POST /api/folder-picker
     */
    async openFolderPicker() {
        return await this._request('/api/folder-picker', {
            method: 'POST'
        });
    },

    /**
     * Trigger filesystem scan across one or more target directories
     * POST /api/scan
     */
    async triggerScan(directories) {
        return await this._request('/api/scan', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ directories })
        });
    },

    /**
     * Request a quarantine preview for selected files in a duplicate group (Stage 1)
     * POST /api/quarantine/preview
     */
    async quarantinePreview(groupId, fileIds) {
        return await this._request('/api/quarantine/preview', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                group_id: Number(groupId),
                file_ids: fileIds.map(Number)
            })
        });
    },

    /**
     * Execute confirmed quarantine relocation using a valid preview token (Stage 2)
     * POST /api/quarantine/execute
     */
    async quarantineExecute(previewId, confirmation = 'QUARANTINE') {
        return await this._request('/api/quarantine/execute', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                preview_id: String(previewId),
                confirmation: String(confirmation)
            })
        });
    },

    /**
     * Fetch persistent quarantine transaction history manifest
     * GET /api/quarantine/history
     */
    async getQuarantineHistory() {
        return await this._request('/api/quarantine/history');
    },

    /**
     * Request a restoration preview for a quarantined transaction (Stage 1)
     * POST /api/quarantine/restore/preview
     */
    async restorePreview(transactionId) {
        return await this._request('/api/quarantine/restore/preview', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                transaction_id: String(transactionId)
            })
        });
    },

    /**
     * Execute confirmed restoration using a valid preview token (Stage 2)
     * POST /api/quarantine/restore/execute
     */
    async restoreExecute(previewId, confirmation = 'RESTORE') {
        return await this._request('/api/quarantine/restore/execute', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                preview_id: String(previewId),
                confirmation: String(confirmation)
            })
        });
    }
};

