# DUPLICATE-X: Native Windows Folder Picker & Backend Connection Verification

**Project:** DUPLICATE-X (Intelligent File Deduplication & Storage Optimization)  
**Team:** DataVault  
**Academic Year:** 2026–2027  
**Architecture:** Pure C11 Winsock2 Backend + Windows COM `IFileOpenDialog` API + Vanilla JavaScript / HTML5 Frontend

---

## 1. Native Windows Folder Selection Architecture

### Why Browser File Inputs Failed
HTML5 `<input type="file" webkitdirectory>` executes inside the web browser's security sandbox. Modern browsers (Chrome, Edge, Firefox) intentionally strip the host drive letter and absolute filesystem path (e.g. `C:\Users\...`) for security, returning only relative internal names (e.g. `folder/subfile.ext`). When sent to the C backend, these relative paths could not be opened by Win32 `FindFirstFileW`.

### C Backend Native Windows Solution
Instead of relying on browser sandboxed file inputs, DUPLICATE-X provides a dedicated REST API endpoint:
```http
POST /api/folder-picker
```
- **Implementation:** Built directly into `backend/src/api/api_router.c` using the Windows COM Shell API (`IFileOpenDialog`).
- **Flags:**
  - `FOS_PICKFOLDERS`: Configures the modal dialog to select directories rather than individual files.
  - `FOS_FORCEFILESYSTEM`: Guarantees the selected item is a physical directory residing on an accessible disk volume.
  - `FOS_PATHMUSTEXIST`: Ensures only existing folders can be chosen.
- **Path Resolution:** Uses `IShellItem::GetDisplayName` with `SIGDN_FILESYSPATH` to retrieve the absolute Windows path (e.g., `C:\Users\User\Documents`) and encodes it as UTF-8 JSON.
- **Resource Cleanup:** Calls `CoTaskMemFree` on the path string, releases COM interfaces (`IShellItem`, `IFileOpenDialog`), and terminates with `CoUninitialize()`.
- **Cancellation:** If the user closes the dialog or clicks Cancel, the endpoint returns:
  ```json
  {
    "status": "cancelled",
    "selected": false,
    "path": null
  }
  ```
  The frontend acknowledges cancellation cleanly without displaying any error alert.

---

## 2. Separation of Native GUI and Automated Headless CI

Because GitHub Actions Windows runners execute in headless non-interactive window stations (Session 0), displaying an interactive modal GUI dialog (`IFileOpenDialog::Show`) cannot be automated via synthetic keypresses without hanging CI runners.

To maintain 100% CI compliance while preserving native production Windows desktop interaction:
1. **Automated CI Tests:**
   - Verify endpoint routing, method constraints (HTTP 405 on `GET`), and CORS preflight (`OPTIONS` returns 204).
   - Test JSON contracts, state handling, queue validation, and scan execution through `scripts/test_browse_and_connection.ps1` and `scripts/test_api.ps1`.
2. **Interactive Desktop Verification:**
   - Executed manually on Windows 11 desktop using the steps below.

---

## 3. Manual Windows Desktop Testing Instructions

1. **Start the Backend Server:**
   Double-click `start_server.bat` in the workspace root, or run:
   ```cmd
   backend\build\duplicate-x.exe --server
   ```
   A terminal window will open showing:
   ```
   =================================
           DUPLICATE-X
          Powered by DataVault
   =================================
   Starting DUPLICATE-X Local API...
   Server address: http://127.0.0.1:8080
   ```

2. **Open the Dashboard:**
   Navigate to `http://127.0.0.1:8080` in your browser.
   - Verify the top-right indicator shows: `🟢 Online (127.0.0.1:8080)`.
   - Verify `Run Deduplication Scan` is initially disabled (empty queue).

3. **Test Native Folder Picker:**
   - Click the **"Browse Folder..."** button.
   - The native Windows 11 **"Select Target Directory for Deduplication"** dialog appears.
   - Select any directory (e.g. `c:\Users\visha\OneDrive\Desktop\DataVault\data\test_files\device_laptop`).
   - Click **"Select Folder"**.
   - Notice the exact absolute path appears in the input field and is added to the **TARGET QUEUE**.
   - Notice the **Run Deduplication Scan** button is now enabled.

4. **Test Cancellation:**
   - Click **"Browse Folder..."** again.
   - Click **"Cancel"** (or press Escape).
   - Verify no error is displayed and the existing queue remains intact.

5. **Test Multi-Directory Comparison:**
   - Click **`+ Sample USB`** from the Quick Test Folders bar.
   - Both `device_laptop` and `device_usb` are now queued.
   - Click **"Run Deduplication Scan"**.
   - Watch the scan progress, find duplicate groups, and update all 4 metric cards and Top K charts!

