# DUPLICATE-X: Intelligent Multi-Device File Deduplication & Storage Optimization System
### Developed by Team DataVault | Academic Year 2026–2027

---

## 1. Project Overview & Objective

**DUPLICATE-X** is an intelligent file deduplication and storage optimization system designed to identify and manage identical files across multiple storage volumes (e.g., local SSDs, secondary internal hard drives, USB external drives, SD cards, and shared network directories). 

Even when duplicate files have distinct filenames, differing folder paths, or separate timestamps, DUPLICATE-X detects exact duplicates using a high-performance **Four-Tier Deduplication Pipeline** implemented in pure C11. 

### Key Objectives:
* **Academic Rigor:** Implement custom classic data structures (Hash Table with Separate Chaining, Dynamic Linked Lists, Binary Min Heap, and Trie) in pure C11 without relying on external libraries or built-in container abstractions.
* **Algorithmic Efficiency:** Eliminate non-duplicates rapidly via file-size pre-filtering ($O(1)$) and chunk sampling, reserving full SHA-256 cryptographic hashing and Tier 4 byte-by-byte direct comparisons only for true duplicate candidates.
* **Strict Data Safety:** Enforce a volume-local quarantine model, protect the original "root copy" from accidental deletion, and guarantee that the last copy of a file can never be lost.
* **Modern Dashboard:** Provide an intuitive dark navy and royal blue user interface for real-time scan telemetry and user-confirmed file isolation.

---

## 2. Technology Stack

* **Backend & DSA Core:** Pure C (C11 Standard: `-std=c11 -Wall -Wextra -Wpedantic`).
* **Platform Layer:** Windows Win32 APIs (`FindFirstFileW`) and Windows Cryptography API: Next Generation (CNG / `bcrypt.h`) with a portable C11 software SHA-256 fallback.
* **Internal Hashing vs. Content Hashing:**
  - **SHA-256:** Cryptographic content fingerprint over the entire file payload.
  - **FNV-1a / Murmur3:** Internal in-memory hash table bucket indexing functions.
* **Inter-Process Communication:** Embedded lightweight non-blocking HTTP REST server in C (Winsock2) communicating via standard JSON.
* **Frontend:** Modern HTML5, Responsive CSS3, Vanilla JavaScript (presentation layer only).

---

## 3. Project Folder Structure

```text
DUPLICATE-X/
|
|-- backend/
|   |-- include/                    # C Header files (.h)
|   |   |-- config.h                # Project configuration, branding & limits
|   |   |-- status.h                # Unified status and error codes
|   |   |-- engine.h                # Core engine lifecycle interface
|   |   |-- types.h                 # Domain types (files, duplicate clusters)
|   |   |-- ds/                     # Custom data structure headers
|   |   |-- core/                   # Deduplication pipeline & scanner headers
|   |   |-- platform/               # Platform abstraction headers
|   |   |-- api/                    # HTTP server & JSON serializer headers
|   |
|   |-- src/                        # C Source files (.c)
|   |   |-- main.c                  # Main executable entry point & banner
|   |   |-- core/                   # Engine implementation
|   |   |   |-- engine.c
|   |   |-- ds/                     # Custom data structures
|   |   |-- platform/               # Windows-specific I/O & BCrypt
|   |   |-- api/                    # Server & IPC implementation
|   |
|   |-- tests/                      # Unit and smoke test suites
|   |   |-- smoke_test.c
|   |
|   |-- build/                      # Compiled binary targets (.exe, .o)
|
|-- frontend/                       # Modern Dark Navy Dashboard
|   |-- assets/                     # Icons, SVGs, logos
|   |-- css/                        # Responsive CSS styling
|   |-- js/                         # Browser API & presentation client
|
|-- data/                           # Manifests, test sample data, logs
|-- docs/                           # Architecture diagrams, viva notes
|-- scripts/                        # Automation & build scripts
|   |-- build.ps1                   # C11 build orchestrator
|   |-- run.ps1                     # Execution script
|
|-- PROJECT_PLAN.md                 # Master project charter
|-- ARCHITECTURE.md                 # Detailed architecture & pipeline design
|-- DATA_STRUCTURES.md              # Formal DSA analysis & Big-O proofs
|-- IMPLEMENTATION_ROADMAP.md       # Phased implementation schedule
|-- README.md                       # Project documentation (this file)
```

---

## 4. Build & Execution Instructions

### Prerequisites:
A C11-compliant compiler is required (MinGW-w64 GCC or Clang).

If GCC is not yet installed, it can be installed via Windows Package Manager:
```powershell
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

### Building the C Backend:
From the workspace root directory, run the PowerShell build script:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```
The script will verify compiler availability, compile `backend/src/` with C11 flags (`-std=c11 -Wall -Wextra -Wpedantic -municode -lbcrypt`), and place the output executable at:
```text
backend/build/duplicate-x.exe
```

### Running the Backend & Scanning Directories:
```powershell
# Display usage
powershell -ExecutionPolicy Bypass -File scripts/run.ps1

# Scan target directories via CLI
powershell -ExecutionPolicy Bypass -File scripts/run.ps1 "data/test_files/device_laptop" "data/test_files/device_usb"
```

### Launching the Web Dashboard (Part 6B):
DUPLICATE-X embeds a high-performance Winsock2 HTTP server that serves both the REST API and the frontend dashboard from a single C binary:
```powershell
# Start local C Winsock2 server on 127.0.0.1:8080
.\backend\build\duplicate-x.exe --server
```
Open your web browser and navigate to:
```text
http://127.0.0.1:8080
```
From the interactive dashboard, you can:
- Queue arbitrary native Windows directories.
- Run multi-directory deduplication scans with live visual telemetry.
- View Top K storage waste rankings calculated by the C11 Binary Min Heap ($O(N \log K)$).
- Inspect discovered duplicate groups in expandable accordions with byte-level verification status.
- Perform instantaneous path prefix queries powered by the custom C11 Prefix Trie ($O(L)$).

### Running Tests Locally:
To build the test fixtures and execute the verification test suites:
```powershell
# Part 4 Verification
powershell -ExecutionPolicy Bypass -File scripts/test.ps1

# Part 6A/6B API & Static Asset Serving Verification
powershell -ExecutionPolicy Bypass -File scripts/test_api.ps1

# Part 7A Safe Quarantine & File Recovery Verification
powershell -ExecutionPolicy Bypass -File scripts/test_quarantine.ps1
```
*Note on Windows 11 Smart App Control (SAC):* If Smart App Control is enabled in Enforcement Mode (`VerifiedAndReputablePolicyState = 1`), execution of newly compiled unsigned native binaries may be intercepted by host Code Integrity policies. The test scripts detect this condition and report diagnostic information. Complete test execution is verified on GitHub-hosted Windows runners.

### Running Tests via GitHub Actions CI:
A standardized Windows CI workflow is provided at `.github/workflows/windows-tests.yml`.

1. **Triggering the CI Workflow:**
   - **Automatic:** Pushing commits or opening pull requests targeting `main` or `master`.
   - **Manual:** Navigate to **Actions** $\rightarrow$ **Windows CI - C11 Deduplication & Test Suite** $\rightarrow$ click **Run workflow**.
2. **What the CI Workflow Executes:**
   - Runs on a GitHub-hosted `windows-latest` runner.
   - Provisions the official `mingw-w64-ucrt-x86_64-gcc` toolchain via MSYS2.
   - Compiles and runs all modular test suites:
     - Part 4 (12 tests): Hash Table Separate Chaining & Linked List Grouping.
     - Part 5A (15 tests): Binary Min Heap & Top K Storage Waste.
     - Part 5B (18 tests): Prefix Trie & Path Sub-Tree Search.
     - Part 6A/6B (18 tests): Winsock2 REST API & Static Asset File Serving.
     - Part 7A (25 tests): Safe Quarantine Engine, Write-Ahead Journal, Crash Recovery & Restoration.
   - Executes `duplicate-x.exe --help` CLI smoke test.

---

## 5. Current Implementation Status

| Milestone / Component | Status | Notes |
| :--- | :--- | :--- |
| **Project Master Plan & Scope** | Completed | `PROJECT_PLAN.md` ratified with team branding. |
| **System Architecture Design** | Completed | `ARCHITECTURE.md` with 4-tier pipeline & volume quarantine. |
| **Data Structures Formal Spec** | Completed | `DATA_STRUCTURES.md` with Hash Table, Linked List, Heap, Trie, Graph. |
| **Project Scaffolding** | Completed | Clean modular directory structure (`backend/`, `frontend/`, `scripts/`). |
| **Initial C11 Foundation** | Completed | `config.h`, `status.h`, `engine.h`, `engine.c`, `main.c`. |
| **Build & Run Automation** | Completed | `scripts/build.ps1`, `scripts/run.ps1`, `scripts/test.ps1`, `scripts/test_api.ps1`, `scripts/test_quarantine.ps1`. |
| **File Scanner Module** | Completed | Recursive Unicode traversal (`win_file_scanner.c`), dynamic `FileInventory`. |
| **SHA-256 Engine (BCrypt)** | Completed | Windows BCrypt 64KB chunk streaming (`win_crypto.c`) with modification safety checks. |
| **Part 4: Custom Hash Table** | Completed | Separate chaining, prime capacities, dynamic resizing at $\alpha > 0.75$, 64-bit FNV-1a. |
| **Part 4: Duplicate Group Lists** | Completed | Non-owning `FileNode` linked lists, recoverable storage math. |
| **Part 4: Byte-by-Byte Comparator** | Completed | 64KB streaming buffer, `CreateFileW` with `FILE_SHARE_READ`. |
| **Part 4: Duplicate Detection Engine** | Completed | 10-step grouping pipeline, cryptographic collision branch isolation. |
| **Part 4 Test Suite (12 Scenarios)** | Verified (12/12 Passed) | Verified on GitHub Actions Windows CI runner. |
| **Part 5A: Binary Min Heap** | Verified (15/15 Passed) | Custom Min Heap, Top K storage ranking ($O(N \log K)$), verified on GitHub Actions. |
| **Part 5B: Prefix Trie** | Verified (18/18 Passed) | Custom Prefix Trie, sub-tree path search ($O(L)$), verified on GitHub Actions. |
| **Part 6A: Local C REST API** | Verified (15/15 Passed) | Pure C Winsock2 HTTP/1.1 REST API, JSON serialization, loopback binding. |
| **Part 6B: Frontend Dashboard** | Completed & Verified | HTML5/CSS3/Vanilla JS dark navy UI, static file routing, path traversal protection (18/18 tests). |
| **Part 7A: Safe Quarantine & Recovery** | Completed & Verified (25/25 Passed) | Same-volume `MoveFileExW` vault, sole-copy protection, 2-stage BCrypt tokens, write-ahead journaling & crash reconciliation. |

