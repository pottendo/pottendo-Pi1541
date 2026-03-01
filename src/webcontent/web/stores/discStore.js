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

    // Load a directory's contents using proxy store
    async loadDirectoryContents(path) {
      try {
        if (
          typeof Alpine !== "undefined" &&
          Alpine.store &&
          Alpine.store("proxy")
        ) {
          const relativePath = path === "/" ? "" : path.substring(1);

          console.log(`[discStore] Loading directory contents for: ${path}`);

          const result = await Alpine.store("proxy").processIndex(relativePath);

          if (result) {
            const directory = this.findDirectoryByPath(path);
            if (directory) {
              directory.loaded = true;
              directory.subdirectories = result.subdirectories || [];
              directory.disc_images = result.disc_images || [];

              directory.subdirectories.forEach((subdir) => {
                subdir.loaded = false;
              });

              console.log(
                `[discStore] Successfully loaded ${directory.subdirectories.length} subdirectories and ${directory.disc_images.length} files`,
              );

              if (Alpine.store("cache")) Alpine.store("cache").saveTree(this.root);
              return true;
            }
          }
        } else {
          console.warn(
            "[discStore] Proxy store not available for loading directory contents",
          );
        }
      } catch (err) {
        console.error(
          `[discStore] Error loading directory contents for ${path}:`,
          err,
        );
      }
      return false;
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
      const lines = this.petsciiToLines(html)
        .map((l) => l.ascii.toUpperCase())
        .filter((l) => l.length > 0);
      return lines.slice(0, -1);
    },

    // Extract searchable ASCII lines from C64 screen-code HTML using a single regex pass.
    petsciiToLines(html) {
      return html.split(/<br\s*\/?>/i).map((line) => {
        const chars = line.match(/[\uee01-\uee3f\uee81-\ueebf]/g) || [];
        const ascii = chars.map((c) => {
          let sc = c.codePointAt(0) - 0xee00;
          if (sc >= 0x80) sc -= 0x80;
          if (sc >= 0x01 && sc <= 0x1a) return String.fromCharCode(sc + 0x40);
          if (sc === 0x1f) return "_";
          if (sc >= 0x20 && sc <= 0x3f) return String.fromCharCode(sc);
          return "";
        }).join("");
        return { ascii, html: line };
      });
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
