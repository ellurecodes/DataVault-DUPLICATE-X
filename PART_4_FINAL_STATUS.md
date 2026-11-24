# DUPLICATE-X — PART 4 FINAL STATUS REPORT

**Project Name:** DUPLICATE-X (Intelligent Multi-Device File Deduplication & Storage Optimization System)  
**Team Name:** DataVault  
**Academic Year:** 2026–2027  
**Platform:** Windows 11 64-bit  
**Compiler:** MinGW-w64 GCC 16.2.0 (UCRT)  
**Status Date:** November 24, 2026  

---

## 1. Executive Status Summary

This report establishes the verified state of **Part 4: Custom Hash Table with Separate Chaining and Linked List-Based Duplicate Grouping**.

| Phase / Discipline | Verified Status | Details & Observations |
|:---|:---:|:---|
| **Source Code Implementation** | **COMPLETED** | All custom DSA data structures (`hashtable.c`, `duplicate_group.c`, `duplicate_detector.c`, `win_file_compare.c`) fully written in C11. |
| **Static Source Inspection** | **COMPLETED** | In-depth code walkthrough completed. Memory lifecycle, ownership boundaries, rehashing safety, and edge cases verified. |
| **C11 Compilation** | **PASSED** | GCC 16.2.0 compiled both `duplicate-x.exe` (93,836 B) and `test_duplicate_detector.exe` (101,791 B) with Exit Code `0` and zero warnings. |
| **Runtime Execution (Local)** | **PENDING** | Blocked on local machine by Windows 11 Smart App Control (SAC) / Device Guard Code Integrity Policy (`VerifiedAndReputablePolicyState = 1`). |
| **Automated CI Verification** | **CONFIGURED** | GitHub Actions Windows CI pipeline configured to run complete 12-test suite on standard runner. |

> [!IMPORTANT]
> **Runtime Status Clarification:** All 12 test scenarios are fully authored and compile cleanly. However, their actual runtime execution has not been established locally because Windows Application Control policy intercepts execution of newly created unsigned executables. The tests have NOT yet passed at runtime on the local machine.

---

## 2. Source Code Implementation Breakdown

All requested components for Part 4 were written from scratch in pure C11 without relying on external libraries:

1. **Custom Hash Table with Separate Chaining (`backend/include/hashtable.h`, `backend/src/ds/hashtable.c`):**
   - Implements dynamic bucket array initialized to a prime capacity.
   - Internal indexing uses 64-bit FNV-1a over composite key `(file_size, sha256_binary)`.
   - Dynamic table expansion triggers whenever load factor $\alpha > 0.75$, stepping through a monotonic prime sequence (`61, 127, 251, 509, 1021, ...`).
   - Re-chains existing nodes into newly sized bucket array during resizing without corrupting registered `groups_head` pointers.

2. **Duplicate Group Linked Lists (`backend/include/duplicate_group.h`, `backend/src/ds/duplicate_group.c`):**
   - `FileNode` instances maintain non-owning `const FileRecord *` references into `FileInventory`.
   - `DuplicateGroup` groups files sharing verified identical contents.
   - Calculates recoverable storage on each addition: `recoverable_bytes = (file_count - 1) * file_size`.
   - `DuplicateGroupList` tracks all verified duplicate clusters, total duplicate copies, and aggregate recoverable bytes.

3. **Byte-by-Byte Comparator (`backend/include/file_compare.h`, `backend/src/platform/windows/win_file_compare.c`):**
   - Opens files using `CreateFileW` with `FILE_SHARE_READ`.
   - Streams file contents in 64 KB buffers using `ReadFile`.
   - Re-verifies file size and modification timestamps to detect concurrent tampering.

4. **Duplicate Detection Engine (`backend/include/duplicate_detector.h`, `backend/src/core/duplicate_detector.c`):**
   - 10-step pipeline: inventory traversal $\rightarrow$ candidate lookup/insert $\rightarrow$ byte-level confirmation against group representatives $\rightarrow$ cryptographic collision branch isolation $\rightarrow$ cluster extraction.
   - Unlinks confirmed duplicate groups (`file_count >= 2`) from hash table entries and transfers them to the caller's `DuplicateGroupList`.
   - Destroys solitary non-duplicate candidate entries cleanly via `hashtable_free(ht)`.

5. **Verification Test Suite (`backend/tests/test_duplicate_detector.c`):**
   - 12 comprehensive unit scenarios covering duplicate naming, size collisions, multi-folder triplets, unique file rejection, 0-byte files, extension heterogeneity, table collisions, dynamic resizing, modification flags, 150 KB streaming chunks, intra-directory duplicates, and Unicode paths.

---

## 3. Compilation Verification

Compilation was validated using the project's build orchestrators:

- **Main Engine:**
  ```text
  Command: gcc -std=c11 -Wall -Wextra -Wpedantic -municode -Ibackend/include backend/src/*.c -o backend/build/duplicate-x.exe -lbcrypt
  Exit Code: 0
  Target: backend/build/duplicate-x.exe (93,836 bytes)
  Diagnostics: Zero warnings, zero errors.
  ```

- **Test Harness:**
  ```text
  Command: gcc -std=c11 -Wall -Wextra -Wpedantic -municode -Ibackend/include backend/tests/test_duplicate_detector.c ... -o backend/build/test_duplicate_detector.exe -lbcrypt
  Exit Code: 0
  Target: backend/build/test_duplicate_detector.exe (101,791 bytes)
  Diagnostics: Zero warnings, zero errors.
  ```

---

## 4. Local Runtime Testing Obstacle

When executing `duplicate-x.exe` or `test_duplicate_detector.exe` in the local development environment, the operating system raises:
```text
Program 'test_duplicate_detector.exe' failed to run: An Application Control policy has blocked this file.
'backend/build/test_duplicate_detector.exe' was blocked by your organization's Device Guard policy.
```

### Forensic Root Cause
1. **Windows 11 Smart App Control (SAC) Enforcement:**
   - Registry check of `HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy` reports:
     - `VerifiedAndReputablePolicyState = 1` (Active Enforcement).
     - `SAC_EnforcementReason = 1`.
     - `EnableVirtualizationBasedSecurity = 1`.
     - `HypervisorEnforcedCodeIntegrity = 1`.
   - Smart App Control blocks newly created native PE executables that lack an established Microsoft cloud reputation or a recognized code-signing certificate from an authorized public CA.
2. **OneDrive Cloud Filter Reparse Tag:**
   - Files created in `C:\Users\visha\OneDrive\Desktop\DataVault` carry NTFS Reparse Tag `0x9000601a` (`IO_REPARSE_TAG_ONEDRIVE`), prompting stricter Device Guard code-integrity evaluation.

---

## 5. Verification Strategy & Next Steps

Because modifying local Windows security policies, creating Defender exclusions, or signing with self-signed certificates are prohibited workarounds:

1. **GitHub Actions Windows CI (`.github/workflows/windows-tests.yml`):**
   - Provides a clean, standardized, Microsoft-hosted Windows Server runner.
   - Installs MinGW-w64 UCRT GCC.
   - Compiles the application and test harness.
   - Executes all 12 test scenarios in a standard environment without Smart App Control interference.
   - Captures comprehensive test logs and enforces a non-zero exit code upon assertion failures.
2. **Local Test Script Hardening (`scripts/test.ps1`):**
   - Updated to ensure native process exit codes are propagated accurately and failures immediately halt processing.

