// Alpine.js Cache Store
// Persists the disc tree to localStorage and disc_content to IndexedDB.
// On startup, restores both back into discStore.

document.addEventListener("alpine:init", () => {
  Alpine.store("cache", {
    _db: null,
    restoring: false,

    async _openDB() {
      if (this._db) return this._db;
      return new Promise((resolve, reject) => {
        const req = indexedDB.open("pi1541ui", 1);
        req.onupgradeneeded = (e) => {
          e.target.result.createObjectStore("disc_content");
        };
        req.onsuccess = (e) => {
          this._db = e.target.result;
          resolve(this._db);
        };
        req.onerror = (e) => reject(e.target.error);
      });
    },

    async saveDiscContent(path, content) {
      try {
        const db = await this._openDB();
        return new Promise((resolve, reject) => {
          const tx = db.transaction("disc_content", "readwrite");
          tx.objectStore("disc_content").put(content, path);
          tx.oncomplete = () => resolve();
          tx.onerror = (e) => reject(e.target.error);
        });
      } catch (e) {
        console.warn("[cacheStore] saveDiscContent failed:", e);
      }
    },

    async _getAllDiscContent() {
      const db = await this._openDB();
      return new Promise((resolve, reject) => {
        const tx = db.transaction("disc_content", "readonly");
        const store = tx.objectStore("disc_content");
        const keys = [], values = [];
        store.openCursor().onsuccess = (e) => {
          const cursor = e.target.result;
          if (cursor) {
            keys.push(cursor.key);
            values.push(cursor.value);
            cursor.continue();
          }
        };
        tx.oncomplete = () => {
          const result = {};
          keys.forEach((k, i) => (result[k] = values[i]));
          resolve(result);
        };
        tx.onerror = (e) => reject(e.target.error);
      });
    },

    saveTree(root) {
      try {
        localStorage.setItem(
          "pi1541ui_tree",
          JSON.stringify(this._stripDiscContent(root)),
        );
      } catch (e) {
        console.warn("[cacheStore] saveTree failed:", e);
      }
    },

    _loadTree() {
      try {
        const json = localStorage.getItem("pi1541ui_tree");
        return json ? JSON.parse(json) : null;
      } catch (e) {
        console.warn("[cacheStore] loadTree failed:", e);
        return null;
      }
    },

    _stripDiscContent(dir) {
      return {
        ...dir,
        disc_images: (dir.disc_images || []).map(
          ({ disc_content, disc_ascii, ...img }) => img,
        ),
        subdirectories: (dir.subdirectories || []).map((d) =>
          this._stripDiscContent(d),
        ),
      };
    },

    async restoreAll() {
      this.restoring = true;
      try {
        const contents = await this._getAllDiscContent();
        const count = Object.keys(contents).length;
        if (count > 0) {
          const disc = Alpine.store("disc");
          for (const [path, content] of Object.entries(contents)) {
            const image = disc.findDiscImageByPath(path);
            if (image) {
              image.disc_content = content;
              image.disc_ascii = disc.computeDiscAscii(content);
            }
          }
          console.log(`[cacheStore] Restored ${count} disc_content entries from IndexedDB`);
        }
      } catch (e) {
        console.error("[cacheStore] restoreAll failed:", e);
      } finally {
        this.restoring = false;
      }
    },

    clearAll() {
      localStorage.removeItem("pi1541ui_tree");
      this._openDB().then((db) => {
        const tx = db.transaction("disc_content", "readwrite");
        tx.objectStore("disc_content").clear();
        console.log("[cacheStore] Cache cleared");
      });
      Alpine.store("toast").info("Cache cleared");
      Alpine.store("disc").root = {
        path: "/",
        date_string: new Date().toISOString().split("T")[0],
        loaded: false,
        subdirectories: [],
        disc_images: [],
      };
      Alpine.store("proxy").processIndex();
    },
  });

  Alpine.store("cache").restoreAll();
});
