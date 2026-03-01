// Alpine.js Favourites Store
// Manages favourite disc images and directories, persisted to localStorage.

document.addEventListener("alpine:init", () => {
  Alpine.store("favourites", {
    items: [],

    toggle(path, type) {
      if (this.isFavourite(path)) {
        this.remove(path);
        Alpine.store("toast").info(path.split("/").pop() || path, "Removed from favourites");
      } else {
        this.items.push({
          path,
          type,
          addedDate: new Date().toISOString(),
        });
        this._save();
        Alpine.store("toast").info(path.split("/").pop() || path, "Added to favourites");
      }
    },

    toggleDetail(discPath, line) {
      if (this.isDetailFavourite(discPath, line)) {
        this.items = this.items.filter(
          (f) => !(f.type === "detail" && f.discPath === discPath && f.line === line),
        );
      } else {
        this.items.push({
          path: discPath,
          discPath,
          type: "detail",
          line,
          addedDate: new Date().toISOString(),
        });
      }
      this._save();
    },

    isDetailFavourite(discPath, line) {
      return this.items.some(
        (f) => f.type === "detail" && f.discPath === discPath && f.line === line,
      );
    },

    isFavourite(path) {
      return this.items.some((f) => f.path === path && f.type !== "detail");
    },

    remove(path) {
      this.items = this.items.filter((f) => f.path !== path || f.type === "detail");
      this._save();
    },

    removeDetail(discPath, line) {
      this.items = this.items.filter(
        (f) => !(f.type === "detail" && f.discPath === discPath && f.line === line),
      );
      this._save();
    },

    _save() {
      try {
        localStorage.setItem(
          "pi1541ui_favourites",
          JSON.stringify(this.items),
        );
      } catch (e) {
        console.warn("[favouritesStore] save failed:", e);
      }
    },

    _load() {
      try {
        const json = localStorage.getItem("pi1541ui_favourites");
        this.items = json ? JSON.parse(json) : [];
      } catch (e) {
        console.warn("[favouritesStore] load failed:", e);
        this.items = [];
      }
    },
  });

  Alpine.store("favourites")._load();
});
