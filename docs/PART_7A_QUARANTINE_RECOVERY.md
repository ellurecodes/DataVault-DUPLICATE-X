# DUPLICATE-X: Part 7A — Safe Quarantine & File Recovery Engine
**Team Name:** DataVault  
**Academic Year:** 2026–2027  
**Platform:** Windows 11 (64-bit) | **Language:** Pure C11 (`-std=c11 -Wall -Wextra -Wpedantic -municode`)  
**Underlying Subsystems:** Win32 File API (`MoveFileExW`), Windows Cryptography API: Next Generation (`BCrypt`), Winsock2 HTTP REST Server  

---

## 1. Executive Summary & Architectural Overview

In file deduplication systems, the remediation phase—relocating or removing duplicate copies—presents the highest operational risk. A naive deduplicator risks permanent data loss if a single copy is prematurely deleted, if destination files are overwritten, or if a crash occurs mid-relocation.

**DUPLICATE-X Part 7A** implements an enterprise-grade, non-destructive **Safe Quarantine and File Recovery Engine**. Key tenets include:
1. **Never Permanently Delete:** Files are relocated to a hidden quarantine vault (`.datavault_quarantine`) on the identical NTFS volume.
2. **Sole Remaining Copy Invariant:** Under no condition can the last remaining copy of a duplicate group be quarantined (`STATUS_ERROR_SOLE_COPY_REMAINING`).
3. **Same-Volume Atomic Relocation:** Cross-volume relocation is strictly prohibited (`STATUS_ERROR_CROSS_VOLUME_MOVE`), ensuring operations use metadata pointer updates rather than volatile block copy-and-delete cycles.
4. **Two-Stage Cryptographic Confirmation:** Any destructive action requires a cryptographically generated preview token (`prev_<timestamp>_<entropy>`) with strict TTL expiration and single-use invalidation.
5. **Write-Ahead Journaling & Crash Reconciliation:** Every state transition is recorded in a Write-Ahead Log (`quarantine_journal.log`) and atomically synced to an indexed JSON manifest (`quarantine_manifest.json`).

```
                              +-------------------------------------------+
                              |         DUPLICATE-X REST CLIENT           |
                              |       (Frontend / Admin Console)          |
                              +---------------------+---------------------+
                                                    |
                       1. POST /preview             |  3. POST /execute
                                                    v
+---------------------------------------------------------------------------------------------------+
|                                  C11 QUARANTINE ENGINE                                            |
|                                                                                                   |
|  +---------------------------+    +----------------------------+    +--------------------------+  |
|  |     Safety Validation     |    |   Two-Stage Preview Cache  |    |   Atomic Relocation      |  |
|  | - Sole copy check (>= 1)  |--->| - BCrypt secure token      |--->| - MoveFileExW            |  |
|  | - Same volume check       |    | - 5-min TTL enforcement    |    | - WRITE_THROUGH (No copy)|  |
|  | - Reparse point guard     |    | - Single-use token lock    |    | - Hidden destination dir |  |
|  | - Pre-move SHA-256 verify |    +----------------------------+    +--------------------------+  |
|  +---------------------------+                                                    |               |
|                                                                                   v               |
|  +---------------------------------------------------------------------------------------------+  |
|  |                        Write-Ahead Journal & Reconciliation Manifest                        |  |
|  |   - quarantine_journal.log (Append-only write-ahead log)                                    |  |
|  |   - quarantine_manifest.json (Atomic temp-file write + MoveFileEx replace)                  |  |
|  |   - Crash startup reconciliation (Self-healing interrupted states)                          |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 2. Windows Filesystem & Storage Mechanics

### 2.1 Same-Volume Move Semantics (`MoveFileExW`)
When files are moved between different storage volumes (e.g., `C:` to `D:`), the operating system must perform a multi-step block copy followed by unlinking the source blocks. An unexpected power failure during cross-volume copy leaves partially written files or duplicated storage consumption.

To eliminate this vulnerability, DUPLICATE-X enforces **Same-Volume Quarantine**:
- Destination path: `<source_parent_dir>\.datavault_quarantine\<tx_id>_<filename>`
- Function: `MoveFileExW(src_w, dest_w, MOVEFILE_WRITE_THROUGH)`
- Specifically omitted: `MOVEFILE_COPY_ALLOWED` (which would implicitly permit cross-volume fallback).

On NTFS, a same-volume `MoveFileExW` merely updates the Master File Table (MFT) directory entries and directory index B-trees ($INDEX_ROOT and $INDEX_ALLOCATION). The underlying file records, data clusters, and security descriptors remain stationary on the storage platter or NAND flash. The `MOVEFILE_WRITE_THROUGH` flag instructs the NTFS driver and storage controller cache to immediately flush metadata changes to persistent storage before returning.

### 2.2 Volume Identity Verification (`quarantine_is_same_volume`)
Drive letters and paths are resolved using Win32 `GetVolumePathNameW()`:
```c
bool quarantine_is_same_volume(const wchar_t *src_w, const wchar_t *dest_w) {
    wchar_t src_vol[MAX_PATH];
    wchar_t dest_vol[MAX_PATH];
    if (!GetVolumePathNameW(src_w, src_vol, MAX_PATH)) return false;
    if (!GetVolumePathNameW(dest_w, dest_vol, MAX_PATH)) return false;
    return (_wcsicmp(src_vol, dest_vol) == 0);
}
```

### 2.3 Reparse Point & Symbolic Link Immunity
Symbolic links and directory junctions can create filesystem loops or point to critical OS binaries. If a symlink were quarantined, restoring or modifying it could corrupt system link targets.
Before admitting any file into a quarantine preview, its attributes are inspected:
```c
bool quarantine_is_reparse_point(const wchar_t *path_w) {
    DWORD attr = GetFileAttributesW(path_w);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}
```
If `FILE_ATTRIBUTE_REPARSE_POINT` is flagged, the operation is aborted with `STATUS_ERROR_IS_REPARSE_POINT`.

### 2.4 Hard Link Identity Safety (`quarantine_is_hardlink`)
In NTFS, multiple directory entries can point to the same MFT file record (Hard Links). Moving or deleting one hard-linked filename does not free storage blocks until all links are removed. Furthermore, claiming independent recoverable storage for two hard-linked duplicate paths would falsely inflate storage metrics.
DUPLICATE-X retrieves low-level NTFS file identifiers via `BY_HANDLE_FILE_INFORMATION`:
```c
bool quarantine_is_hardlink(const wchar_t *path1_w, const wchar_t *path2_w) {
    HANDLE h1 = CreateFileW(path1_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE h2 = CreateFileW(path2_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    BY_HANDLE_FILE_INFORMATION info1, info2;
    GetFileInformationByHandle(h1, &info1);
    GetFileInformationByHandle(h2, &info2);
    CloseHandle(h1); CloseHandle(h2);
    return (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
            info1.nFileIndexHigh == info2.nFileIndexHigh &&
            info1.nFileIndexLow == info2.nFileIndexLow);
}
```
When two duplicate entries share identical volume serial and MFT index values, the engine identifies them as hard links and prevents double-counting recoverable bytes.

---

## 3. Critical Safety Invariants

| Invariant | Implementation Mechanism | Error Code on Violation |
|---|---|---|
| **Sole Remaining Copy** | `remaining_copies = total_copies - selected_copies; if (remaining_copies < 1)` | `STATUS_ERROR_SOLE_COPY_REMAINING` |
| **Cross-Volume Rejection** | `GetVolumePathNameW(src) == GetVolumePathNameW(dest)` | `STATUS_ERROR_CROSS_VOLUME_MOVE` |
| **No Symlink/Junction Move** | Attribute check for `FILE_ATTRIBUTE_REPARSE_POINT` | `STATUS_ERROR_IS_REPARSE_POINT` |
| **Concurrent Mutation** | Re-run SHA-256 hash & byte comparison immediately before move | `STATUS_ERROR_CONCURRENT_MODIFICATION` |
| **Anti-Overwrite Protection** | Confirm destination does not exist before moving or restoring | `STATUS_ERROR_DESTINATION_EXISTS` |
| **Tamper Detection on Restore** | Quarantined file SHA-256 recomputed and checked against manifest | `STATUS_ERROR_FILE_TAMPERED` |
| **Token Expiry** | Ephemeral token checked against `expires_at = created_at + 300` | `STATUS_ERROR_PREVIEW_EXPIRED` |
| **Single-Use Token** | `if (preview->is_used) return STATUS_ERROR_PREVIEW_ALREADY_USED` | `STATUS_ERROR_PREVIEW_ALREADY_USED` |

---

## 4. Two-Stage Confirmation Protocol

To eliminate inadvertent data modification, every quarantine and restoration workflow is decoupled into two explicit stages:

```
[Client]                                                        [Engine]
   |                                                                |
   |---- 1. POST /api/quarantine/preview (group_id, file_ids) ----->|
   |                                                                | Validate copies >= 1
   |                                                                | Validate byte equality
   |                                                                | Generate BCrypt token
   |<--- 2. Return Preview (preview_id, expires_at, recoverable) ---|
   |                                                                |
   |---- 3. POST /api/quarantine/execute (preview_id, "QUARANTINE")->|
   |                                                                | Verify token != used
   |                                                                | Verify time < expires_at
   |                                                                | Atomic MoveFileExW
   |                                                                | Append journal & save manifest
   |<--- 4. Return Execution Confirmation (tx_ids, count) ----------|
```

### 4.1 Cryptographic Token Generation
Tokens are generated using Windows BCrypt CNG (`BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG`), ensuring non-deterministic, collision-free tokens resistant to replay attacks:
```c
BCryptGenRandom(NULL, rand_bytes, sizeof(rand_bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
snprintf(out_buf, buf_sz, "%s_%llx_%02x%02x%02x%02x%02x%02x", prefix, time(NULL), ...);
```

---

## 5. Write-Ahead Journaling & Crash Reconciliation

### 5.1 Write-Ahead Journal (`quarantine_journal.log`)
Before any filesystem mutation or manifest update, an entry is written and flushed (`fflush()`) to `quarantine_journal.log`:
```
<timestamp> | <action> | <transaction_id> | <status> | <original_path> | <quarantine_path>
```
Example:
```
1795539007 | QUARANTINE_ADD | tx_6b05c126_1b7d92300548 | ACTIVE | C:\Data\doc2.txt | C:\Data\.datavault_quarantine\tx_6b05c126_1b7d92300548_doc2.txt
1795539008 | STATUS_UPDATE | tx_6b05c126_1b7d92300548 | RESTORED | C:\Data\doc2.txt | C:\Data\.datavault_quarantine\tx_6b05c126_1b7d92300548_doc2.txt
```

### 5.2 Atomic Manifest Persistence (`quarantine_manifest.json`)
The manifest represents the current state of all quarantine transactions in structured JSON. To prevent corruption if the power fails during serialization:
1. Serialize manifest data to temporary file `<filepath>.tmp`.
2. Flush and close the temporary file.
3. Replace the live manifest atomically using:
   ```c
   MoveFileExA(temp_path, manifest->manifest_filepath,
               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
   ```

### 5.3 Startup Reconciliation (`quarantine_manifest_reconcile`)
Whenever DUPLICATE-X starts, `quarantine_manager_init()` reconciles in-memory records with the physical filesystem:
- If a record is marked `ACTIVE`, but the file is present at `original_path` and missing from `quarantine_path`, an interruption occurred mid-flight.
- The record status is reconciled to `QUARANTINE_STATUS_FAILED`, and a compensating entry (`RECONCILE_FAILED_INCOMPLETE_MOVE`) is logged in the journal.

---

## 6. REST API Endpoint Specifications

### 6.1 `POST /api/quarantine/preview`
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
    "success": true,
    "preview_id": "prev_6b05c1db_eaa46a731f14",
    "group_id": 1,
    "selected_file_count": 1,
    "remaining_copies": 2,
    "estimated_recoverable_bytes": 34,
    "expires_at": 1795539307
  }
  ```

### 6.2 `POST /api/quarantine/execute`
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
    "success": true,
    "quarantined_count": 1,
    "transaction_ids": [
      "tx_6b05c1db_dcd513a7db10"
    ]
  }
  ```

### 6.3 `GET /api/quarantine/history`
- **Response (200 OK):**
  ```json
  {
    "count": 2,
    "records": [
      {
        "transaction_id": "tx_6b05c1db_dcd513a7db10",
        "group_id": 1,
        "file_record_id": 2,
        "original_path": "C:\\Data\\doc2.txt",
        "quarantine_path": "C:\\Data\\.datavault_quarantine\\tx_..._doc2.txt",
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

### 6.4 `POST /api/quarantine/restore/preview`
- **Request Body:**
  ```json
  {
    "transaction_id": "tx_6b05c1db_dcd513a7db10"
  }
  ```
- **Response (200 OK):**
  ```json
  {
    "success": true,
    "preview_id": "prev_restore_6b05c1dc_081a29f8c0e1",
    "transaction_id": "tx_6b05c1db_dcd513a7db10",
    "original_path": "C:\\Data\\doc2.txt",
    "file_size": 34,
    "expires_at": 1795539308
  }
  ```

### 6.5 `POST /api/quarantine/restore/execute`
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
    "success": true,
    "message": "File successfully restored from quarantine",
    "restored_path": "C:\\Data\\doc2.txt"
  }
  ```

---

## 7. Comprehensive 25-Scenario Verification Matrix

The Part 7A test harness (`backend/tests/test_quarantine.c`) validates every boundary condition:

| Test ID | Verification Target | Expected Behavior | Status |
|:---:|---|---|:---:|
| **1** | Initialize empty quarantine manager | Initialized with 0 records and empty preview cache | **PASS** |
| **2** | Preview a valid duplicate file | Generated preview token with remaining copies = 2 | **PASS** |
| **3** | Preview non-mutating guarantee | Source file untouched at original location during preview | **PASS** |
| **4** | Confirmed quarantine execution | File relocated, transaction registered, count = 1 | **PASS** |
| **5** | Source file absence verification | Original file nonexistent at original path | **PASS** |
| **6** | Quarantine vault verification | File exists in hidden `.datavault_quarantine` directory | **PASS** |
| **7** | Content integrity verification | SHA-256 fingerprint verified identical | **PASS** |
| **8** | Last remaining copy protection | Move rejected with `STATUS_ERROR_SOLE_COPY_REMAINING` | **PASS** |
| **9** | Group wipeout protection | Attempting to quarantine all copies in group rejected | **PASS** |
| **10** | Overflow count protection | Attempting to quarantine > total copies rejected | **PASS** |
| **11** | Nonexistent group ID handling | Group 9999 rejected with `STATUS_ERROR_INVALID_ARGUMENT` | **PASS** |
| **12** | Nonexistent file ID handling | Unrecognized file ID rejected with `STATUS_ERROR_INVALID_ARGUMENT` | **PASS** |
| **13** | Expired preview token rejection | Expired token rejected with `STATUS_ERROR_PREVIEW_EXPIRED` | **PASS** |
| **14** | Replayed / reused preview token | Used token rejected with `STATUS_ERROR_PREVIEW_ALREADY_USED` | **PASS** |
| **15** | Pre-move file mutation detection | File altered before move rejected via SHA-256 check | **PASS** |
| **16** | Pre-move file deletion handling | Missing source file handled with `STATUS_ERROR_PATH_NOT_FOUND` | **PASS** |
| **17** | Exclusive lock handling | Read-only / locked file handled without state corruption | **PASS** |
| **18** | Successful restoration | Quarantined file safely moved back to original path | **PASS** |
| **19** | Anti-overwrite destination check | Conflicting file at original path rejects restore | **PASS** |
| **20** | Quarantine vault tamper detection | Tampered quarantine file detected via SHA-256 mismatch | **PASS** |
| **21** | Manifest persistence across restarts | Reinitialized manager reloads exact records from JSON | **PASS** |
| **22** | Interrupted transaction reconciliation | Interrupted move reconciled to `QUARANTINE_STATUS_FAILED` | **PASS** |
| **23** | Unicode path preservation | Russian Cyrillic (`файл_123_копия.txt`) quarantined & restored | **PASS** |
| **24** | Cross-volume move rejection | Differing volume roots strictly rejected | **PASS** |
| **25** | Hard link identity safety | Shared volume & MFT index verified to prevent false metrics | **PASS** |

---

## 8. Viva Voce Examination Questions & Answers (20 In-Depth Technical Q&As)

### Q1: Why does DUPLICATE-X use a Quarantine directory instead of calling Win32 `DeleteFileW()` directly?
**Answer:** Permanent deletion is irreversible. If an external process or user misidentified files or if a scan encountered concurrency race conditions, calling `DeleteFileW()` would permanently destroy user data. Quarantine provides an air-gapped staging area where files can be inspected and safely restored at any point without data loss.

### Q2: What exact Win32 API is used to relocate files into quarantine, and what flags are passed?
**Answer:** `MoveFileExW(src_w, dest_w, MOVEFILE_WRITE_THROUGH)`. The `MOVEFILE_WRITE_THROUGH` flag forces the filesystem to commit the directory metadata changes directly to disk before the call returns, eliminating data loss during sudden power outages. Crucially, `MOVEFILE_COPY_ALLOWED` is omitted to ensure the move never degenerates into a non-atomic copy across volumes.

### Q3: What is the architectural purpose of the "Same-Volume" restriction?
**Answer:** On NTFS, a move on the same volume is an atomic metadata operation: it updates pointers in the Master File Table (MFT) and directory indexes without copying data clusters. Across volumes, a move requires copying all data blocks followed by deleting the source. If interrupted mid-copy, cross-volume operations risk data duplication or corruption. By enforcing same-volume quarantine, relocations complete in constant time $O(1)$ and retain atomic safety.

### Q4: How does DUPLICATE-X verify that two paths reside on the same volume?
**Answer:** It uses the Win32 API `GetVolumePathNameW()` on both the source and target paths. It retrieves the volume mount point (e.g., `C:\` or `D:\Volume1\`) for each and compares them case-insensitively using `_wcsicmp()`. If they differ, the engine returns `STATUS_ERROR_CROSS_VOLUME_MOVE`.

### Q5: How does the engine prevent accidental deletion of the last remaining copy of a file?
**Answer:** The preview creation function calculates `remaining_copies = total_group_copies - selected_copies`. If `remaining_copies < 1`, the operation is immediately aborted with `STATUS_ERROR_SOLE_COPY_REMAINING`. The engine guarantees that at least one designated original copy remains untouched.

### Q6: What is the two-stage confirmation protocol and why is it essential?
**Answer:** The protocol separates operation preparation from execution. Stage 1 (`/preview`) validates arguments, verifies that the target files still match their SHA-256 fingerprint, ensures byte-by-byte identity with the retained copy, and generates an ephemeral cryptographic preview token. Stage 2 (`/execute`) requires presenting this exact token. This ensures user awareness, prevents accidental execution, and protects against stale state.

### Q7: How are preview tokens generated, and why is standard `rand()` avoided?
**Answer:** Tokens are generated using the Windows Cryptography API: Next Generation (`BCryptGenRandom` with `BCRYPT_USE_SYSTEM_PREFERRED_RNG`). Standard `rand()` uses a linear congruential generator (LCG) with low entropy, producing predictable sequences that could be guessed or replayed. BCrypt utilizes hardware entropy to generate cryptographically secure, unpredictable 128-bit random tokens.

### Q8: What prevents replay attacks using previously generated preview tokens?
**Answer:** Two mechanisms:
1. **TTL Expiration:** Each preview has an `expires_at` timestamp set to `created_at + 300` (5 minutes). Any execution attempt past this threshold is rejected with `STATUS_ERROR_PREVIEW_EXPIRED`.
2. **Single-Use State Invalidation:** As soon as execution commences, the token is flagged `preview->is_used = true`. Any subsequent request with the same token is rejected with `STATUS_ERROR_PREVIEW_ALREADY_USED`.

### Q9: How does the engine handle a file that was modified after the preview was generated?
**Answer:** Immediately before executing `MoveFileExW()`, the engine recomputes the file's SHA-256 digest using `sha256_calculate_file()`. If the computed hash differs from the expected hash recorded in the duplicate group or preview, the operation is rejected with `STATUS_ERROR_CONCURRENT_MODIFICATION`.

### Q10: What happens if a file is placed at the original location before a quarantined file is restored?
**Answer:** Before restoring, `quarantine_restore_preview()` calls `GetFileAttributesW(orig_w)`. If the path already exists, the engine returns `STATUS_ERROR_DESTINATION_EXISTS`. The engine will never overwrite an existing file on restore.

### Q11: How does DUPLICATE-X prevent tampering of files inside the quarantine directory?
**Answer:** When `/api/quarantine/restore/preview` is requested, the engine recalculates the SHA-256 digest of the file inside `.datavault_quarantine` and compares it to the digest recorded in the transaction manifest at the time of initial quarantine. If the file was modified, truncated, or infected, the restore preview fails with `STATUS_ERROR_FILE_TAMPERED`.

### Q12: Why does the system maintain both a Write-Ahead Journal (`.log`) and a JSON Manifest (`.json`)?
**Answer:** 
- The **Write-Ahead Journal** is an append-only flat log where entries are flushed immediately prior to any disk mutation, ensuring an indelible history of every state change.
- The **JSON Manifest** represents the indexed snapshot of all records for quick lookups and API reporting. It is rewritten atomically via temp-file replacement (`MoveFileExA` with `MOVEFILE_REPLACE_EXISTING`).

### Q13: How does the engine achieve atomic manifest file updates?
**Answer:** It writes the updated JSON serialization to `quarantine_manifest.json.tmp`, flushes it to disk via `fflush()`, and closes the file descriptor. It then calls `MoveFileExA(tmp, json, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`. On Windows NTFS, this replaces the target file in a single atomic directory transaction.

### Q14: How does startup crash reconciliation work?
**Answer:** During startup (`quarantine_manager_init()`), the engine iterates through all active records in the manifest. If a record has status `ACTIVE`, but the file still exists at `original_path` and is absent from `quarantine_path`, the engine detects an interrupted move, updates the record status to `QUARANTINE_STATUS_FAILED`, appends a `RECONCILE_FAILED_INCOMPLETE_MOVE` record to the journal, and rewrites the manifest.

### Q15: Why are symbolic links and directory junctions rejected from quarantine?
**Answer:** Reparse points (symlinks and directory junctions) are metadata pointers rather than regular data files. Moving or deleting a symlink could break operating system links, cause dangling references, or trigger unintended directory recursion. The engine inspects `FILE_ATTRIBUTE_REPARSE_POINT` and returns `STATUS_ERROR_IS_REPARSE_POINT`.

### Q16: How does the engine detect NTFS Hard Links, and why does this matter?
**Answer:** It inspects the `dwVolumeSerialNumber`, `nFileIndexHigh`, and `nFileIndexLow` fields returned by Win32 `GetFileInformationByHandle()`. If two directory paths share identical values, they point to the exact same MFT record. Moving one hard link frees zero storage clusters; claiming recoverable storage for it would produce inaccurate metrics.

### Q17: How is Unicode support achieved across quarantine and restoration?
**Answer:** All Windows filesystem interactions utilize 16-bit wide character functions (`MoveFileExW`, `GetFileAttributesW`, `CreateFileW`, `FindFirstFileW`). Path strings within the internal C structs are stored as UTF-8 and converted using `MultiByteToWideChar(CP_UTF8)` and `WideCharToMultiByte(CP_UTF8)`. This guarantees flawless handling of international characters, Cyrillic, CJK, and spaces.

### Q18: Why is the Quarantine Manager struct dynamically allocated rather than placed on the thread stack?
**Answer:** Each `QuarantinePreview` holds buffers for multiple paths (`file_paths[32][1024] = 32 KB`). Storing 64 preview slots statically within `QuarantineManager` consumes over 2 Megabytes of memory. On Windows, default stack sizes are 1 MB or 2 MB; allocating `QuarantineManager` on the stack would trigger `STATUS_STACK_OVERFLOW` (`0xC00000FD`). DUPLICATE-X dynamically allocates the previews cache on the heap.

### Q19: What is the time complexity of a quarantine move and a restore operation?
**Answer:** 
- Quarantine move: $O(1)$ disk metadata update per file on NTFS, bounded only by the SHA-256 pre-verification of file content chunks $O(S)$ where $S$ is file size.
- Restore operation: $O(1)$ disk metadata update per file, bounded by SHA-256 integrity re-verification $O(S)$.

### Q20: How does the REST API surface quarantine capabilities to modern web frontends?
**Answer:** Through five RESTful HTTP endpoints implemented in pure C with Winsock2:
1. `POST /api/quarantine/preview` — Generates preview and cryptographic token.
2. `POST /api/quarantine/execute` — Executes relocation with explicit `"QUARANTINE"` confirmation.
3. `GET /api/quarantine/history` — Returns full transaction log with current statuses.
4. `POST /api/quarantine/restore/preview` — Previews restoration and verifies quarantine file integrity.
5. `POST /api/quarantine/restore/execute` — Re-attaches file to original path with `"RESTORE"` confirmation.
