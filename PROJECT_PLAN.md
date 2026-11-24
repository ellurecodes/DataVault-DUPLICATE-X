# DUPLICATE-X: Intelligent Multi-Device File Deduplication & Storage Optimization System
## Comprehensive Project Master Plan (EL 2026–2027)

---

### Project Identity & Academic Context
* **Team Name:** DataVault
* **Project Name:** DUPLICATE-X
* **Tagline:** *Smarter Storage. Zero Duplicate Waste.*
* **Full Title:** Intelligent Multi-Device File Deduplication and Storage Optimization System
* **Course:** Data Structures and Applications Experiential Learning (EL) Project
* **Academic Year:** 2026–2027

---

## 1. Executive Summary & Problem Statement

Modern computing environments span multiple storage volumes, external drives, cloud sync folders, and removable media. Users frequently accumulate duplicate files—photos, documents, system installers, and video assets—across disparate directories under differing filenames (e.g., `report.pdf`, `report_copy(1).pdf`, `backup_final.pdf`). 

Traditional duplicate cleaners rely solely on naive filename or file-size checks, leading to both false positives (destroying unique files with identical sizes) and uncollected duplicates. Conversely, naive full-file cryptographic hashing across entire drives causes severe I/O bottlenecks.

**DUPLICATE-X** solves these challenges using a high-performance **Modular C11 Engine** implementing custom classic data structures and an intelligent **four-tier deduplication pipeline**:
1. **Tier 1 (Metadata & Size Pre-filtering):** Immediate elimination of unique file sizes ($O(1)$ lookup). *Crucial Rule:* File size is merely an initial filter; equal file sizes do **not** mean files are duplicates.
2. **Tier 2 (Partial/Chunk Hash Sampling):** Quick validation on head/tail chunks of files with identical sizes to filter out distinct large files quickly. *Crucial Rule:* Sample chunk comparisons can eliminate candidate pairs quickly, but can **never** confirm duplicates.
3. **Tier 3 (Full SHA-256 Cryptographic Hashing):** Full cryptographic fingerprint calculation across the entire file content, guaranteeing content identity regardless of filename, path, or timestamp.
4. **Tier 4 (Byte-by-Byte Verification):** Zero-collision guarantee before any destructive or relocation action is committed.

All core algorithms, memory management, and data structures are constructed from scratch in portable C11. Platform-specific operations (e.g., Windows Win32 file traversal, Windows BCrypt API) are strictly isolated in `backend/src/platform/windows/`. The frontend provides a modern dark-navy responsive dashboard purely for telemetry, visualization, and user confirmation.

---

## 2. Development Environment Inspection & Findings

An automated diagnostic scan of the host environment was performed:
* **Operating System:** Microsoft Windows 11 / Windows NT 10.0 (Build 26300, 64-bit).
* **Execution Shell:** Windows PowerShell / cmd.exe.
* **Working Directory:** `C:\Users\visha\OneDrive\Desktop\DataVault`.
* **Installed Development Editors:** Microsoft Visual Studio Code (`C:\Users\visha\AppData\Local\Programs\Microsoft VS Code`), Antigravity IDE.
* **Package Management Available:** Windows Package Manager (`winget.exe`).
* **Toolchain Inspection:**
  - `gcc`, `clang`, `cl`: Not found in active system `PATH`.
  - `git`: Not found in active system `PATH`.
  - `node`, `npm`: Not found in active system `PATH`.
* **Compiler Search on Winget:**
  - Exact Package ID verified: `BrechtSanders.WinLibs.POSIX.UCRT` (MinGW-w64 with GCC 16.2.0, POSIX threads, UCRT runtime).
  - Recommended command: `winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT`
  - *Status:* Awaiting explicit user permission before initiating installation.

---

## 3. Technology Stack & Justification

| Layer | Technology | Technical Justification |
| :--- | :--- | :--- |
| **Core Engine** | Portable C (C11 Standard) | Maximum execution speed, explicit memory management, zero runtime overhead, and strict adherence to college DSA academic criteria. |
| **Platform Layer** | Windows Win32 & BCrypt APIs | Native directory enumeration (`FindFirstFileW`), file attribute queries, and hardware-accelerated SHA-256 hashing via Windows Cryptography API: Next Generation (CNG / `bcrypt.h`), alongside a portable C11 software SHA-256 fallback. |
| **Internal Hashing vs Content Hashing** | SHA-256 vs FNV-1a / Murmur3 | **Strict distinction:** SHA-256 calculates the actual cryptographic file content fingerprint. FNV-1a is solely used for in-memory Hash Table bucket indexing. |
| **Data Structures** | Custom C11 Implementations | Core: Hash Table with Separate Chaining, Dynamic Linked Lists, Binary Min Heap, and Trie. Optional Advanced: Graph for multi-device cross-redundancy mapping. |
| **IPC / Local Bridge** | Native C HTTP Server (Winsock) | Eliminates heavyweight runtimes. The C executable runs a lightweight non-blocking loop serving JSON REST endpoints over `localhost:8080`. |
| **Persistence** | CSV / JSON Journaling & Manifests | Human-readable, robust against sudden crashes, and easily auditable for academic evaluation. |
| **Frontend UI** | HTML5, Modern CSS3, Pure JavaScript | Zero-dependency browser interface. Dark navy and royal blue palette, responsive metrics, interactive quarantine controls. Zero duplicate detection logic in JS. |

---

## 4. System Boundaries & Core Functional Scope

### In-Scope Functionalities:
1. **Multi-Folder / Multi-Device Scanning:** Support scanning arbitrary mounted volumes (e.g., `C:\`, `D:\`, external USB drives `E:\`).
2. **Metadata Harvesting:** Extraction of file paths, file sizes in bytes, creation/modification timestamps, and volume identifiers.
3. **Four-Tier Deduplication Pipeline:** Size grouping $\to$ Chunk sample check $\to$ Full SHA-256 $\to$ Byte-by-byte verification.
4. **Custom Data Structures Core:**
   - Hash Table with Separate Chaining for $O(1)$ average hash-to-duplicate lookups.
   - Linked Lists for maintaining flexible, dynamic duplicate clusters.
   - Binary Min Heap for ranking duplicate groups by recoverable storage space.
   - Trie for fast path prefix indexing, folder filtering, and common ancestor discovery.
   - Graph (Optional Advanced Module) for cross-volume dependency and duplicate relationship mapping.
5. **Storage Wastage Calculation:** Exact byte-level recoverable space computation $( (\text{count} - 1) \times \text{size} )$.
6. **Volume-Specific Quarantine Management:** 
   - Moving duplicates to volume-local isolation vaults (e.g., `C:\.datavault_quarantine`, `D:\.datavault_quarantine`) allowing instant, atomic intra-volume moves.
   - For cross-volume transfers, a verified recovery-safe procedure is executed: copy to vault, verify SHA-256 hash match against source, then remove source.
7. **Restoration Facility:** Restoring quarantined files back to their exact original absolute paths with preserved metadata.
8. **Copy Count Invariant:** Dynamically re-checking remaining copy count before moving any file, strictly guaranteeing that $\ge 1$ copy (the root copy) is permanently preserved.

---

## 5. Security, File Safety & Risk Management Policy

| Risk | Likelihood | Impact | Built-in Mitigation Mechanism |
| :--- | :--- | :--- | :--- |
| **Accidental Data Loss** | Medium | Critical | **Volume-Specific Quarantine:** Files are never deleted permanently by default; they are moved to volume-local `.datavault_quarantine` or transferred via copy-verify-delete. |
| **Last-Copy Deletion** | Low | Critical | **Root Copy Lock & Dynamic Recheck:** The algorithm marks the primary copy as `IMMUTABLE_ROOT`. Before any quarantine operation, the backend verifies active remaining copies $\ge 1$. |
| **Cross-Volume Move Failures** | Medium | High | **No False Atomic Move Promises:** Cross-volume moves are explicitly handled via copy $\to$ SHA-256 verification $\to$ source deletion, or isolated per-volume. |
| **False Duplicate Declaration** | Low | Critical | **Multi-Tier Safeguard:** Equal size $\neq$ duplicate. Chunk match $\neq$ duplicate. Full SHA-256 content fingerprinting and Tier 4 byte-by-byte check are mandatory. |
| **Symlink / Junction Loops** | Medium | High | **Reparse Point Bypassing:** Inspects `FILE_ATTRIBUTE_REPARSE_POINT` to avoid infinite traversal cycles. |
| **System Folder Infiltration** | High | Critical | **Trie Prefix Blacklist:** Rejects critical OS paths (`C:\Windows`, `System Volume Information`) in $O(L)$ time. |
| **Concurrent File Modification** | Low | Medium | **Pre-Action Verification:** Re-verifies file size and timestamps immediately before staging. |
