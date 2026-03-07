// Alpine.js Pi Stats Store
// Handles Pi1541 device stats: fetching, parsing, and display state.
// Reads pi_endpoint from proxyStore so configuration stays in one place.

document.addEventListener("alpine:init", () => {
  Alpine.store("piStats", {
    stats: { deviceId: null, currentDiskimage: null, piTemp: null, piFreq: null, time: null },
    statsLoading: false,

    /**
     * Downloads pistats.html from the Pi1541 device
     * @returns {Promise<string|null>}
     */
    async downloadStats() {
      try {
        const url = `${Alpine.store("proxy").pi_endpoint}/pistats.html`;
        console.log(`[piStatsStore] Fetching stats URL: ${url}`);
        const resp = await fetch(url);
        const text = await resp.text();
        return text;
      } catch (err) {
        console.error("[piStatsStore] downloadStats error:", err);
        return null;
      }
    },

    /**
     * Parses pistats HTML and extracts device status fields
     * @param {string} htmlString
     * @returns {object}
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
        console.error("[piStatsStore] parseStats error:", err);
        return { deviceId: null, currentDiskimage: null, piTemp: null, piFreq: null, time: null };
      }
    },

    /**
     * Downloads and parses pistats.html, updating this.stats
     * @returns {Promise<object|null>}
     */
    async processStats() {
      try {
        this.statsLoading = true;
        const htmlContent = await this.downloadStats();
        if (!htmlContent) {
          console.error("[piStatsStore] processStats: No HTML content received");
          Alpine.store("toast").error("Could not fetch Pi stats");
          return null;
        }
        const stats = this.parseStats(htmlContent);
        console.log("[piStatsStore] Parsed stats:", stats);
        this.stats = stats;
        return stats;
      } catch (err) {
        console.error("[piStatsStore] processStats error:", err);
        Alpine.store("toast").error("Could not fetch Pi stats");
        return null;
      } finally {
        this.statsLoading = false;
      }
    },
  });
});
