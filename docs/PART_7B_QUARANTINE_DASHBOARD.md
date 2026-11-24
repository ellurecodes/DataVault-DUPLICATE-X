# DUPLICATE-X: Part 7B — Frontend Quarantine & File Recovery Dashboard
**Team Name:** DataVault  
**Academic Year:** 2026–2027  
**Platform:** Windows 11 (64-bit) | **Backend:** Pure C11 (Winsock2 REST API)  
**Frontend:** HTML5, Responsive CSS3, Vanilla JavaScript (ES6+)  

---

## 1. Executive Summary & UI Architecture

**DUPLICATE-X Part 7B** integrates the safe quarantine and file recovery engine into an intuitive, high-performance web dashboard. The design preserves the dark navy and cyan visual identity while introducing a clean tabbed navigation dividing **Duplicate Management** and **Quarantine & Recovery**.

```
+-------------------------------------------------------------------------------------------------------+
|  🛡️ DataVault DUPLICATE-X  |  Smarter Storage. Zero Duplicate Waste.  [Online: 127.0.0.1:8080] [Refresh] |
+-------------------------------------------------------------------------------------------------------+
|  [📁 Duplicate Management (Active)]         [🛡️ Quarantine & Recovery (Badge: Active Count)]            |
+-------------------------------------------------------------------------------------------------------+
|                                                                                                       |
|  VIEW A: DUPLICATE MANAGEMENT                     VIEW B: QUARANTINE & RECOVERY                       |
|  ----------------------------------               -----------------------------------                 |
|  1. Summary Telemetry Cards (4 Metrics)           1. Safe Volume-Local Architecture Banner            |
|  2. Target Directory Queue & Scan Trigger         2. Quarantine Metrics (Total, Active, Restored)     |
|  3. Top K Storage Waste (Binary Min Heap)         3. Transaction Search & Status Filter Pills         |
|  4. Duplicate File Explorer (Hash Table & Lists)  4. Persistent History Table (from C Manifest)       |
|     - Checkbox selection per duplicate copy       5. Stage 1/2 Restore Modals                         |
|     - Sole original copy protection lock                                                              |
|     - Stage 1/2 Quarantine Modals                                                                     |
|  5. Fast Path Prefix Search (Prefix Trie)                                                             |
|                                                                                                       |
+-------------------------------------------------------------------------------------------------------+
```

### Key Architectural Tenets
1. **Decoupled Architecture:** All destructive file relocation logic remains strictly within the compiled C11 engine (`backend/src/core/quarantine.c`). JavaScript acts purely as a presentation and coordination layer.
2. **Two-Stage Confirmation Enforcement:** No file can be moved or restored via a single click. Every operation requires a Stage 1 cryptographic preview (`POST /api/quarantine/preview` or `POST /api/quarantine/restore/preview`) followed by an explicit Stage 2 user confirmation (`POST /api/quarantine/execute` or `POST /api/quarantine/restore/execute`).
3. **Sole Copy Invariant in UI:** The primary/root copy of every duplicate group is permanently locked (`disabled` checkbox) with clear visual badging (`Root / Keep`). The quarantine submit button is disabled if the user attempts to select all copies or zero copies.
4. **Anti-Overwrite UI Protection:** When restoring, if the destination already contains a file, the engine returns HTTP 409 (`STATUS_ERROR_DESTINATION_EXISTS`). The restore modal prominently highlights the conflict and disables the execution button.
5. **Zero Raw HTML Injection:** All filenames, filesystem paths, transaction IDs, and hashes are sanitized using `textContent` or strict HTML escaping to eliminate XSS vulnerabilities.

---

## 2. Duplicate Selection & Quarantine Workflow

```
[Duplicate Explorer]
        |
        v User checks redundant copy(ies)
[Selection Bar: "Selected 1 copy (34 B)"]
        |
        v User clicks "Quarantine Selected"
[Stage 1: POST /api/quarantine/preview]
        |
        v C Backend validates remaining copies >= 1, hashes, creates token
[Quarantine Preview Modal Appears]
        |-- Displays Group ID, SHA-256, Recoverable Storage, Remaining Copies (>=1)
        |-- Lists exact files, sizes, and original locations
        |-- Shows 300s TTL countdown timer
        |
        +---> [User clicks "Cancel"] ---> Modal closes, preview token discarded
        |
        +---> [User clicks "Confirm & Quarantine"]
                    |
                    v Button disabled, spinner shown (double-submit prevented)
              [Stage 2: POST /api/quarantine/execute]
                    |
                    v C Backend performs atomic MoveFileExW to .datavault_quarantine
              [Toast Notification: "Successfully quarantined X file(s)"]
              [Auto-refresh duplicate groups, stats, and quarantine history]
```

---

## 3. Quarantine History & Restoration Workflow

```
[Quarantine & Recovery Tab]
        |
        v Calls GET /api/quarantine/history
[Renders Transaction Table from C Manifest]
        |-- Filterable by status: ALL, ACTIVE, RESTORED, FAILED
        |-- Real-time instant search by filename, path, TX ID, SHA-256
        |-- Telemetry cards: Total, Active, Restored, Failed
        |
        v User clicks "Restore" on an ACTIVE transaction
[Stage 1: POST /api/quarantine/restore/preview]
        |
        v C Backend verifies quarantine file integrity & checks destination path
        |
        +---> [Destination Conflict Detected (HTTP 409)]
        |           |
        |           v Modal displays prominent warning:
        |             "Destination Conflict: File already exists at original location"
        |             "Confirm & Restore" button permanently DISABLED
        |
        +---> [Integrity Verified & Clean Destination]
                    |
                    v Restore Preview Modal Displays Details:
                      - Transaction ID, Original Path, File Size
                      - SHA-256 integrity confirmation
                    |
                    v User clicks "Confirm & Restore"
              [Stage 2: POST /api/quarantine/restore/execute]
                    |
                    v C Backend moves file back via MoveFileExW
              [Toast Notification: "File Restored Successfully"]
              [Auto-refresh manifest table and duplicate groups]
```

---

## 4. Complete REST API Specifications

### 4.1 Quarantine Preview (Stage 1)
- **Endpoint:** `POST /api/quarantine/preview`
- **Request Headers:** `Content-Type: application/json`
- **Request Body:**
  ```json
  {
    "group_id": 1,
    "file_ids": [2]
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "status": "success",
    "preview_id": "prev_6b05c1db_eaa46a731f14",
    "group_id": 1,
    "file_count": 1,
    "remaining_copies": 2,
    "estimated_recoverable_bytes": 34,
    "expires_in_seconds": 300,
    "confirmation_required": "QUARANTINE",
    "files": [
      {
        "file_id": 2,
        "file_size": 34,
        "filepath": "C:\\Data\\device_laptop\\doc2.txt"
      }
    ]
  }
  ```
- **Error Response (400 Bad Request):**
  ```json
  {
    "code": 400,
    "error": "Safety violation: at least one original copy must remain (cannot quarantine all copies in group)."
  }
  ```

### 4.2 Quarantine Execution (Stage 2)
- **Endpoint:** `POST /api/quarantine/execute`
- **Request Body:**
  ```json
  {
    "preview_id": "prev_6b05c1db_eaa46a731f14",
    "confirmation": "QUARANTINE"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "status": "success",
    "quarantined_count": 1,
    "transactions": [
      "tx_6b05c1db_dcd513a7db10"
    ]
  }
  ```

### 4.3 Quarantine Transaction History
- **Endpoint:** `GET /api/quarantine/history`
- **Response (200 OK):**
  ```json
  {
    "status": "success",
    "total_records": 1,
    "records": [
      {
        "transaction_id": "tx_6b05c1db_dcd513a7db10",
        "group_id": 1,
        "file_record_id": 2,
        "original_path": "C:\\Data\\device_laptop\\doc2.txt",
        "quarantine_path": "C:\\Data\\device_laptop\\.datavault_quarantine\\tx_6b05c1db_dcd513a7db10_doc2.txt",
        "original_filename": "doc2.txt",
        "file_size": 34,
        "sha256_hex": "b52384b8a7bb69b8955b04b8a6d9a046cac02ecb974bb4f962c2586a31e30d33",
        "quarantine_timestamp": 1795539007,
        "restore_timestamp": 0,
        "status": "ACTIVE"
      }
    ]
  }
  ```

### 4.4 Restore Preview (Stage 1)
- **Endpoint:** `POST /api/quarantine/restore/preview`
- **Request Body:**
  ```json
  {
    "transaction_id": "tx_6b05c1db_dcd513a7db10"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "status": "success",
    "preview_id": "prev_restore_6b05c1dc_081a29f8c0e1",
    "transaction_id": "tx_6b05c1db_dcd513a7db10",
    "original_path": "C:\\Data\\device_laptop\\doc2.txt",
    "file_size": 34,
    "expires_in_seconds": 300,
    "confirmation_required": "RESTORE"
  }
  ```
- **Error Response (409 Conflict):**
  ```json
  {
    "code": 409,
    "error": "Destination file already exists at original location. Overwrite prevented."
  }
  ```

### 4.5 Restore Execution (Stage 2)
- **Endpoint:** `POST /api/quarantine/restore/execute`
- **Request Body:**
  ```json
  {
    "preview_id": "prev_restore_6b05c1dc_081a29f8c0e1",
    "confirmation": "RESTORE"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "status": "success",
    "restored_path": "C:\\Data\\device_laptop\\doc2.txt"
  }
  ```

---

## 5. Security & Safety Protections

| Threat / Risk Vector | UI/Frontend Defense | Backend C11 Engine Defense |
|---|---|---|
| **Accidental Data Loss** | Root copy checkbox disabled; user cannot select all copies | `STATUS_ERROR_SOLE_COPY_REMAINING` ($O(1)$ check) |
| **Silent Overwrite on Restore** | HTTP 409 caught; modal displays warning; button disabled | `GetFileAttributesW()` checks destination nonexistence |
| **Quarantine Vault Tampering** | Error toast displayed; execution prevented | SHA-256 recalculated before restore; `STATUS_ERROR_FILE_TAMPERED` |
| **Double-Click / Re-submission** | Buttons disabled with loading spinner upon click | Single-use preview token invalidated (`STATUS_ERROR_PREVIEW_ALREADY_USED`) |
| **Stale Session Execution** | Dynamic 300s timer displayed in modal; auto-expires | Previews older than 300s rejected (`STATUS_ERROR_PREVIEW_EXPIRED`) |
| **XSS via Malicious Filename** | All DOM injections use `textContent` or `escapeHtml()` | Filenames serialized into strict JSON with escape characters |
| **Server Crash during Move** | Error toast surfaces server offline state | Write-ahead journal (`.log`) + atomic temp-file JSON replacement |

---

## 6. Manual Testing Instructions

To manually verify the end-to-end integration:

1. **Launch C Engine & HTTP Server:**
   ```powershell
   .\backend\build\duplicate-x.exe --server
   ```
2. **Open Dashboard:**
   Navigate to `http://127.0.0.1:8080` in Chrome, Edge, or Firefox.
3. **Queue Test Directories & Scan:**
   - Add `data\test_files\device_laptop` and `data\test_files\device_usb`.
   - Click **Run Deduplication Scan**.
   - Notice the telemetry cards update: 3 files scanned, 1 duplicate group detected, 3 B recoverable.
4. **Test Redundant Copy Selection:**
   - In Duplicate Explorer, expand Group #1.
   - Notice the first row (`Root / Keep`) has a disabled checkbox.
   - Check the duplicate copy (`renamed_notes.txt`).
   - Notice the toolbar updates: `Selected: 1 redundant copies (3 B)`.
   - Click **Quarantine Selected**.
5. **Verify Quarantine Modal & Execution:**
   - The Stage 1 modal opens with `GRP #1`, `1 valid copy will remain safe`, and a 5-minute countdown.
   - Click **Confirm & Quarantine**.
   - A success toast appears, the duplicate disappears from the explorer, and statistics refresh.
6. **Verify Quarantine History:**
   - Switch to the **Quarantine & Recovery** tab.
   - The transaction appears with status `ACTIVE`.
   - Filter by status or search by filename in the search bar.
7. **Test File Restoration:**
   - Click **Restore** next to the active transaction.
   - The Stage 1 restore modal appears with integrity confirmation.
   - Click **Confirm & Restore**.
   - The file is moved back to its original location, and the status changes to `RESTORED`.

---

## 7. Viva Voce Examination Questions & Answers (15 Technical Q&As)

### Q1: Why is quarantine confirmation implemented as a two-stage protocol instead of a single API request?
**Answer:** A two-stage protocol decouples request validation from physical state mutation. In Stage 1 (`/preview`), the engine evaluates safety invariants (checking that at least one copy remains, verifying that files still match their SHA-256 hash, and estimating recoverable bytes) without moving any data. In Stage 2 (`/execute`), the user must explicitly present the server-issued cryptographic token, preventing unintended or automated deletion.

### Q2: How does the frontend ensure that the last remaining copy of a file cannot be quarantined?
**Answer:** The frontend designates the primary copy of each group as the `Root / Keep` copy and permanently disables its checkbox. Furthermore, the selection logic recalculates `remaining = total_in_group - selected_count`. If `remaining < 1`, the "Quarantine Selected" button is automatically disabled. Even if a user attempts to bypass the UI, the C backend independently enforces `STATUS_ERROR_SOLE_COPY_REMAINING`.

### Q3: How are preview tokens protected against replay attacks or reuse?
**Answer:** Each preview token generated by the C backend is marked single-use. Once `POST /api/quarantine/execute` is called with a token, the backend flags `preview->is_used = true`. Any subsequent request with that token is rejected with `STATUS_ERROR_PREVIEW_ALREADY_USED`. Additionally, tokens carry an `expires_at` timestamp with a 5-minute TTL (`STATUS_ERROR_PREVIEW_EXPIRED`).

### Q4: What happens if a user tries to restore a file, but another file has already been created at that original path?
**Answer:** When `POST /api/quarantine/restore/preview` is called, the C engine checks the destination path using Win32 `GetFileAttributesW()`. If an existing file is detected, it returns HTTP 409 (`STATUS_ERROR_DESTINATION_EXISTS`). The frontend intercepts this response, displays a prominent red conflict alert, and permanently disables the "Confirm & Restore" button to prevent data clobbering.

### Q5: How does the dashboard prevent Cross-Site Scripting (XSS) when rendering arbitrary user filenames?
**Answer:** Untrusted input strings (such as filenames and directory paths scanned from disk) are never injected using `innerHTML`. The UI uses `document.createTextNode()`, `element.textContent`, or the dedicated `DashboardUI.escapeHtml()` sanitizer which transforms `&`, `<`, `>`, `"`, and `'` into safe HTML entities before template insertion.

### Q6: How does the frontend prevent double-submission if a user rapidly double-clicks the Quarantine or Restore button?
**Answer:** As soon as the confirm button is clicked, an in-flight flag (`isQuarantining` or `isRestoring`) is set to `true`, the button is disabled in the DOM, and its text is replaced with a progress indicator ("Relocating to Vault..."). Subsequent clicks are ignored until the asynchronous request completes.

### Q7: Where does the C backend store quarantined files, and why?
**Answer:** Quarantined files are moved into a hidden `.datavault_quarantine` directory located in the parent directory of the original file on the *same volume*. This ensures that the relocation is an atomic NTFS Master File Table (MFT) pointer update ($O(1)$) rather than a slow, risky cross-volume copy.

### Q8: What mechanism verifies that a file inside `.datavault_quarantine` has not been tampered with or corrupted before restoring it?
**Answer:** Before allowing restoration, `POST /api/quarantine/restore/preview` streams the quarantined file through the SHA-256 engine and compares the computed digest with the SHA-256 fingerprint stored in the manifest at quarantine time. If the hashes differ, it rejects the restore with `STATUS_ERROR_FILE_TAMPERED`.

### Q9: How does the dashboard update its data after a successful quarantine?
**Answer:** Upon receiving HTTP 200 from `POST /api/quarantine/execute`, the frontend calls `refreshAllData()` and `refreshQuarantineHistory()`. This triggers asynchronous calls to `GET /api/stats`, `GET /api/duplicates`, `GET /api/top-k`, and `GET /api/quarantine/history`, updating all metric cards, explorer accordions, and transaction tables without requiring a full page reload.

### Q10: How does the frontend handle server disconnection or backend crashes?
**Answer:** The API client in `frontend/js/api.js` wraps all `fetch()` calls in try/catch blocks. If the server is unreachable, it marks `state.isOnline = false`, updates the header status pill to `Server Offline`, displays `--` on metric cards, and presents friendly toast error messages instead of crashing.

### Q11: How does the history interface support searching and filtering across large numbers of transactions?
**Answer:** The history view stores the manifest records in JavaScript memory (`state.quarantineRecords`). When the user types into the search input or selects a status pill (ALL, ACTIVE, RESTORED, FAILED), the UI filters the in-memory array in $O(N)$ time and re-renders only the matching rows without making redundant server requests.

### Q12: Why are file record IDs (`file_id`) used in the preview request body instead of file paths?
**Answer:** File paths can contain Unicode characters, spaces, and escape sequences that are prone to encoding mismatches. The C engine assigns a unique 64-bit integer `record_id` to every scanned file. Using integer IDs in `file_ids: [2, 3]` provides unambiguous, compact, and collision-free references.

### Q13: What is the purpose of the 5-minute preview countdown timer displayed in the quarantine modal?
**Answer:** It provides visual transparency to the user regarding the token's validity window. If the timer reaches zero before confirmation, the modal informs the user that the token has expired and disables the confirm button, avoiding confusing server-side rejection errors.

### Q14: How does DUPLICATE-X format file sizes across the UI?
**Answer:** The `DashboardUI.formatBytes()` function dynamically divides byte counts by $1024^i$ and appends the appropriate binary suffix (`B`, `KB`, `MB`, `GB`, `TB`). It guarantees consistent and human-readable precision across cards, tables, and modals.

### Q15: Why is the frontend built with Vanilla JavaScript instead of frameworks like React, Vue, or Angular?
**Answer:** The C backend embeds a lightweight, non-blocking HTTP server (Winsock2). Serving a zero-dependency Vanilla JS, HTML5, and CSS3 frontend eliminates the need for Node.js runtimes, package managers, bundlers (Webpack/Vite), and heavy client-side virtual DOM overhead. The entire application runs out-of-the-box as a single compiled executable with instantaneous asset delivery.

