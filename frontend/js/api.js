/**
 * @file api.js
 * @brief REST API client wrapper communicating with local C Winsock2 server.
 * @author DataVault Team (Academic Year 2026-2027)
 */

const DuplicateXApi = {
    baseUrl: '', // Same-origin relative URLs directly to C Winsock2 backend

    /**
     * Helper to perform fetch requests with error handling
     */
    async _request(endpoint, options = {}) {
        try {
            const url = `${this.baseUrl}${endpoint}`;
            const response = await fetch(url, {
                ...options,
                headers: {
                    'Accept': 'application/json',
                    ...(options.headers || {})
                }
            });

            const contentType = response.headers.get('content-type') || '';
            const isJson = contentType.includes('application/json');
            const data = isJson ? await response.json() : await response.text();

            if (!response.ok) {
                const errorMsg = (typeof data === 'object' && data.error) 
                    ? data.error 
                    : `HTTP ${response.status} ${response.statusText}`;
                throw new Error(errorMsg);
            }

            return data;
        } catch (err) {
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
    }
};

