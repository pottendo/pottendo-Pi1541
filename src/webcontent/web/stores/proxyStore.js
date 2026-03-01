// Alpine.js Proxy Store
// Fetches a remote index.html and parses its content

document.addEventListener("alpine:init", () => {
  Alpine.store("proxy", {
    // Connection state
    connectionError: null,
    connecting: false,

    // Pi endpoint persisted in localStorage (can be changed at runtime)
    pi_endpoint: localStorage.getItem("pi_endpoint") || "http://localhost:8000/proxy",
    setPiEndpoint(prefix) {
      this.pi_endpoint = prefix;
      localStorage.setItem("pi_endpoint", prefix);
    },

    // Pi stats state
    stats: { deviceId: null, currentDiskimage: null, piTemp: null, piFreq: null, time: null },
    statsLoading: false,

    /**
     * Downloads index.html from the specified directory
     * @param {string} [directory=""] - Directory path to append to URL (e.g., "/dir/subdir")
     * @returns {Promise<string>} - Promise resolving to the HTML content
     */
    async downloadIndex(directory = "") {
      try {
        // Construct the URL with directory parameter if provided
        const base = this.pi_endpoint;
        let url = `${base}/index.html`;
        if (directory) {
          // Encode the directory path and format as index.html?[DIR]&/dir/subdir
          const encodedDir = encodeURIComponent(directory);
          url = `${base}/index.html?[DIR]&${encodedDir}`;
        }

        console.log(`[proxyStore] Fetching URL: ${url}`);

        const resp = await fetch(url);
        const text = await resp.text();
        //console.log("[proxyStore] downloadIndex result:\n", text);
        return text;
      } catch (err) {
        console.error("[proxyStore] downloadIndex error:", err);
        return null;
      }
    },

    /**
     * Parses HTML index content and extracts current path, directories, and files
     * @param {string} htmlString - The HTML content to parse
     * @returns {object} - Object containing currentPath, directories, and files arrays
     */
    parseIndex(htmlString) {
      try {
        // Create a temporary DOM element to parse the HTML
        const parser = new DOMParser();
        const doc = parser.parseFromString(htmlString, "text/html");

        // Helper function to find elements containing specific text
        const findElementsContainingText = (selector, text) => {
          const allElements = doc.querySelectorAll(selector);
          return Array.from(allElements).filter((el) =>
            el.textContent.includes(text),
          );
        };

        // Extract current path - look for div containing "Current path:" and get the italic element
        const currentPathDivs = findElementsContainingText(
          "div",
          "Current path:",
        );
        let currentPath = null;
        if (currentPathDivs.length > 0) {
          const iElement = currentPathDivs[0].querySelector("i");
          if (iElement) {
            currentPath = iElement.textContent.trim();
          }
        }

        // Extract directory names - look in the directories table (first column of main table)
        const directories = [];
        const dirTables = doc.querySelectorAll(
          "table.dirs > tbody > tr > td:nth-child(1) table.dirs",
        );

        dirTables.forEach((dirTable) => {
          const dirRows = dirTable.querySelectorAll("tr");
          dirRows.forEach((row) => {
            const firstCell = row.querySelector("td:first-child");
            if (firstCell && firstCell.querySelector("a")) {
              const dirName = firstCell.querySelector("a").textContent.trim();
              directories.push(dirName);
            }
          });
        });

        // Extract file names - look in the files table (second column of main table)
        const files = [];
        const fileTables = doc.querySelectorAll(
          "table.dirs > tbody > tr > td:nth-child(2) table.dirs",
        );

        fileTables.forEach((fileTable) => {
          const fileRows = fileTable.querySelectorAll("tr");
          fileRows.forEach((row) => {
            const firstCell = row.querySelector("td:first-child");
            if (firstCell && firstCell.querySelector("a")) {
              const fileName = firstCell.querySelector("a").textContent.trim();
              if (this.isCommodoreFile(fileName)) {
                if (!files.includes(fileName)) {
                  files.push(fileName);
                }
              }
            }
          });
        });

        return {
          currentPath: currentPath,
          directories: directories,
          files: files,
        };
      } catch (err) {
        console.error("[proxyStore] parseIndex error:", err);
        return {
          currentPath: null,
          directories: [],
          files: [],
        };
      }
    },

    /**
     * Downloads the index.html, parses it, and transforms data into discStore structure
     * @returns {Promise<object>} - Promise resolving to the parsed and transformed data
     */
    async processIndex(directory = "") {
      try {
        this.connecting = true;
        this.connectionError = null;
        console.log(
          `[proxyStore] Starting processIndex for directory: ${directory || "root"}`,
        );

        // Download the index.html content for the specified directory
        const htmlContent = await this.downloadIndex(directory);

        if (!htmlContent) {
          console.error("[proxyStore] processIndex: No HTML content received");
          this.connectionError = "Device unreachable";
          Alpine.store("toast").error("Device unreachable");
          return null;
        }

        // Parse the HTML content
        const parsedData = this.parseIndex(htmlContent);

        // Output the parsed results to console
        console.log("[proxyStore] Parsed data:", {
          currentPath: parsedData.currentPath,
          directories: parsedData.directories,
          files: parsedData.files,
        });

        // Transform data into discStore structure with loaded flags
        const transformedData = this.transformToDiscStoreStructureWithLoaded(
          parsedData,
          directory,
          true,
        );

        // Only initialize discStore root when loading the root directory
        if (!directory) {
          if (
            typeof Alpine !== "undefined" &&
            Alpine.store &&
            Alpine.store("disc")
          ) {
            Alpine.store("disc").root = transformedData;
            console.log("[proxyStore] discStore initialized with parsed data");
            if (Alpine.store("cache")) Alpine.store("cache").saveTree(transformedData);
          } else {
            console.warn(
              "[proxyStore] discStore not available for initialization",
            );
          }
        }

        this.connectionError = null;
        return transformedData;
      } catch (err) {
        console.error("[proxyStore] processIndex error:", err);
        this.connectionError = "Device unreachable";
        Alpine.store("toast").error("Device unreachable");
        return null;
      } finally {
        this.connecting = false;
      }
    },

    /**
     * Transforms parsed data into the structure expected by discStore
     * @param {object} parsedData - Data from parseIndex()
     * @returns {object} - Data in discStore structure format
     */
    transformToDiscStoreStructure(parsedData) {
      try {
        // Extract current path and remove "SD:/1541" prefix if present
        const currentPath = parsedData.currentPath || "/";
        const cleanPath =
          currentPath.replace("SD:/1541", "").replace("SD:", "") || "/";

        // Create root structure
        const root = {
          path: "/",
          date_string: this.getCurrentDateString(),
          subdirectories: [],
          disc_images: [],
        };

        // Add directories from parsed data
        parsedData.directories.forEach((dirName) => {
          root.subdirectories.push({
            path: `/${dirName}`,
            date_string: this.getCurrentDateString(),
            subdirectories: [],
            disc_images: [],
          });
        });

        // Add files from parsed data
        parsedData.files.forEach((fileName) => {
          root.disc_images.push({
            path: `/${fileName}`,
            date_string: this.getCurrentDateString(),
          });
        });

        return root;
      } catch (err) {
        console.error("[proxyStore] transformToDiscStoreStructure error:", err);
        return {
          path: "/",
          date_string: this.getCurrentDateString(),
          subdirectories: [],
          disc_images: [],
        };
      }
    },

    /**
     * Checks if a filename has a common Commodore file extension
     * @param {string} fileName - The filename to check
     * @returns {boolean} - True if the file has a Commodore extension
     */
    isCommodoreFile(fileName) {
      const lower = fileName.toLowerCase();
      return lower.endsWith(".d64") || lower.endsWith(".g64") || lower.endsWith(".lst");
    },

    /**
     * Gets current date in YYYY-MM-DD format
     * @returns {string} - Current date string
     */
    getCurrentDateString() {
      const now = new Date();
      return now.toISOString().split("T")[0];
    },

    /**
     * Transforms parsed data into discStore structure with loaded flags
     * @param {object} parsedData - Data from parseIndex()
     * @param {string} [directory=""] - Directory path being processed
     * @param {boolean} [isLoaded=true] - Whether this directory should be marked as loaded
     * @returns {object} - Data in discStore structure format with loaded flags
     */
    transformToDiscStoreStructureWithLoaded(
      parsedData,
      directory = "",
      isLoaded = true,
    ) {
      try {
        // Extract current path and remove "SD:/1541" prefix if present
        const currentPath = parsedData.currentPath || "/";
        const cleanPath =
          currentPath.replace("SD:/1541", "").replace("SD:", "") || "/";

        // Create root structure - if directory is specified, create a directory-specific structure
        let root;
        if (directory) {
          root = {
            path: cleanPath.startsWith("/") ? cleanPath : `/${cleanPath}`,
            date_string: this.getCurrentDateString(),
            loaded: isLoaded,
            subdirectories: [],
            disc_images: [],
          };
        } else {
          root = {
            path: "/",
            date_string: this.getCurrentDateString(),
            loaded: isLoaded,
            subdirectories: [],
            disc_images: [],
          };
        }

        // Add directories from parsed data - mark them as not loaded
        parsedData.directories.forEach((dirName) => {
          // Skip ".." entry as it's handled explicitly in the UI
          if (dirName === "..") {
            return;
          }

          // Build the correct path for subdirectories
          let dirPath;
          if (directory) {
            dirPath = `${cleanPath}/${dirName}`;
          } else {
            dirPath = `/${dirName}`;
          }

          // Ensure path starts with / and has no double slashes
          if (!dirPath.startsWith("/")) {
            dirPath = `/${dirPath}`;
          }
          dirPath = dirPath.replace("//", "/");

          root.subdirectories.push({
            path: dirPath,
            date_string: this.getCurrentDateString(),
            loaded: false,
            subdirectories: [],
            disc_images: [],
          });
        });

        // Add files from parsed data
        parsedData.files.forEach((fileName) => {
          const filePath = root.path === "/" ? `/${fileName}` : `${root.path}/${fileName}`;
          root.disc_images.push({
            path: filePath,
            date_string: this.getCurrentDateString(),
          });
        });

        return root;
      } catch (err) {
        console.error(
          "[proxyStore] transformToDiscStoreStructureWithLoaded error:",
          err,
        );
        return {
          path: directory ? `/${directory}` : "/",
          date_string: this.getCurrentDateString(),
          loaded: false,
          subdirectories: [],
          disc_images: [],
        };
      }
    },

    /**
     * Downloads mount-imgs.html for a specific file to get its details or mount it
     * @param {string} filePath - File path (e.g., "/games/RTYPE  9DMHI.D64")
     * @param {string} [action="FILE"] - URL action component: "FILE" to view, "MOUNT" to mount
     * @returns {Promise<string>} - Promise resolving to the HTML content
     */
    async downloadFileDetails(filePath, action = "FILE") {
      try {
        const encodedPath = encodeURIComponent(filePath);
        const base = this.pi_endpoint;
        const url = `${base}/mount-imgs.html?[${action}]&${encodedPath}`;

        console.log(`[proxyStore] Fetching file details URL: ${url}`);

        const resp = await fetch(url);
        const text = await resp.text();
        return text;
      } catch (err) {
        console.error("[proxyStore] downloadFileDetails error:", err);
        return null;
      }
    },

    /**
     * Mounts a file on the Pi1541 device
     * @param {string} filePath - File path (e.g., "/games/RTYPE  9DMHI.D64")
     * @returns {Promise<void>}
     */
    async mountFile(filePath) {
      try {
        this.connecting = true;
        this.connectionError = null;
        console.log(`[proxyStore] Mounting file: ${filePath}`);
        await this.downloadFileDetails(filePath, "MOUNT");
        console.log(`[proxyStore] Mounted: ${filePath}`);
        Alpine.store("disc").mountedFile = filePath;
        this.connectionError = null;
        Alpine.store("toast").success(filePath.split("/").pop(), "Mounted");
      } catch (err) {
        console.error("[proxyStore] mountFile error:", err);
        this.connectionError = "Failed to mount file";
        Alpine.store("toast").error("Failed to mount file");
        return;
      } finally {
        this.connecting = false;
      }
      await new Promise(resolve => setTimeout(resolve, 200));
      await this.processStats();
    },

    /**
     * Parses mount-imgs HTML and extracts selected path and disk content
     * @param {string} htmlString - The HTML content to parse
     * @returns {object} - Object containing selectedPath and disc_content
     */
    parseFileDetails(htmlString) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(htmlString, "text/html");

        // Extract selected path from <p>Selected <i>SD:/1541/...</i></p>
        let selectedPath = null;
        const paragraphs = doc.querySelectorAll("p");
        for (const p of paragraphs) {
          if (p.textContent.includes("Selected")) {
            const iElement = p.querySelector("i");
            if (iElement) {
              selectedPath = iElement.textContent
                .trim()
                .replace("SD:/1541", "")
                .replace("SD:", "") || "/";
            }
            break;
          }
        }

        // Extract PETSCII disk content from the 3rd column of the directory table
        let disc_content = null;
        const dirsTables = doc.querySelectorAll("table.dirs");
        // The second table.dirs at top level contains the directory/file/content columns
        // Find the table that has Directories/Files/Image content headers
        for (const table of dirsTables) {
          const thirdCell = table.querySelector("tr[valign='top'] > td:nth-child(3)");
          if (thirdCell) {
            disc_content = thirdCell.innerHTML;
            break;
          }
        }

        return {
          selectedPath: selectedPath,
          disc_content: disc_content,
        };
      } catch (err) {
        console.error("[proxyStore] parseFileDetails error:", err);
        return {
          selectedPath: null,
          disc_content: null,
        };
      }
    },

    /**
     * Converts PETSCII-encoded text (U+0EExx codepoints) to readable ASCII.
     * Alphanumeric and common punctuation are converted; all others become '?'.
     * HTML <br> / <br /> tags are converted to newlines.
     * @param {string} petsciiHtml - HTML string containing PETSCII-encoded characters
     * @returns {string} - ASCII representation
     */
    petsciiToAscii(petsciiHtml) {
      // Replace <br> tags with newline markers, then strip remaining HTML tags
      const text = petsciiHtml
        .replace(/<br\s*\/?>/gi, "\n")
        .replace(/<[^>]*>/g, "");

      // Decode HTML entities (&#x0ee30; etc.) by using a textarea
      const textarea = document.createElement("textarea");
      textarea.innerHTML = text;
      const decoded = textarea.value;

      let result = "";
      for (const char of decoded) {
        const code = char.codePointAt(0);

        if (code >= 0x0ee00 && code <= 0x0eeff) {
          let petscii = code - 0x0ee00;

          // 0x80-0xFF are reverse video versions of 0x00-0x7F
          if (petscii >= 0x80) {
            petscii -= 0x80;
          }

          // Space (0x20) and shifted space (0x60)
          if (petscii === 0x20 || petscii === 0x60) {
            result += " ";
          }
          // Digits 0-9
          else if (petscii >= 0x30 && petscii <= 0x39) {
            result += String.fromCharCode(petscii);
          }
          // A-Z (PETSCII unshifted: 0x41-0x5A)
          else if (petscii >= 0x41 && petscii <= 0x5a) {
            result += String.fromCharCode(petscii);
          }
          // A-Z (PETSCII shifted/lowercase: 0x01-0x1A → A-Z)
          else if (petscii >= 0x01 && petscii <= 0x1a) {
            result += String.fromCharCode(0x40 + petscii);
          }
          // Common punctuation
          else if (
            ".,:;!?()-+*/=<>\"'@#$%&".includes(String.fromCharCode(petscii))
          ) {
            result += String.fromCharCode(petscii);
          }
          // Everything else (graphic chars, control codes)
          else {
            result += "?";
          }
        } else if (char === "\n") {
          result += "\n";
        }
        // Regular ASCII passthrough
        else if (code >= 0x20 && code <= 0x7e) {
          result += char;
        }
      }

      return result;
    },

    /**
     * Recursively loads all subdirectories from the device.
     * Manages connecting/connectionError at the top level to avoid flicker.
     */
    // Internal: recursively load unloaded subdirectories, throws on first failure
    async _loadDirsRecursive(startPath) {
      const disc = Alpine.store("disc");

      const startDir = startPath === "/" ? disc.root : disc.findDirectoryByPath(startPath);
      if (!startDir) throw new Error(`Directory not found: ${startPath}`);

      if (!startDir.loaded) {
        const relPath = startPath === "/" ? "" : startPath.substring(1);
        console.log(`[proxyStore] loadDirs: loading ${startPath}`);
        const html = await this.downloadIndex(relPath);
        if (!html) throw new Error(`Failed to load ${startPath}`);
        const parsed = this.parseIndex(html);
        const data = this.transformToDiscStoreStructureWithLoaded(parsed, relPath, true);
        startDir.loaded = true;
        startDir.subdirectories = data.subdirectories || [];
        startDir.disc_images = data.disc_images || [];
      }

      const loadDir = async (directory) => {
        for (const subdir of directory.subdirectories) {
          if (!subdir.loaded) {
            const relPath = subdir.path.startsWith("/") ? subdir.path.substring(1) : subdir.path;
            console.log(`[proxyStore] loadDirs: loading ${subdir.path}`);
            const html = await this.downloadIndex(relPath);
            if (!html) throw new Error(`Failed to load ${subdir.path}`);
            const parsed = this.parseIndex(html);
            const data = this.transformToDiscStoreStructureWithLoaded(parsed, relPath, true);
            subdir.loaded = true;
            subdir.subdirectories = data.subdirectories || [];
            subdir.disc_images = data.disc_images || [];
          }
          await loadDir(subdir);
        }
      };

      await loadDir(startDir);
    },

    // Internal: recursively load file details, throws on first failure
    async _loadFileDetailsRecursive(startPath) {
      const disc = Alpine.store("disc");

      const startDir = startPath === "/" ? disc.root : disc.findDirectoryByPath(startPath);
      if (!startDir) throw new Error(`Directory not found: ${startPath}`);

      const loadDetails = async (directory) => {
        for (const image of directory.disc_images) {
          if (image.disc_content) continue;
          const filePath = image.path;
          console.log(`[proxyStore] loadDetails: loading ${filePath}`);
          const html = await this.downloadFileDetails(filePath);
          if (!html) throw new Error(`Failed to load details for ${filePath}`);
          const details = this.parseFileDetails(html);
          if (details.disc_content) {
            image.disc_content = details.disc_content;
            image.disc_ascii = Alpine.store("disc").computeDiscAscii(details.disc_content);
            if (Alpine.store("cache")) Alpine.store("cache").saveDiscContent(filePath, details.disc_content);
          }
        }
        for (const subdir of directory.subdirectories) {
          await loadDetails(subdir);
        }
      };

      await loadDetails(startDir);
    },

    /**
     * Downloads pistats.html from the Pi1541 device
     * @returns {Promise<string|null>} - Promise resolving to the HTML content
     */
    async downloadStats() {
      try {
        const url = `${this.pi_endpoint}/pistats.html`;
        console.log(`[proxyStore] Fetching stats URL: ${url}`);
        const resp = await fetch(url);
        const text = await resp.text();
        return text;
      } catch (err) {
        console.error("[proxyStore] downloadStats error:", err);
        return null;
      }
    },

    /**
     * Parses pistats HTML and extracts device status fields
     * @param {string} htmlString - The HTML content to parse
     * @returns {object} - Object containing deviceId, currentDiskimage, piTemp, time
     */
    parseStats(htmlString) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(htmlString, "text/html");
        const italics = doc.querySelectorAll("i");
        const tempRaw = italics[2]?.textContent.trim() ?? null;
        const tempParts = tempRaw ? tempRaw.split(" @") : [];
        return {
          deviceId:         italics[0]?.textContent.trim() ?? null,
          currentDiskimage: italics[1]?.textContent.trim().replace("SD:/1541", "").replace("SD:", "") || null,
          piTemp:           tempParts[0] ?? null,
          piFreq:           tempParts[1] ?? null,
          time:             italics[3]?.textContent.trim() ?? null,
        };
      } catch (err) {
        console.error("[proxyStore] parseStats error:", err);
        return { deviceId: null, currentDiskimage: null, piTemp: null, piFreq: null, time: null };
      }
    },

    /**
     * Downloads and parses pistats.html, updating this.stats
     * @returns {Promise<object|null>} - Promise resolving to { deviceId, currentDiskimage, piTemp, time }
     */
    async processStats() {
      try {
        this.statsLoading = true;
        const htmlContent = await this.downloadStats();
        if (!htmlContent) {
          console.error("[proxyStore] processStats: No HTML content received");
          Alpine.store("toast").error("Could not fetch Pi stats");
          return null;
        }
        const stats = this.parseStats(htmlContent);
        console.log("[proxyStore] Parsed stats:", stats);
        this.stats = stats;
        return stats;
      } catch (err) {
        console.error("[proxyStore] processStats error:", err);
        Alpine.store("toast").error("Could not fetch Pi stats");
        return null;
      } finally {
        this.statsLoading = false;
      }
    },

    /**
     * Recursively loads all subdirectories starting from the given path.
     * @param {string} [startPath="/"] - Directory path to start from
     */
    async initIndexRecursive(startPath = "/") {
      this.connecting = true;
      this.connectionError = null;
      Alpine.store("toast").info("Refreshing directories\u2026");
      try {
        await this._loadDirsRecursive(startPath);
        if (Alpine.store("cache")) Alpine.store("cache").saveTree(Alpine.store("disc").root);
        console.log("[proxyStore] initIndexRecursive: complete");
      } catch (err) {
        console.error("[proxyStore] initIndexRecursive error:", err);
        this.connectionError = err.message;
      } finally {
        this.connecting = false;
      }
    },

    /**
     * Loads all subdirectories then all file details starting from the given path.
     * @param {string} [startPath="/"] - Directory path to start from
     */
    async initFileDetailsRecursive(startPath = "/") {
      this.connecting = true;
      this.connectionError = null;
      Alpine.store("toast").info("Refreshing disc details\u2026");
      try {
        await this._loadDirsRecursive(startPath);
        if (Alpine.store("cache")) Alpine.store("cache").saveTree(Alpine.store("disc").root);
        await this._loadFileDetailsRecursive(startPath);
        console.log("[proxyStore] initFileDetailsRecursive: complete");
      } catch (err) {
        console.error("[proxyStore] initFileDetailsRecursive error:", err);
        this.connectionError = err.message;
      } finally {
        this.connecting = false;
      }
    },

    /**
     * Downloads and parses file details from mount-imgs.html
     * @param {string} filePath - File path (e.g., "/games/RTYPE  9DMHI.D64")
     * @returns {Promise<object>} - Promise resolving to { selectedPath, disc_content }
     */
    async processFileDetails(filePath) {
      try {
        this.connecting = true;
        this.connectionError = null;
        console.log(
          `[proxyStore] Starting processFileDetails for: ${filePath}`,
        );

        const htmlContent = await this.downloadFileDetails(filePath);

        if (!htmlContent) {
          console.error(
            "[proxyStore] processFileDetails: No HTML content received",
          );
          this.connectionError = "Device unreachable";
          return null;
        }

        const parsedData = this.parseFileDetails(htmlContent);

        console.log("[proxyStore] Parsed file details:", {
          selectedPath: parsedData.selectedPath,
        });
        if (parsedData.disc_content) {
          console.log(
            "[proxyStore] Disk directory:\n" +
              this.petsciiToAscii(parsedData.disc_content),
          );
        }

        this.connectionError = null;
        return parsedData;
      } catch (err) {
        console.error("[proxyStore] processFileDetails error:", err);
        this.connectionError = "Device unreachable";
        return null;
      } finally {
        this.connecting = false;
      }
    },
  });
});
