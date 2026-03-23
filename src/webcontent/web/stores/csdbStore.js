// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 skyie
//
// This file is part of pi1541ui.
// pi1541ui is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// See the LICENSE file for details.

// Alpine.js CSDb Store
// Searches and browses the C64 Scene Database (csdb.dk)

document.addEventListener("alpine:init", () => {
  Alpine.store("csdb", {
    // State
    searching: false,
    error: null,
    results: { quick: [], group: [], scener: [], toplist: [] },
    resultCounts: { quick: 0, group: 0, scener: 0, toplist: 0 },
    csdb_endpoint: localStorage.getItem("csdb_endpoint") || "http://localhost:8000/csdb-proxy",
    csdb_base: localStorage.getItem("csdb_base") || "http://csdb.dk",

    setCsdbEndpoint(url) {
      this.csdb_endpoint = url;
      localStorage.setItem("csdb_endpoint", url);
    },

    setCsdbBase(url) {
      this.csdb_base = url;
      localStorage.setItem("csdb_base", url);
    },

    effectiveCsdbEndpoint() {
      return Alpine.store("proxy").endpoint_configuration === "custom"
        ? this.csdb_endpoint
        : window.location.origin + "/web/pi1541-proxy.html";
    },

    /**
     * Build a full CSDb URL from a path.
     * @param {string} csdbPath - Path + query string for csdb.dk (e.g. "/search/?seinsel=releases&search=foo")
     */
    _csdbUrl(csdbPath) {
      return `${this.effectiveCsdbEndpoint()}?${this.csdb_base}${csdbPath}`;
    },

    /**
     * Detect the page type from CSDb HTML content.
     * @param {string} html - HTML content
     * @returns {string} - 'release', 'group', 'scener', or 'search'
     */
    _detectPageType(html) {
      if (/<meta property="og:url" content="[^"]*\/release\/\?id=\d+"/.test(html)) return "release";
      if (/<meta property="og:url" content="[^"]*\/group\/\?id=\d+"/.test(html))   return "group";
      if (/<meta property="og:url" content="[^"]*\/scener\/\?id=\d+"/.test(html))  return "scener";
      if (/<title>\[CSDb\]\s*-\s*Group:/i.test(html))   return "group";
      if (/<title>\[CSDb\]\s*-\s*Scener:/i.test(html))  return "scener";
      return "search";
    },

    /**
     * Generic fetch for CSDb pages. Detects the resolved page type.
     * The proxy must follow redirects server-side; the browser uses redirect: 'follow' (default).
     * @param {string} csdbPath - Path + query string for csdb.dk
     * @returns {Promise<{html: string, pageType: string, status: number}>}
     */
    async _fetchCsdb(csdbPath) {
      const url = this._csdbUrl(csdbPath);
      console.log(`[csdbStore] _fetchCsdb: ${url}`);
      const resp = await fetch(url);
      if (!resp.ok) throw new Error(`Network error (HTTP ${resp.status})`);
      const html = await resp.text();
      const pageType = this._detectPageType(html);
      console.log(`[csdbStore] _fetchCsdb: ${url} → status=${resp.status} pageType=${pageType}`);
      return { html, pageType, status: resp.status };
    },

    /**
     * Generic blob fetch for CSDb binary downloads.
     * @param {string} url - Full proxy URL
     * @returns {Promise<{blob: Blob, status: number}>}
     */
    async _fetchCsdbBlob(url) {
      console.log(`[csdbStore] _fetchCsdbBlob: ${url}`);
      const resp = await fetch(url);
      if (!resp.ok) throw new Error("HTTP " + resp.status);
      const blob = await resp.blob();
      console.log(`[csdbStore] _fetchCsdbBlob: ${url} → ${blob.size} bytes`);
      return { blob, status: resp.status };
    },

    // Group / Scener browse state
    groupInfo: null,     // { id, name, country }
    scenerInfo: null,    // { id, name, country }
    groupMatches: [],    // multiple matches waiting for user selection
    scenerMatches: [],   // multiple matches waiting for user selection

    // Release type IDs for the advanced search dropdown
    releaseTypes: [
      { id: 1, label: "C64 Demo" },
      { id: 2, label: "C64 One-File Demo" },
      { id: 3, label: "C64 Intro" },
      { id: 4, label: "C64 4K Intro" },
      { id: 5, label: "C64 Crack Intro" },
      { id: 7, label: "C64 Music" },
      { id: 9, label: "C64 Graphics" },
      { id: 11, label: "C64 Game" },
      { id: 12, label: "C64 32K Game" },
      { id: 13, label: "C64 Diskmag" },
      { id: 15, label: "C64 Tool" },
      { id: 18, label: "C64 1K Intro" },
      { id: 20, label: "C64 Crack" },
      { id: 36, label: "C64 256b Intro" },
    ],

    /**
     * Unified search entry point.
     * @param {string} type - 'quick' | 'group' | 'scener' | 'toplist'
     * @param {string|number} query - Search term or toplist subtype ID
     */
    async search(type, query) {
      if (type !== "toplist" && (!query || !String(query).trim())) return;
      this.searching = true;
      this.error = null;
      this.results[type] = [];
      this.resultCounts[type] = 0;
      if (type === "group") { this.groupInfo = null; this.groupMatches = []; }
      if (type === "scener") { this.scenerInfo = null; this.scenerMatches = []; }

      try {
        this.results[type] = await this._fetchResults(type, query);
        this.resultCounts[type] = this.results[type].length;
        console.log(`[csdbStore] search(${type}, '${query}'): ${this.resultCounts[type]} results`);
      } catch (err) {
        const notFound = /No (group|scener) found/i.test(err.message);
        if (notFound) console.info(`[csdbStore] search(${type}): ${err.message}`);
        else console.error(`[csdbStore] search(${type}) error:`, err);
        this.error = err.message;
        if (err.message.startsWith("Network error")) {
          Alpine.store('toast').error(err.message, 'CSDb');
        }
      } finally {
        this.searching = false;
      }
    },

    /**
     * Fetch and parse results for a given search type.
     * @param {string} type - 'quick' | 'group' | 'scener' | 'toplist'
     * @param {string|number} query
     * @returns {Promise<Array>}
     */
    async _fetchResults(type, query) {
      const encoded = encodeURIComponent(String(query).trim());

      switch (type) {
        case "quick": {
          const { html, pageType } = await this._fetchCsdb(`/search/?seinsel=releases&search=${encoded}`);
          if (pageType === "release") {
            const d = this.parseReleasePage(html);
            return d.id ? [{ id: d.id, name: d.name, type: d.type, group: d.group, date: d.date }] : [];
          }
          return this.parseQuickSearchResults(html);
        }
        case "group": {
          const { html: searchHtml, pageType } = await this._fetchCsdb(`/search/?seinsel=groups&search=${encoded}`);
          if (pageType === "group") {
            const id = this.parseGroupIdFromPage(searchHtml);
            if (!id) throw new Error("No group found");
            this.groupMatches = [];
            this.groupInfo = { id, name: this.parseGroupNameFromPage(searchHtml), country: "" };
            return this.parseGroupPage(searchHtml);
          }
          const groups = this.parseGroupSearchResults(searchHtml);
          if (!groups.length) throw new Error("No group found");
          if (groups.length === 1) {
            this.groupMatches = [];
            this.groupInfo = groups[0];
            const { html: groupHtml } = await this._fetchCsdb(`/group/?id=${groups[0].id}`);
            return this.parseGroupPage(groupHtml);
          }
          this.groupMatches = groups;
          this.groupInfo = null;
          return [];
        }
        case "scener": {
          const { html: searchHtml, pageType } = await this._fetchCsdb(`/search/?seinsel=sceners&search=${encoded}`);
          if (pageType === "scener") {
            const id = this.parseScenerIdFromPage(searchHtml);
            if (!id) throw new Error("No scener found");
            this.scenerMatches = [];
            this.scenerInfo = { id, name: this.parseScenerNameFromPage(searchHtml), country: "" };
            return this.parseScenerPage(searchHtml);
          }
          const sceners = this.parseScenerSearchResults(searchHtml);
          if (!sceners.length) throw new Error("No scener found");
          if (sceners.length === 1) {
            this.scenerMatches = [];
            this.scenerInfo = sceners[0];
            const { html: scenerHtml } = await this._fetchCsdb(`/scener/?id=${sceners[0].id}`);
            return this.parseScenerPage(scenerHtml);
          }
          this.scenerMatches = sceners;
          this.scenerInfo = null;
          return [];
        }
        case "toplist": {
          const { html } = await this._fetchCsdb(`/toplist.php?type=release&subtype=${encodeURIComponent(`(${query})`)}`);
          return this.parseToplistResults(html);
        }
        default:
          throw new Error(`Unknown search type: ${type}`);
      }
    },

    /**
     * Download a release file, storing result in downloadResult
     * @param {{ url: string, filename: string, externalUrl: string }} dl
     * @param {number} id - Release ID
     */
    async downloadFile(dl, id) {
      console.log(`[csdbStore] downloadFile: url=${dl.url}`);
      Alpine.store('release').downloading = true;
      try {
        let { blob } = await this._fetchCsdbBlob(dl.url);
        let zipContents = null;
        if (dl.filename.toLowerCase().endsWith('.zip')) {
          const zip = await JSZip.loadAsync(blob);
          zipContents = [];
          zip.forEach((path, file) => zipContents.push({ name: path, size: file._data?.uncompressedSize ?? null }));
        } else if (dl.filename.toLowerCase().endsWith('.gz')) {
          const ds = new DecompressionStream('gzip');
          const decompressed = await new Response(blob.stream().pipeThrough(ds)).blob();
          const innerName = dl.filename.replace(/\.gz$/i, '');
          zipContents = [{ name: innerName, size: decompressed.size }];
          blob = decompressed;
          dl = { ...dl, filename: innerName };
        }
        const downloadId = parseInt(new URL(dl.url, location.href).searchParams.get("id")) || null;
        const record = {
          id: downloadId,
          filename: dl.filename,
          url: dl.url,
          externalUrl: dl.externalUrl,
          size: blob.size,
          zipContents,
        };
        Alpine.store('release').downloadResult = { ...record, releaseId: id, blob };
        Alpine.store('toast').success(dl.filename, 'Downloaded');
      } catch (err) {
        Alpine.store('toast').error('Download failed: ' + err.message);
      } finally {
        Alpine.store('release').downloading = false;
      }
    },

    /**
     * Fetch details for a single release
     * @param {number} id - Release ID
     * @returns {Promise<object>} - { rating, votes, group, date, party, placement }
     */
    async fetchReleaseDetails(id) {
      try {
        const { html } = await this._fetchCsdb(`/release/?id=${id}`);
        return this.parseReleasePage(html);
      } catch (err) {
        console.error(`[csdbStore] fetchReleaseDetails error for ${id}:`, err);
        throw err;
      }
    },

    /**
     * Load releases for a group chosen from the disambiguation list.
     * @param {{ id, name, country }} group
     */
    async selectGroup(group) {
      this.groupMatches = [];
      this.groupInfo = group;
      this.searching = true;
      this.error = null;
      try {
        const { html } = await this._fetchCsdb(`/group/?id=${group.id}`);
        this.results.group = this.parseGroupPage(html);
        this.resultCounts.group = this.results.group.length;
      } catch (err) {
        console.error(`[csdbStore] selectGroup error:`, err);
        this.error = err.message;
        Alpine.store('toast').error(err.message, 'CSDb');
      } finally {
        this.searching = false;
      }
    },

    /**
     * Load releases for a scener chosen from the disambiguation list.
     * @param {{ id, name, country }} scener
     */
    async selectScener(scener) {
      this.scenerMatches = [];
      this.scenerInfo = scener;
      this.searching = true;
      this.error = null;
      try {
        const { html } = await this._fetchCsdb(`/scener/?id=${scener.id}`);
        this.results.scener = this.parseScenerPage(html);
        this.resultCounts.scener = this.results.scener.length;
      } catch (err) {
        console.error(`[csdbStore] selectScener error:`, err);
        this.error = err.message;
        Alpine.store('toast').error(err.message, 'CSDb');
      } finally {
        this.searching = false;
      }
    },

    // --- HTML Parsers ---

    /**
     * Parse quick search results (li elements with release links)
     * HTML pattern: <li><a href="/release/download.php?id=...">...</a><a href="/release/?id=ID">NAME</a> (TYPE) by <a href="/group/?id=...">GROUP</a> <font>(DATE)</font>
     */
    parseQuickSearchResults(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");
        const items = doc.querySelectorAll("ol > li");
        const results = [];

        for (const li of items) {
          const releaseLink = li.querySelector('a[href*="/release/?id="]');
          if (!releaseLink) continue;

          const idMatch = releaseLink.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const name = releaseLink.textContent.trim();
          const id = parseInt(idMatch[1]);

          // Extract type from parentheses after release link
          const liText = li.textContent;
          const typeMatch = liText.match(/\(([^)]*(?:Demo|Intro|Game|Crack|Music|Graphics|Diskmag|Tool|Misc|Release|Charts|Hardware|Papermag|Votesheet|Cover|Software|Collection)[^)]*)\)/i);
          const type = typeMatch ? typeMatch[1].trim() : "";

          // Extract group
          const groupLink = li.querySelector('a[href*="/group/?id="]');
          const group = groupLink ? groupLink.textContent.trim() : "";

          // Extract date (in green font)
          const dateFont = li.querySelector('font[color="#32E814"]');
          const date = dateFont ? dateFont.textContent.replace(/[()]/g, "").trim() : "";

          results.push({ id, name, type, group, date });
        }

        return results;
      } catch (err) {
        console.error("[csdbStore] parseQuickSearchResults error:", err);
        return [];
      }
    },

    /**
     * Parse group search results
     * HTML pattern: <li><a href="/group/?id=ID">NAME</a> (COUNTRY)</li>
     */
    parseGroupSearchResults(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");

        // Find the "N group match" section
        const groups = [];
        const groupLinks = doc.querySelectorAll('li a[href*="/group/?id="]');

        for (const link of groupLinks) {
          const idMatch = link.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const name = link.textContent.trim();
          const id = parseInt(idMatch[1]);

          // Country is in parentheses after the link
          const liText = (link.closest('li') || link.parentElement).textContent;
          const countryMatch = liText.match(/\(([^)]+)\)\s*$/);
          const country = countryMatch ? countryMatch[1].trim() : "";

          groups.push({ id, name, country });
        }

        return groups;
      } catch (err) {
        console.error("[csdbStore] parseGroupSearchResults error:", err);
        return [];
      }
    },

    /**
     * Extract group ID from a group page (self-referential link or canonical)
     * HTML pattern: <a href="/group/?id=ID">
     */
    parseGroupIdFromPage(html) {
      const match = html.match(/\/group\/\?id=(\d+)/);
      return match ? parseInt(match[1]) : null;
    },

    /**
     * Extract group name from a group page title
     * HTML pattern: <title>[CSDb] - Group: NAME</title>
     */
    parseGroupNameFromPage(html) {
      const match = html.match(/<title>\[CSDb\]\s*-\s*(?:Group:\s*)?([^<]+)<\/title>/i);
      return match ? this.decodeHtmlEntities(match[1].trim()) : "";
    },

    /**
     * Parse group page releases
     * HTML pattern: table rows with release link, year, type, party placement
     */
    parseGroupPage(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");

        // Find the "Releases :" section — releases are in a table after the bold header
        const releases = [];
        const rows = doc.querySelectorAll("table[cellspacing='1'] tr");

        for (const row of rows) {
          const cells = row.querySelectorAll("td");
          if (cells.length < 4) continue;

          const releaseLink = cells[0].querySelector('a[href*="/release/?id="]');
          if (!releaseLink) continue;

          const idMatch = releaseLink.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const id = parseInt(idMatch[1]);
          const name = releaseLink.textContent.trim();
          const date = cells[2] ? cells[2].textContent.trim() : "";
          const type = cells[3] ? cells[3].textContent.trim() : "";

          // Party placement in 5th cell → description
          let description = "";
          if (cells[4]) {
            description = cells[4].textContent.trim().replace(/[()]/g, "");
          }

          releases.push({ id, name, type, group: "", date, description });
        }

        return releases;
      } catch (err) {
        console.error("[csdbStore] parseGroupPage error:", err);
        return [];
      }
    },

    /**
     * Parse scener search results
     * HTML pattern: <li><a href="/scener/?id=ID">NAME</a> (COUNTRY)</li>
     */
    parseScenerSearchResults(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");
        const sceners = [];
        const scenerLinks = doc.querySelectorAll('li a[href*="/scener/?id="]');

        for (const link of scenerLinks) {
          const idMatch = link.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const name = link.textContent.trim();
          const id = parseInt(idMatch[1]);

          const liText = (link.closest('li') || link.parentElement).textContent;
          const countryMatch = liText.match(/\(([^)]+)\)\s*$/);
          const country = countryMatch ? countryMatch[1].trim() : "";

          sceners.push({ id, name, country });
        }

        return sceners;
      } catch (err) {
        console.error("[csdbStore] parseScenerSearchResults error:", err);
        return [];
      }
    },

    /**
     * Extract scener ID from a scener page
     */
    parseScenerIdFromPage(html) {
      const match = html.match(/\/scener\/\?id=(\d+)/);
      return match ? parseInt(match[1]) : null;
    },

    /**
     * Extract scener name from a scener page title
     * HTML pattern: <title>[CSDb] - Scener: NAME</title> or <title>[CSDb] - NAME</title>
     */
    parseScenerNameFromPage(html) {
      const match = html.match(/<title>\[CSDb\]\s*-\s*(?:Scener:\s*)?([^<]+)<\/title>/i);
      return match ? this.decodeHtmlEntities(match[1].trim()) : "";
    },

    /**
     * Parse scener page — "Releases released" and "Credits" sections.
     * Both use <b> markers followed by sibling <table> elements.
     *
     * Releases released: cells[0]=release link, cells[2]=year, cells[3]=type, cells[4]=party
     * Credits: cells[0]=release link + "by" + group, cells[2]=year+type, cells[3]=role
     */
    parseScenerPage(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");
        const releases = [];
        const seen = new Set();

        // Helper: find the next <table> sibling after a <b> marker.
        // The <b> is invalid inside <table>, so the DOM parser moves it
        // before the table — closest("table") would hit a layout table.
        const findTableAfter = (label) => {
          for (const b of doc.querySelectorAll("b")) {
            if (b.textContent.trim() === label) {
              let sibling = b.nextElementSibling;
              while (sibling) {
                if (sibling.tagName === "TABLE") return sibling;
                sibling = sibling.nextElementSibling;
              }
            }
          }
          return null;
        };

        // --- Releases released ---
        const releasesTable = findTableAfter("Releases released :");
        if (releasesTable) {
          for (const row of releasesTable.querySelectorAll("tr")) {
            const cells = row.querySelectorAll("td");
            if (cells.length < 4) continue;

            const releaseLink = cells[0].querySelector('a[href*="/release/?id="]');
            if (!releaseLink) continue;

            const idMatch = releaseLink.getAttribute("href").match(/id=(\d+)/);
            if (!idMatch) continue;

            const id = parseInt(idMatch[1]);
            const name = releaseLink.textContent.trim();
            const date = cells[2] ? cells[2].textContent.trim() : "";
            const type = cells[3] ? cells[3].textContent.trim() : "";

            let description = "";
            if (cells[4]) {
              description = cells[4].textContent.trim().replace(/[()]/g, "");
            }

            seen.add(id);
            releases.push({ id, name, type, group: "", date, description: description ? "[r] " + description : "[r]" });
          }
        }

        // --- Credits ---
        const creditsTable = findTableAfter("Credits :");
        if (creditsTable) {
          for (const row of creditsTable.querySelectorAll("tr")) {
            const cells = row.querySelectorAll("td");
            if (cells.length < 4) continue;

            const releaseLink = cells[0].querySelector('a[href*="/release/?id="]');
            if (!releaseLink) continue;

            const idMatch = releaseLink.getAttribute("href").match(/id=(\d+)/);
            if (!idMatch) continue;

            const id = parseInt(idMatch[1]);
            if (seen.has(id)) continue; // skip duplicates
            seen.add(id);

            const name = releaseLink.textContent.trim();

            // Group from cells[0]: "by <a href="/group/?id=...">Name</a>"
            const groupLinks = cells[0].querySelectorAll('a[href*="/group/?id="], a[href*="/scener/?id="]');
            const group = [...groupLinks].map(a => a.textContent.trim()).join(", ");

            // cells[2] has year + type combined, e.g. "2025 Music Collection"
            const yearTypeText = cells[2] ? cells[2].textContent.trim() : "";
            const yearMatch = yearTypeText.match(/^(\d{4})\s*(.*)/);
            const date = yearMatch ? yearMatch[1] : yearTypeText;
            const type = yearMatch ? yearMatch[2].trim() : "";

            // cells[3] has role, e.g. "(Music)"
            const role = cells[3] ? cells[3].textContent.trim().replace(/[()]/g, "") : "";

            const desc = [role, group].filter(Boolean).join(" - ");

            releases.push({ id, name, type, group, date, description: desc ? "[c] " + desc : "[c]" });
          }
        }

        return releases;
      } catch (err) {
        console.error("[csdbStore] parseScenerPage error:", err);
        return [];
      }
    },

    /**
     * Parse toplist results
     * HTML pattern: <tr><td>RANK</td><td><a href="/release/?id=ID">NAME</a> by GROUP</td><td>RATING</td><td></td><td>VOTES</td></tr>
     */
    parseToplistResults(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");
        const results = [];

        // Find the results table — rows with release links
        const rows = doc.querySelectorAll("table[cellspacing='2'] tr");

        for (const row of rows) {
          const cells = row.querySelectorAll("td");
          if (cells.length < 5) continue;

          const releaseLink = cells[1]?.querySelector('a[href*="/release/?id="]');
          if (!releaseLink) continue;

          const idMatch = releaseLink.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const id = parseInt(idMatch[1]);
          const name = releaseLink.textContent.trim();
          const rank = parseInt(cells[0].textContent.trim()) || null;

          // Extract group(s) from the "by" text — can be links to group or scener
          const groupLinks = cells[1].querySelectorAll('a[href*="/group/?id="]');
          const scenerLinks = cells[1].querySelectorAll('a[href*="/scener/?id="]');
          const allCreators = [...groupLinks, ...scenerLinks];
          const group = allCreators.map(a => a.textContent.trim()).join(", ");

          // Rating is in the 3rd cell
          const ratingText = cells[2]?.textContent.trim();
          const rating = ratingText ? parseFloat(ratingText) : null;

          // Votes in the 5th cell
          const votes = parseInt(cells[4]?.textContent.trim()) || null;

          results.push({ id, name, type: "", group, date: "", rank, rating, votes });
        }

        return results;
      } catch (err) {
        console.error("[csdbStore] parseToplistResults error:", err);
        return [];
      }
    },

    /**
     * Parse individual release page for details
     * @param {string} html - Release page HTML
     * @returns {object} - { id, name, type, rating, votes, group, date, party, downloads }
     */
    parseReleasePage(html) {
      try {
        const parser = new DOMParser();
        const doc = parser.parseFromString(html, "text/html");

        // ID from og:url meta tag
        const idMatch = html.match(
          /<meta property="og:url" content="[^"]*release\/\?id=(\d+)"/
        );
        const id = idMatch ? parseInt(idMatch[1]) : null;

        // Name and group from title: "[CSDb] - NAME by GROUP (YEAR)" — year may be absent
        const titleMatch = html.match(
          /<title>\[CSDb\]\s*-\s*(.+?)\s+by\s+(.+?)(?:\s*\(\d{4}\))?\s*<\/title>/
        );
        const name = titleMatch ? this.decodeHtmlEntities(titleMatch[1].trim()) : "";
        const group = titleMatch ? this.decodeHtmlEntities(titleMatch[2].trim()) : "";

        // Type from <a href="...rrelease_type...">TYPE</a>
        const typeEl = doc.querySelector('a[href*="rrelease_type"]');
        const type = typeEl ? typeEl.textContent.trim() : "";

        // Rating: first "N.N/10 (N votes)" only (page may have two rows)
        const ratingMatch = html.match(/([\d.]+)\/10\s*\((\d+)\s*votes?\)/);
        const rating = ratingMatch ? parseFloat(ratingMatch[1]) : null;
        const votes = ratingMatch ? parseInt(ratingMatch[2]) : null;

        // Release date
        const dateMatch = html.match(/Release Date\s*:[\s\S]*?(\d{1,2}\s+\w+\s+\d{4})/);
        const date = dateMatch ? dateMatch[1].trim() : "";

        // Released at (party)
        const eventLink = doc.querySelector('a[href*="/event/?id="]');
        const party = eventLink ? eventLink.textContent.trim() : "";

        // Download links — only http/https (ftp links can't be fetched via proxy)
        const seen = new Set();
        const downloads = [];
        for (const a of doc.querySelectorAll('a[href*="download.php?id="]')) {
          const href = a.getAttribute("href");
          const externalUrl = a.textContent.trim();
          if (!externalUrl.startsWith("http://") && !externalUrl.startsWith("https://")) continue;
          if (seen.has(href)) continue;
          seen.add(href);
          const parsed = new URL(href, "https://csdb.dk/release/");
          const url = this._csdbUrl(parsed.pathname + parsed.search);
          const filename = externalUrl.split("/").pop().split("?")[0] || externalUrl;
          console.log(`[csdbStore] download: href=${href} url=${url} filename=${filename} externalUrl=${externalUrl}`);
          downloads.push({ url, externalUrl, filename });
        }

        // Year from title: "[CSDb] - NAME by GROUP (YEAR)", fallback to date
        const year = (titleMatch ? titleMatch[0].match(/\((\d{4})\)/)?.[1] : "") || date.match(/(\d{4})/)?.[0] || "";

        const result = { id, name, year, type, rating, votes, group, date, party, downloads };
        console.log("[csdbStore] parseReleasePage:", result);
        return result;
      } catch (err) {
        console.error("[csdbStore] parseReleasePage error:", err);
        return { id: null, name: "", year: "", type: "", rating: null, votes: null, group: "", date: "", party: "", downloads: [] };
      }
    },

    /**
     * Decode HTML entities in a string
     */
    decodeHtmlEntities(str) {
      const textarea = document.createElement("textarea");
      textarea.innerHTML = str;
      return textarea.value;
    },
  });
});
