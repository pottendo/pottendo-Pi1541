// Alpine.js File Operations Store
// All write operations against the Pi1541 device: upload, delete, rename,
// create directory, and mount. Read/parse operations remain in proxyStore.
//
// Loading state (connecting, connectionError) lives in proxyStore so the
// single loading overlay in file-browser.html covers all device activity.

document.addEventListener("alpine:init", () => {
  Alpine.store("fileOps", {

    // Convenience accessors — avoids repeating Alpine.store("proxy") everywhere
    get _proxy() { return Alpine.store("proxy"); },

    /**
     * Creates a new directory on the Pi1541 device.
     * @param {string} path - Parent directory path (e.g., "/test/t2")
     * @param {string} dirName - Name of the directory to create
     */
    async createDirectory(path, dirName) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        const url = `${proxy.pi_endpoint}/index.html?[DIR]&${path}&[MKDIR]&${encodeURIComponent(dirName)}`;
        console.log(`[fileOpsStore] Creating directory: ${path}/${dirName}`);
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        await proxy._reloadDirectory(path);
        Alpine.store("toast").success(dirName, "Directory created");
        proxy.connectionError = null;
      } catch (err) {
        console.error("[fileOpsStore] createDirectory error:", err);
        proxy.connectionError = "Failed to create directory";
        Alpine.store("toast").error("Failed to create directory");
      } finally {
        proxy.connecting = false;
      }
    },

    /**
     * Creates a directory only if it is not already present in the disc tree.
     * Silent — no toast. Intended for programmatic use (e.g. release install).
     * @param {string} parentPath - Absolute parent path (e.g., "/csdb-releases")
     * @param {string} dirName - Name of the directory to create
     */
    async _createDirectoryIfMissing(parentPath, dirName) {
      const proxy = this._proxy;
      const disc = Alpine.store("disc");
      const parent = parentPath === "/" ? disc.root : disc.findDirectoryByPath(parentPath);
      const fullPath = (parentPath === "/" ? "" : parentPath) + "/" + dirName;
      if (parent?.subdirectories?.some(d => d.path === fullPath)) {
        console.log(`[fileOpsStore] Directory already exists: ${fullPath}`);
        return;
      }
      const url = `${proxy.pi_endpoint}/index.html?[DIR]&${parentPath}&[MKDIR]&${encodeURIComponent(dirName)}`;
      console.log(`[fileOpsStore] Creating directory: ${fullPath}`);
      const resp = await fetch(url);
      if (!resp.ok) {
        console.warn(`[fileOpsStore] MKDIR ${fullPath} returned HTTP ${resp.status} — may already exist`);
      }
      await proxy._reloadDirectory(parentPath);
    },

    /**
     * Deletes a file or directory on the Pi1541 device.
     * @param {string} path - Parent directory path
     * @param {string} name - Name of the item to delete
     */
    async deleteObject(path, name) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        const url = `${proxy.pi_endpoint}/index.html?[DIR]&${encodeURIComponent(path)}&[DEL]&${encodeURIComponent(name)}`;
        console.log(`[fileOpsStore] Deleting: ${path}/${name}`);
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        await proxy._reloadDirectory(path);
        Alpine.store("toast").success(name, "Deleted");
        proxy.connectionError = null;
      } catch (err) {
        console.error("[fileOpsStore] deleteObject error:", err);
        proxy.connectionError = "Failed to delete";
        Alpine.store("toast").error("Failed to delete");
      } finally {
        proxy.connecting = false;
      }
    },

    /**
     * Renames a file or directory on the Pi1541 device.
     * @param {string} oldPath - Full path of the item to rename
     * @param {string} newName - New name (filename only, no slashes)
     */
    async rename(oldPath, newName) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        const parentPath = oldPath.split("/").slice(0, -1).join("/") || "/";
        const newPath = `${parentPath === "/" ? "" : parentPath}/${newName}`;
        const url = `${proxy.pi_endpoint}/mount-imgs.html?[RENAME]&${encodeURIComponent(oldPath)}&[NEWNAME]&${encodeURIComponent(newPath)}`;
        console.log(`[fileOpsStore] Renaming: ${oldPath} → ${newPath}`);
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        await proxy._reloadDirectory(parentPath);
        Alpine.store("toast").success(newName, "Renamed");
        proxy.connectionError = null;
      } catch (err) {
        console.error("[fileOpsStore] rename error:", err);
        proxy.connectionError = "Failed to rename";
        Alpine.store("toast").error("Failed to rename");
      } finally {
        proxy.connecting = false;
      }
    },

    /**
     * Uploads a single disk image file to the Pi1541 device.
     * @param {string} path - Current directory path (e.g., "/games")
     * @param {File} file - The file to upload
     * @param {boolean} [automount=false] - Whether to automount after upload
     * @returns {Promise<boolean>}
     */
    async uploadFile(path, file, automount = false) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        const form = new FormData();
        form.append("xpath", path);
        form.append("diskimage", file, file.name);
        if (automount) form.append("am-cb1", "1");
        form.append("buttsubmit", "Upload Diskimage");

        console.log(`[fileOpsStore] Uploading file: ${file.name} to ${path}`);
        const resp = await fetch(`${proxy.pi_endpoint}/index.html`, { method: "POST", body: form });
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        await proxy._reloadDirectory(path);
        Alpine.store("toast").success(file.name, "Uploaded");
        proxy.connectionError = null;
        return true;
      } catch (err) {
        console.error("[fileOpsStore] uploadFile error:", err);
        proxy.connectionError = "Upload failed";
        Alpine.store("toast").error("Upload failed");
        return false;
      } finally {
        proxy.connecting = false;
      }
    },

    /**
     * Uploads a directory (set of files) to the Pi1541 device.
     * @param {string} path - Current directory path (e.g., "/games")
     * @param {FileList} files - Files from a webkitdirectory input
     * @returns {Promise<boolean>}
     */
    async uploadDirectory(path, files) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        const form = new FormData();
        form.append("xpath", path);
        for (const file of files) {
          form.append("directory", file, file.webkitRelativePath || file.name);
        }
        form.append("buttsubmitdir", "Upload Directory");

        const dirName = files[0]?.webkitRelativePath?.split("/")[0] || "directory";
        console.log(`[fileOpsStore] Uploading directory: ${dirName} to ${path}`);
        const resp = await fetch(`${proxy.pi_endpoint}/index.html`, { method: "POST", body: form });
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        await proxy._reloadDirectory(path);
        Alpine.store("toast").success(dirName, "Directory uploaded");
        proxy.connectionError = null;
        return true;
      } catch (err) {
        console.error("[fileOpsStore] uploadDirectory error:", err);
        proxy.connectionError = "Upload failed";
        Alpine.store("toast").error("Upload failed");
        return false;
      } finally {
        proxy.connecting = false;
      }
    },

    /**
     * Uploads a single file silently — no toast, no connecting state, no
     * directory reload. Throws on HTTP error so the caller can handle it.
     * Intended for bulk programmatic uploads (e.g. release install).
     * @param {string} path - Target directory path
     * @param {File} file - The file to upload
     */
    async uploadFileSilent(path, file) {
      const form = new FormData();
      form.append("xpath", path);
      form.append("diskimage", file, file.name);
      form.append("buttsubmit", "Upload Diskimage");
      const resp = await fetch(`${this._proxy.pi_endpoint}/index.html`, { method: "POST", body: form });
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    },

    /**
     * Mounts the default fallback image (fb-d64) from the root directory.
     */
    async mountDefaultImage() {
      await this.mountFile("/fb.d64");
    },

    /**
     * Mounts a file on the Pi1541 device.
     * @param {string} filePath - File path (e.g., "/games/RTYPE  9DMHI.D64")
     */
    async mountFile(filePath) {
      const proxy = this._proxy;
      try {
        proxy.connecting = true;
        proxy.connectionError = null;
        console.log(`[fileOpsStore] Mounting file: ${filePath}`);
        await proxy.downloadFileDetails(filePath, "MOUNT");
        Alpine.store("disc").mountedFile = filePath;
        proxy.connectionError = null;
        Alpine.store("toast").success(filePath, "Mounted");
      } catch (err) {
        console.error("[fileOpsStore] mountFile error:", err);
        proxy.connectionError = "Failed to mount file";
        Alpine.store("toast").error("Failed to mount file");
        return;
      } finally {
        proxy.connecting = false;
      }
      await new Promise(resolve => setTimeout(resolve, 200));
      await Alpine.store("piStats").processStats();
    },
  });
});
