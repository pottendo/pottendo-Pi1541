// Alpine.js Disc Store
// Manages hierarchical tree structure of disc images and directories

document.addEventListener("alpine:init", () => {
  const _savedTree = (() => {
    try {
      const json = localStorage.getItem("pi1541ui_tree");
      return json ? JSON.parse(json) : null;
    } catch (e) {
      return null;
    }
  })();

  Alpine.store("disc", {
    mountedFile: null,

    // Root directory structure — restored from localStorage if available
    root: _savedTree || {
      path: "/",
      date_string: "2024-01-15",
      loaded: false,
      subdirectories: [],
      disc_images: [],
    },

    // Find a directory by path
    findDirectoryByPath(path) {
      if (path === "/") return this.root;

      const parts = path.split("/").filter((p) => p !== "");
      let dir = this.root;
      let current = "";

      for (const part of parts) {
        current += "/" + part;
        dir = dir.subdirectories.find((s) => s.path === current);
        if (!dir) return null;
      }

      return dir;
    },

    // Find a disc image object by its full path (e.g. "/Games/pacman.d64")
    findDiscImageByPath(filePath) {
      const parts = filePath.split("/");
      parts.pop();
      const dirPath = parts.join("/") || "/";
      const dir = this.findDirectoryByPath(dirPath);
      return dir ? dir.disc_images.find((d) => d.path === filePath) : null;
    },

    // Check if a directory is loaded
    isDirectoryLoaded(path) {
      const directory = this.findDirectoryByPath(path);
      return directory ? directory.loaded : false;
    },

    goToParent(currentPath) {
      if (currentPath === "/") return currentPath;
      const parts = currentPath.split("/").filter((p) => p !== "");
      parts.pop();
      return parts.length === 0 ? "/" : "/" + parts.join("/");
    },

    // Pre-compute uppercased ASCII lines from disc_content for fast search.
    // Call once when disc_content is set; store result as disc.disc_ascii.
    computeDiscAscii(html) {
      const lines = PetsciiUtils.petsciiToLines(html)
        .map((l) => l.ascii.toUpperCase())
        .filter((l) => l.length > 0);
      return lines.slice(0, -1);
    },

    searchIgnoreList: ["autoswap.lst"],

    // Search all loaded directories and files across the entire tree
    searchAll(query, searchContents = false) {
      const q = query.toUpperCase();
      const results = [];

      const walk = (directory) => {
        const dirName = directory.path === "/" ? "Root" : directory.path.split("/").pop();
        if (directory.path !== "/" && dirName.toUpperCase().includes(q)) {
          results.push({
            type: "directory",
            name: dirName,
            path: directory.path,
            isDirectory: true,
            discCount: this.computeDiscCountRecursive(directory),
            subdirCount: this.computeSubdirCountRecursive(directory),
          });
        }

        for (const disc of directory.disc_images || []) {
          const discName = disc.path.split("/").pop();
          if (this.searchIgnoreList.includes(discName.toLowerCase())) continue;
          const nameMatch = discName.toUpperCase().includes(q);
          let matchingLines = null;
          if (searchContents && disc.disc_ascii) {
            const matched = disc.disc_ascii.filter((line) => line.includes(q));
            if (matched.length > 0) matchingLines = matched;
          }
          if (nameMatch || matchingLines) {
            results.push({
              type: "file",
              name: discName,
              path: disc.path,
              isDirectory: false,
              date_string: disc.date_string,
              disc_content: disc.disc_content || null,
              matchingLines: matchingLines,
            });
          }
        }

        for (const subdir of directory.subdirectories || []) {
          walk(subdir);
        }
      };

      walk(this.root);
      return results;
    },

    computeDiscCountRecursive(directory) {
      if (!directory) return 0;
      let count = (directory.disc_images || []).length;
      for (const subdir of directory.subdirectories || []) {
        count += this.computeDiscCountRecursive(subdir);
      }
      return count;
    },

    countLoadedDetailsRecursive(directory) {
      if (!directory) return 0;
      let count = (directory.disc_images || []).filter(d => d.disc_content).length;
      for (const subdir of directory.subdirectories || []) {
        count += this.countLoadedDetailsRecursive(subdir);
      }
      return count;
    },

    computeSubdirCountRecursive(directory) {
      if (!directory) return 0;
      let count = (directory.subdirectories || []).length;
      for (const subdir of directory.subdirectories || []) {
        count += this.computeSubdirCountRecursive(subdir);
      }
      return count;
    },

    // Get all items (directories and files) for unified display
    getAllItems(currentPath) {
      const directory = this.findDirectoryByPath(currentPath);
      if (!directory) return [];

      const items = [];

      if (currentPath !== "/") {
        const parts = currentPath.split("/").filter((p) => p !== "");
        const parentPath =
          parts.length > 1 ? "/" + parts.slice(0, -1).join("/") : "/";

        items.push({
          type: "parent",
          name: "..",
          path: parentPath,
          isDirectory: true,
        });
      }

      directory.subdirectories.forEach((subdir) => {
        items.push({
          type: "directory",
          name: subdir.path.split("/").pop(),
          path: subdir.path,
          isDirectory: true,
          discCount: this.computeDiscCountRecursive(subdir),
          subdirCount: this.computeSubdirCountRecursive(subdir),
        });
      });

      directory.disc_images.forEach((disc) => {
        items.push({
          type: "file",
          name: disc.path.split("/").pop(),
          path: disc.path,
          isDirectory: false,
          date_string: disc.date_string,
          disc_content: disc.disc_content || null,
        });
      });

      return items;
    },

  });
});
