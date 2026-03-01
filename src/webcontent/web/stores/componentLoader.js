// Alpine.js Component Loader Store
// Handles dynamic loading of UI components

document.addEventListener("alpine:init", () => {
  Alpine.store("componentLoader", {
    // Component configuration
    components: {
      "pi-stats": {
        containerId: "pi-stats-component",
        filePath: "components/pi-stats.html",
      },
"favourites-panel": {
        containerId: "favourites-panel-component",
        filePath: "components/favourites-panel.html",
      },
      "file-browser": {
        containerId: "file-browser-component",
        filePath: "components/file-browser.html",
      },
      "operations-panel": {
        containerId: "operations-panel-component",
        filePath: "components/operations-panel.html",
      },
      "csdb-search": {
        containerId: "csdb-search-component",
        filePath: "components/csdb-search.html",
      },
      "config": {
        containerId: "config-component",
        filePath: "components/config.html",
      },
    },

    // Initialize all components and load remote index
    async init() {
      await this.loadAllComponents();
      // Skip auto-fetch if the tree was already loaded from localStorage
      if (Alpine.store("disc").root.loaded) {
        console.log("[componentLoader] Tree loaded from cache — skipping auto-fetch");
        return;
      }
      try {
        await Alpine.store("proxy").processIndex();
      } catch (err) {
        console.warn("[componentLoader] Could not load remote index:", err);
      }
    },

    // Load all components
    async loadAllComponents() {
      for (const [componentName, config] of Object.entries(this.components)) {
        await this.loadComponent(componentName);
      }
    },

    // Load a single component
    async loadComponent(componentName) {
      const config = this.components[componentName];
      if (!config) {
        console.warn(`Component ${componentName} not found in configuration`);
        return;
      }

      try {
        const response = await fetch(config.filePath);
        if (!response.ok) {
          throw new Error(
            `Failed to load ${componentName}: ${response.status}`,
          );
        }

        const html = await response.text();
        const container = document.getElementById(config.containerId);

        if (container) {
          container.innerHTML = html;
          console.log(`Loaded ${componentName} component`);
        } else {
          console.error(
            `Container ${config.containerId} not found for ${componentName}`,
          );
        }
      } catch (error) {
        console.error(`Error loading ${componentName}:`, error);
      }
    },

    // Load a component by name (for dynamic loading)
    loadComponentByName(name) {
      if (this.components[name]) {
        return this.loadComponent(name);
      }
      console.warn(`Component ${name} not configured`);
    },
  });

  // Initialize the component loader when Alpine starts
  Alpine.store("componentLoader").init();
});
