git --version# DUPLICATE-X: Intelligent Multi-Device File Deduplication & Storage Optimization System
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

# Scan target directories
powershell -ExecutionPolicy Bypass -File scripts/run.ps1 "data/test_files/device_laptop" "data/test_files/device_usb"
```

### Running Tests Locally:
To build the test fixtures and execute the 12-scenario Part 4 verification test suite:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/test.ps1
```
*Note on Windows 11 Smart App Control (SAC):* If Smart App Control is enabled in Enforcement Mode (`VerifiedAndReputablePolicyState = 1`), execution of newly compiled unsigned native binaries may be intercepted by host Code Integrity policies. The test script detects this condition, reports diagnostic information, and halts with an error rather than falsely claiming test success.

### Running Tests via GitHub Actions CI:
A standardized Windows CI workflow is provided at `.github/workflows/windows-tests.yml`.

1. **Triggering the CI Workflow:**
   - **Automatic:** Pushing commits or opening pull requests targeting `main` or `master`.
   - **Manual:** Navigate to **Actions** $\rightarrow$ **Windows CI - C11 Deduplication & Test Suite** $\rightarrow$ click **Run workflow**.
2. **What the CI Workflow Executes:**
   - Runs on a GitHub-hosted `windows-latest` runner.
   - Provisions the official `mingw-w64-ucrt-x86_64-gcc` toolchain via MSYS2.
   - Compiles both `duplicate-x.exe` and `test_duplicate_detector.exe`.
   - Executes all 12 test scenarios in `test_duplicate_detector.exe`.
   - Executes `duplicate-x.exe --help` smoke test.
3. **Interpreting CI Results:**
   - **Green Checkmark (PASS):** All 12 test scenarios completed with 100% assertions verified and exit code `0`.
   - **Red Cross (FAIL):** Any C assertion failed or compilation aborted, failing the entire job and preserving step logs.

---

## 5. Current Implementation Status

| Milestone / Component | Status | Notes |
| :--- | :--- | :--- |
| **Project Master Plan & Scope** | Completed | `PROJECT_PLAN.md` ratified with team branding. |
| **System Architecture Design** | Completed | `ARCHITECTURE.md` with 4-tier pipeline & volume quarantine. |
| **Data Structures Formal Spec** | Completed | `DATA_STRUCTURES.md` with Hash Table, Linked List, Heap, Trie, Graph. |
| **Project Scaffolding** | Completed | Clean modular directory structure (`backend/`, `frontend/`, `scripts/`). |
| **Initial C11 Foundation** | Completed | `config.h`, `status.h`, `engine.h`, `engine.c`, `main.c`. |
| **Build & Run Automation** | Completed | `scripts/build.ps1`, `scripts/run.ps1`, `scripts/test.ps1`. |
| **File Scanner Module** | Completed | Recursive Unicode traversal (`win_file_scanner.c`), dynamic `FileInventory`. |
| **SHA-256 Engine (BCrypt)** | Completed | Windows BCrypt 64KB chunk streaming (`win_crypto.c`) with modification safety checks. |
| **Part 4: Custom Hash Table** | Completed | Separate chaining, prime capacities, dynamic resizing at $\alpha > 0.75$, 64-bit FNV-1a. |
| **Part 4: Duplicate Group Lists** | Completed | Non-owning `FileNode` linked lists, recoverable storage math. |
| **Part 4: Byte-by-Byte Comparator** | Completed | 64KB streaming buffer, `CreateFileW` with `FILE_SHARE_READ`. |
| **Part 4: Duplicate Detection Engine** | Completed | 10-step grouping pipeline, cryptographic collision branch isolation. |
| **Part 4 Verification Test Suite** | Compiled (CI Configured) | All 12 scenarios implemented in `test_duplicate_detector.c`; local runtime pending due to Windows SAC; automated Windows CI workflow configured. |

