// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 skyie
//
// This file is part of pi1541ui.
// pi1541ui is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// See the LICENSE file for details.

// Alpine.js Component Loader Store
// Handles dynamic loading of UI components

document.addEventListener("alpine:init", () => {
  Alpine.store("componentLoader", {
    // Component configuration
    components: {
"favourites-panel": {
        containerId: "favourites-panel-component",
        filePath: "components/favourites-panel.html",
      },
      "file-browser": {
        containerId: "file-browser-component",
        filePath: "components/file-browser.html",
      },
      "upload-panel": {
        containerId: "upload-panel-component",
        filePath: "components/upload-panel.html",
      },
      "disc-detail": {
        containerId: "disc-detail-component",
        filePath: "components/disc-detail.html",
      },
      "operations-panel": {
        containerId: "operations-panel-component",
        filePath: "components/operations-panel.html",
      },
      "csdb-search": {
        containerId: "csdb-search-component",
        filePath: "components/csdb-search.html",
      },
      "releases-panel": {
        containerId: "releases-panel-component",
        filePath: "components/releases-panel.html",
      },
      "release-detail": {
        containerId: "release-detail-component",
        filePath: "components/release-detail.html",
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
      // upload-panel and disc-detail containers live inside file-browser,
      // so file-browser must be injected first.
      const needsFileBrowser = ['upload-panel', 'disc-detail'];
      const independent = Object.keys(this.components).filter(n => !needsFileBrowser.includes(n));

      await Promise.all(independent.map(n => this.loadComponent(n)));
      await Promise.all(needsFileBrowser.map(n => this.loadComponent(n)));
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

});
