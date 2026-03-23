// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 skyie
//
// This file is part of pi1541ui.
// pi1541ui is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// See the LICENSE file for details.

// Alpine.js Release Store
// Persists CSDb release metadata and download history to localStorage

document.addEventListener("alpine:init", () => {
  Alpine.store("release", {
    releases: {},
    downloadResult: null,
    detail: null,
    detailLoading: false,
    downloading: false,
    installing: false,
    typeFilter: 'All',

    init() {
      try {
        const saved = localStorage.getItem("csdb_releases");
        if (saved) this.releases = JSON.parse(saved);
      } catch (err) {
        console.error("[releaseStore] Failed to load from localStorage:", err);
      }
    },

    persist() {
      try {
        localStorage.setItem("csdb_releases", JSON.stringify(this.releases));
      } catch (err) {
        console.error("[releaseStore] Failed to persist to localStorage:", err);
      }
    },

    async openDetail(result, fetch = true) {
      if (this.detailLoading) return;
      const cached = this.releases[result.id]?.meta;
      if (!fetch || cached) {
        this.detail = cached
          ? { ...cached }
          : { id: result.id, name: result.name, year: result.year || '', type: result.type || '', group: result.group || '', date: result.date || '', party: result.party || '', rating: result.rating ?? null, votes: result.votes ?? null, downloads: result.downloads || [] };
        return;
      }
      this.detail = { id: result.id, name: result.name, year: result.year || '', type: result.type || '', group: result.group || '', date: result.date || '', party: result.party || '', rating: result.rating ?? null, votes: result.votes ?? null, downloads: result.downloads || [] };
      this.detailLoading = true;
      try {
        const details = await Alpine.store('csdb').fetchReleaseDetails(result.id);
        if (details) {
          this.detail = { ...details };
        }
      } catch (err) {
        Alpine.store('toast').error('Failed to load release details: ' + err.message);
        this.detail = null;
      } finally {
        this.detailLoading = false;
      }
    },

    closeDetail() { this.detail = null; this.downloadResult = null; },

    _buildInstallPath({ year, name, group, type, date } = {}) {
      const y = year || date?.match(/\d{4}/)?.[0] || '';
      const base = [...(y ? [y] : []), name || ''].join(' - ');
      const subdirName = (group ? `${base} [${group}]` : base).replace(/[<>:"/\\|?*]/g, '_').trim();
      const typeName = (type || '').replace(/[<>:"/\\|?*]/g, '_').trim();
      const rootPath = '/csdb-releases';
      const typePath = typeName ? `${rootPath}/${typeName}` : rootPath;
      return { typePath, subdirPath: `${typePath}/${subdirName}`, subdirName, typeName };
    },

    installPath() {
      if (!this.detail) return '';
      return this._buildInstallPath(this.detail).subdirPath;
    },

    formatSize(bytes) {
      if (!bytes) return '';
      if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
      if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' KB';
      return bytes + ' B';
    },

    /**
     * Replace metadata for a release (keeps existing downloads intact)
     * @param {object} parsed - Result from csdbStore.parseReleasePage
     */
    upsertMeta(parsed) {
      if (!parsed?.id) return;
      const existing = this.releases[parsed.id];
      this.releases[parsed.id] = {
        meta: parsed,
        downloads: existing?.downloads || [],
        installs: existing?.installs || [],
      };
      this.persist();
    },

    /**
     * Record a completed download for a release
     * @param {number} id - Release ID
     * @param {object} record - { filename, url, externalUrl, size, zipContents }
     */
    recordDownload(id, record) {
      if (!this.releases[id]) return;
      this.releases[id].downloads.push({
        ...record,
        downloadedAt: new Date().toISOString(),
      });
      this.persist();
    },

    /**
     * Install a downloaded release onto the Pi1541 device.
     * Uploads extracted zip files (or a single file) into:
     *   /csdb-releases/<year> - <name> - <group>/
     * Creates the root and subdirectory if they don't already exist.
     * @param {number} releaseId - Release ID
     * @param {number} downloadId - Download ID
     */
    async installRelease(releaseId, downloadId) {
      if (!this.detail) return;

      const blob = this.downloadResult?.blob;
      if (!blob) {
        Alpine.store('toast').error('No download in memory — re-download first');
        console.warn('[releaseStore] installRelease: no blob in downloadResult');
        return;
      }

      const { typePath, subdirPath, subdirName, typeName } = this._buildInstallPath({
        year: this.detail.year,
        name: this.detail.name,
        group: this.detail.group,
        type: this.detail.type,
        date: this.detail.date,
      });
      const filename = this.downloadResult?.filename || 'file.d64';
      const proxy = Alpine.store('proxy');
      const fileOps = Alpine.store('fileOps');

      this.installing = true;
      console.log(`[releaseStore] Installing "${subdirName}" → ${subdirPath}`);
      try {
        await fileOps._createDirectoryIfMissing('/', 'csdb-releases');

        if (typeName) {
          await fileOps._createDirectoryIfMissing('/csdb-releases', typeName);
        }

        console.log(`[releaseStore] Ensuring ${subdirPath} exists`);
        await fileOps._createDirectoryIfMissing(typePath, subdirName);

        const diskImages = [];

        if (filename.toLowerCase().endsWith('.zip')) {
          console.log(`[releaseStore] Extracting zip: ${filename}`);
          const zip = await JSZip.loadAsync(blob);
          for (const [zipPath, zipFile] of Object.entries(zip.files)) {
            if (zipFile.dir) continue;
            const fname = zipPath.split('/').pop();
            const content = await zipFile.async('blob');
            const fileObj = new File([content], fname);
            console.log(`[releaseStore] Uploading ${subdirPath}/${fname}`);
            await fileOps.uploadFileSilent(subdirPath, fileObj);
            if (/\.(d64|g64|t64|prg)$/i.test(fname)) diskImages.push(fname);
          }
        } else {
          const fileObj = new File([blob], filename);
          console.log(`[releaseStore] Uploading ${subdirPath}/${filename}`);
          await fileOps.uploadFileSilent(subdirPath, fileObj);
          if (/\.(d64|g64|t64|prg)$/i.test(filename)) diskImages.push(filename);
        }

        let mountFile = null;
        if (diskImages.length > 1) {
          const lstContent = [...diskImages, 'dos1541'].join('\n');
          const lstFile = new File([lstContent], '_autoswap.lst', { type: 'text/plain' });
          console.log(`[releaseStore] Uploading _autoswap.lst:\n${lstContent}`);
          await fileOps.uploadFileSilent(subdirPath, lstFile);
          mountFile = '_autoswap.lst';
        } else if (diskImages.length === 1) {
          mountFile = diskImages[0];
        }

        await proxy._reloadDirectory(typePath);

        this.upsertMeta(this.detail);
        this.recordDownload(releaseId, {
          id: this.downloadResult.id,
          filename: this.downloadResult.filename,
          url: this.downloadResult.url,
          externalUrl: this.downloadResult.externalUrl,
          size: this.downloadResult.size,
          zipContents: this.downloadResult.zipContents,
        });
        this.releases[releaseId].installs.push({ path: subdirPath, mountFile, downloadId, installedAt: new Date().toISOString() });
        this.persist();

        this.downloadResult = null;
        this.detail = null;
        Alpine.store('toast').success(subdirName, 'Installed');
        console.log(`[releaseStore] Install complete: ${subdirPath}`);
      } catch (err) {
        console.error('[releaseStore] Install failed:', err);
        Alpine.store('toast').error('Install failed: ' + err.message);
      } finally {
        this.installing = false;
      }
    },

    async installAndMount(releaseId, downloadId) {
      await this.installRelease(releaseId, downloadId);
      if (this.isInstalled(releaseId)) {
        await this.mountInstall(releaseId);
      }
    },

    /**
     * Mounts fb-d64 from root to displace any loaded release, then deletes /csdb-releases.
     */
    async deleteReleasesDir() {
      await Alpine.store('fileOps').mountDefaultImage();
      await Alpine.store('fileOps').deleteObject('/', 'csdb-releases');
    },

    timeAgo(iso) {
      if (!iso) return '';
      const diff = Date.now() - new Date(iso).getTime();
      const mins = Math.floor(diff / 60000);
      if (mins < 1) return 'just now';
      if (mins < 60) return `${mins}m ago`;
      const hours = Math.floor(mins / 60);
      if (hours < 24) return `${hours}h ago`;
      const days = Math.floor(hours / 24);
      if (days < 30) return `${days}d ago`;
      const months = Math.floor(days / 30);
      if (months < 12) return `${months}mo ago`;
      return `${Math.floor(months / 12)}y ago`;
    },

    isInstalled(id) {
      return (this.releases[id]?.installs?.length ?? 0) > 0;
    },

    mountFilePath(id) {
      const install = this.releases[id]?.installs?.at(-1);
      if (!install?.mountFile) return null;
      return `${install.path}/${install.mountFile}`;
    },

    async mountInstall(id) {
      const path = this.mountFilePath(id);
      if (!path) return;
      await Alpine.store('fileOps').mountFile(path);
    },

    isMounted(id) {
      const path = this.mountFilePath(id);
      return !!path && Alpine.store('disc').mountedFile === path;
    },

    async uninstallRelease(releaseId) {
      const entry = this.releases[releaseId];
      if (!entry) return;

      const installPath = entry.installs?.at(-1)?.path;
      if (!installPath) return;

      const confirmed = window.confirm(
        `Warning: uninstall will remove metadata and delete "${installPath}" from the filesystem.\n\nContinue?`
      );
      if (!confirmed) return;

      const lastSlash = installPath.lastIndexOf('/');
      const parentPath = installPath.substring(0, lastSlash) || '/';
      const dirName = installPath.substring(lastSlash + 1);

      const proxy = Alpine.store('proxy');
      await Alpine.store('fileOps').deleteObject(parentPath, dirName);
      if (proxy.connectionError) return;

      delete this.releases[releaseId];
      this.persist();
      this.closeDetail();
    },

    /**
     * Get the full stored record for a release
     * @param {number} id
     * @returns {{ meta, downloads } | null}
     */
    get(id) {
      return this.releases[id] ?? null;
    },

    /**
     * All stored releases as an array, newest download first
     * @returns {Array}
     */
    all() {
      return Object.values(this.releases).sort((a, b) => {
        const aDate = a.downloads.at(-1)?.downloadedAt ?? "";
        const bDate = b.downloads.at(-1)?.downloadedAt ?? "";
        return bDate.localeCompare(aDate);
      });
    },

    availableTypes() {
      const types = new Set(Object.values(this.releases).map(r => r.meta.type).filter(Boolean));
      return ['All', ...[...types].sort()];
    },

    filtered() {
      const all = this.all();
      if (this.typeFilter === 'All') return all;
      return all.filter(r => r.meta.type === this.typeFilter);
    },
  });
});
