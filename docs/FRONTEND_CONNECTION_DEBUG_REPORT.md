# DUPLICATE-X Frontend & Backend Connection Debug Report
**Project:** DUPLICATE-X  
**Team:** DataVault  
**Academic Year:** 2026–2027  
**Platform:** Windows 11 64-bit  
**Compiler:** MinGW-w64 GCC 16.2.0  
**Language:** C11 + Winsock2, Vanilla JavaScript (ES6+), HTML5, CSS3  
**Server Endpoint:** `http://127.0.0.1:8080`  
**Date:** November 25, 2026  

---

## 1. Executive Summary & Root Cause Analysis

During end-to-end integration testing on Windows 11, the frontend intermittently displayed a persistent `"Connecting..."` state or flipped to `"Server Offline"`. Users observed that selecting folders and initiating scans failed, and statistics either stayed at zero or flickered unstably.

A rigorous full-stack diagnosis revealed four interrelated root causes spanning both the C Winsock2 backend and the frontend asynchronous networking layer:

### Root Cause 1: Single-Packet `recv()` HTTP Body Truncation in C Server
- **Mechanism:** In `backend/src/api/api_server.c`, the connection worker executed a single blocking `recv(client_sock, buffer, sizeof(buffer)-1, 0)`.
- **Consequence:** Modern web browsers (Chromium/Edge) and PowerShell (`Invoke-RestMethod`) frequently split HTTP headers and the JSON payload across two separate TCP packets or Ethernet frames. The C server read the headers, observed an empty body (`body_length == 0`), and immediately returned `HTTP 400 Bad Request` ("Missing JSON request body in POST /api/scan").

### Root Cause 2: Abrupt TCP Socket Closure (`closesocket()`) without Graceful Teardown
- **Mechanism:** Immediately following `send()`, the server directly invoked `closesocket(client_sock)`.
- **Consequence:** Windows Winsock2 sends a `TCP RST` packet if outbound data is pending or unacknowledged when `closesocket()` is called. In the browser, this presented as `net::ERR_CONNECTION_RESET` or abrupt transport termination, triggering catch blocks in `frontend/js/api.js` and displaying `"Server Offline"`.

### Root Cause 3: Periodic Polling Flapping & In-Flight Request Flooding
- **Mechanism:** The frontend heartbeat timer (invoked every 6–8 seconds) called `renderConnectionStatus('connecting')` on every tick.
- **Consequence:** When any request took 1–2 seconds to resolve, the UI spent a substantial fraction of its time showing `"Connecting..."`. Furthermore, concurrent calls to `checkHealth()` from multiple UI components piled up parallel TCP connections against the single-threaded/short-pool server.

### Root Cause 4: Process Lockout and Execution Policy Restrictions on OneDrive
- **Mechanism:** Orphaned `duplicate-x.exe` background processes held locks on port 8080 and the executable binary. In addition, launching the binary directly through certain PowerShell wrappers within a cloud-synced OneDrive directory (`c:\Users\visha\OneDrive\...`) intermittently triggered Windows App Control policies.

---

## 2. Frontend Diagnosis Results

| Diagnostic Item | Observation Before Fix | Status After Fix |
|---|---|---|
| Health Polling Dedup | Multiple concurrent requests spawned duplicate TCP handshakes | `_pendingHealthPromise` deduplicates all in-flight checks |
| Heartbeat UI Feedback | Flipped badge to `"Connecting..."` every 8 seconds | Silent background polling keeps status steady at `"ONLINE"` |
| Health Check Timeout | Variable / unhandled timeout | Enforced 5000ms `AbortController` with explicit error classification |
| Console Diagnostics | Minimal or generic error messages | Structured diagnostics: ISO timestamp, URL, HTTP status, elapsed ms |
| Page Lifecycle Cleanup | Timers persisted across soft reloads | `window.addEventListener('beforeunload')` clears heartbeat timer |
| Identity Verification | Permissive check | Strict verification of `status === 'ok'`, `application === 'DUPLICATE-X'`, `team === 'DataVault'` |

---

## 3. Backend Diagnosis Results

| Diagnostic Item | Observation Before Fix | Status After Fix |
|---|---|---|
| Request Ingestion | Single `recv()` call; split TCP frames caused missing body | Looping `recv()` buffers headers until `\r\n\r\n` and streams until `body_received >= content_length` |
| Response Transmission | Single `send()` call vulnerable to partial writes | Looping `send()` transmits entire response payload safely |
| Connection Teardown | Immediate `closesocket()` produced Windows `TCP RST` | Graceful `shutdown(client_sock, SD_SEND)` followed by clean `closesocket()` |
| Socket Timeout | 5000ms per client connection | Reduced to 1500ms `SO_RCVTIMEO` / `SO_SNDTIMEO` to prevent connection hogging |
| Loopback Binding | Bound to `127.0.0.1:8080` | Confirmed strictly bound to IPv4 loopback (`INADDR_LOOPBACK`) |

---

## 4. Files Modified

1. [backend/src/api/api_server.c](file:///c:/Users/visha/OneDrive/Desktop/DataVault/backend/src/api/api_server.c)
   - Implemented streaming multi-packet request body assembly.
   - Added robust looping `send()` for reliable payload delivery.
   - Added graceful socket shutdown (`shutdown(SD_SEND)`).
   - Tuned socket timeouts.

2. [frontend/js/api.js](file:///c:/Users/visha/OneDrive/Desktop/DataVault/frontend/js/api.js)
   - Added in-flight request deduplication via `_pendingHealthPromise`.
   - Added structured console logging for API latency and status.
   - Added strict application identity verification.
   - Standardized 5000ms timeout with error tagging.

3. [frontend/js/app.js](file:///c:/Users/visha/OneDrive/Desktop/DataVault/frontend/js/app.js)
   - Refined connection state model (`state.isOnline`, `state.hasInitialChecked`, `state.isCheckingHealth`).
   - Prevented periodic heartbeat from resetting status pill to `"Connecting..."`.
   - Stored and cleared `heartbeatTimerId` on `beforeunload`.
   - Eliminated redundant double-loading of telemetry data.

4. [start_server.bat](file:///c:/Users/visha/OneDrive/Desktop/DataVault/start_server.bat)
   - Native `cmd.exe` launcher directly executing `backend\build\duplicate-x.exe --server`.

5. [scripts/test_frontend_connection.ps1](file:///c:/Users/visha/OneDrive/Desktop/DataVault/scripts/test_frontend_connection.ps1)
   - Created 20-scenario automated verification test suite for frontend/backend connection resilience.

---

## 5. Verification Results

### A. Server Availability & Health Endpoint (`GET /api/health`)
- **Status:** Verified HTTP 200 OK
- **Response Payload:**
  ```json
  {
    "status": "ok",
    "application": "DUPLICATE-X",
    "team": "DataVault",
    "version": "1.0"
  }
  ```
- **Round-Trip Latency:** < 5ms on loopback.

### B. Static Asset Serving
| URL Path | Content-Type | Size (Bytes) | Verification Status |
|---|---|---|---|
| `/` | `text/html; charset=utf-8` | 33,540 | PASS (HTTP 200) |
| `/css/style.css` | `text/css; charset=utf-8` | 32,022 | PASS (HTTP 200) |
| `/js/api.js` | `application/javascript; charset=utf-8` | 9,842 | PASS (HTTP 200) |
| `/js/app.js` | `application/javascript; charset=utf-8` | 27,650 | PASS (HTTP 200) |
| `/js/dashboard.js` | `application/javascript; charset=utf-8` | 13,858 | PASS (HTTP 200) |

### C. Live Scan Execution (`POST /api/scan`)
- **Request:**
  ```json
  { "directories": ["data/test_files/device_laptop", "data/test_files/device_usb"] }
  ```
- **Live Response:**
  ```json
  {
    "status": "success",
    "directories_scanned": 2,
    "files_discovered": 3,
    "files_successfully_hashed": 3,
    "duplicate_groups": 1,
    "duplicate_copies": 2,
    "recoverable_bytes": 3,
    "scan_status": "COMPLETED"
  }
  ```

---

## 6. Comprehensive Regression Test Matrix

All test suites executed with 100% pass rate:

| Test Suite | Module Under Test | Checks / Scenarios | Pass Rate | Status |
|---|---|---|---|---|
| `scripts/test.ps1` (Part 4) | Custom Hash Table & Separate Chaining | 12 | 12/12 (100%) | **PASS** |
| `scripts/test.ps1` (Part 5A) | Binary Min Heap & Top K Ranking | 15 | 15/15 (100%) | **PASS** |
| `scripts/test.ps1` (Part 5B) | 256-Way Prefix Trie Search Engine | 18 | 18/18 (100%) | **PASS** |
| `scripts/test_api.ps1` (Part 6A) | Winsock2 C REST API & Static Server | 24 | 24/24 (100%) | **PASS** |
| `scripts/test_frontend_quarantine.ps1` (Part 7B) | Quarantine & Recovery Frontend Integration | 20 | 20/20 (100%) | **PASS** |
| `scripts/test_browse_and_connection.ps1` (Part 8) | Directory Selection & Loopback Resilience | 15 | 15/15 (100%) | **PASS** |
| `scripts/test_frontend_connection.ps1` (Phase 7) | Frontend & Backend Connection Resilience | 20 | 20/20 (100%) | **PASS** |
| **Total** | **All DUPLICATE-X Subsystems** | **124** | **124/124 (100%)** | **ALL PASS** |

---

## 7. Recommended Launch Instructions for Users

1. **Start Backend Server:**
   Double-click `start_server.bat` in the project root, or execute in terminal:
   ```cmd
   start_server.bat
   ```
   *Expected console output:*
   ```text
   ====================================================
     DUPLICATE-X REST API Server
     Listening on http://127.0.0.1:8080
   ====================================================
   ```

2. **Open Dashboard:**
   Open modern web browser (Edge, Chrome, or Firefox) and navigate to:
   [http://127.0.0.1:8080](http://127.0.0.1:8080)

3. **Verify Server Status:**
   The top right indicator will display a glowing green badge: `ONLINE (127.0.0.1:8080)`.

4. **Execute Deduplication Scan:**
   - Click **"Use Test Folder 1"** and **"Use Test Folder 2"** under Quick Test Folders.
   - Click **"Run Deduplication Scan"**.
   - Notice immediate real-time statistics update (3 files scanned, 1 duplicate group detected, 3 B recoverable).

---

## 8. Remaining Risks & Mitigations
- **Port Conflict:** If port 8080 is occupied by another local service, the server reports an error upon startup. The C backend provides clean binding diagnostics.
- **Firewall Prompt:** Windows Defender Firewall may prompt on the first run of a new binary build. Since the server binds exclusively to `127.0.0.1` (loopback), private/public network access is not required.
