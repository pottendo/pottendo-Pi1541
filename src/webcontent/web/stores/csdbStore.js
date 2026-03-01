// Alpine.js CSDb Store
// Searches and browses the C64 Scene Database (csdb.dk)

document.addEventListener("alpine:init", () => {
  Alpine.store("csdb", {
    // State
    searching: false,
    error: null,
    results: { quick: [], group: [], toplist: [] },
    resultCounts: { quick: 0, group: 0, toplist: 0 },
    csdb_endpoint: localStorage.getItem("csdb_endpoint") || "http://localhost:8000/csdb",

    setCsdbEndpoint(url) {
      this.csdb_endpoint = url;
      localStorage.setItem("csdb_endpoint", url);
    },

    // Group browse state
    groupInfo: null, // { id, name, country, releases: [] }

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
     * Quick search — keyword search across releases
     * @param {string} query - Search query
     */
    async searchQuick(query) {
      if (!query || !query.trim()) return;
      this.searching = true;
      this.error = null;
      this.results.quick = [];
      this.resultCounts.quick = 0;

      try {
        const encoded = encodeURIComponent(query.trim());
        const base = this.csdb_endpoint;
        const url = `${base}/search/?seinsel=releases&search=${encoded}`;
        const resp = await fetch(url);
        const html = await resp.text();

        // Single-result: CSDb redirects directly to the release page (no og:title meta)
        if (!html.includes('<meta property="og:title" content="CSDb"')) {
          const details = this.parseReleasePage(html);
          this.results.quick = details.id ? [{ id: details.id, name: details.name, type: details.type, group: details.group, date: details.date }] : [];
        } else {
          this.results.quick = this.parseQuickSearchResults(html);
        }
        this.resultCounts.quick = this.results.quick.length;
        console.log(`[csdbStore] Quick search '${query}': ${this.resultCounts.quick} results`);
      } catch (err) {
        console.error("[csdbStore] searchQuick error:", err);
        this.error = "Search failed: " + err.message;
      } finally {
        this.searching = false;
      }
    },

    /**
     * Group search — find group by name and load its releases
     * @param {string} name - Group name to search for
     */
    async searchGroup(name) {
      if (!name || !name.trim()) return;
      this.searching = true;
      this.error = null;
      this.results.group = [];
      this.resultCounts.group = 0;
      this.groupInfo = null;

      try {
        // Step 1: Search for group to get its ID
        // The proxy follows redirects server-side, so a single-match search
        // returns the group page directly.
        const encoded = encodeURIComponent(name.trim());
        const base = this.csdb_endpoint;
        const searchResp = await fetch(`${base}/search/?seinsel=groups&search=${encoded}`);
        const searchHtml = await searchResp.text();

        let groupId, groupName, groupCountry, groupHtml;

        const groups = this.parseGroupSearchResults(searchHtml);
        if (groups.length > 0) {
          // Multiple results — pick the first and load its group page
          ({ id: groupId, name: groupName, country: groupCountry } = groups[0]);
          groupHtml = await fetch(`${this.csdb_endpoint}/group/?id=${groupId}`).then(r => r.text());
        } else {
          // Single result — proxy already followed the redirect to the group page
          groupId = this.parseGroupIdFromPage(searchHtml);
          if (!groupId) {
            this.error = "No group found";
            return;
          }
          groupName = this.parseGroupNameFromPage(searchHtml);
          groupCountry = "";
          groupHtml = searchHtml;
        }

        // Step 2: Parse releases from the group page
        const releases = this.parseGroupPage(groupHtml);

        this.groupInfo = { id: groupId, name: groupName, country: groupCountry };
        this.results.group = releases;
        this.resultCounts.group = releases.length;
        console.log(`[csdbStore] Group '${groupName}': ${releases.length} releases`);
      } catch (err) {
        console.error("[csdbStore] searchGroup error:", err);
        this.error = "Group search failed: " + err.message;
      } finally {
        this.searching = false;
      }
    },

    /**
     * Top list — fetch ranked chart for a release type
     * @param {number} subtype - Release type ID (e.g. 1 for C64 Demo)
     */
    async searchToplist(subtype) {
      this.searching = true;
      this.error = null;
      this.results.toplist = [];
      this.resultCounts.toplist = 0;

      try {
        const encoded = encodeURIComponent(`(${subtype})`);
        const base = this.csdb_endpoint;
        const url = `${base}/toplist.php?type=release&subtype=${encoded}`;
        const resp = await fetch(url);
        const html = await resp.text();
        this.results.toplist = this.parseToplistResults(html);
        this.resultCounts.toplist = this.results.toplist.length;
        console.log(`[csdbStore] Toplist subtype ${subtype}: ${this.resultCounts.toplist} results`);
      } catch (err) {
        console.error("[csdbStore] searchToplist error:", err);
        this.error = "Toplist failed: " + err.message;
      } finally {
        this.searching = false;
      }
    },

    /**
     * Fetch details for a single release
     * @param {number} id - Release ID
     * @returns {Promise<object>} - { rating, votes, group, date, party, placement }
     */
    async fetchReleaseDetails(id) {
      try {
        const resp = await fetch(`${this.csdb_endpoint}/release/?id=${id}`);
        const html = await resp.text();
        return this.parseReleasePage(html);
      } catch (err) {
        console.error(`[csdbStore] fetchReleaseDetails error for ${id}:`, err);
        return null;
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
        const groupLinks = doc.querySelectorAll('ol > li > a[href*="/group/?id="]');

        for (const link of groupLinks) {
          const idMatch = link.getAttribute("href").match(/id=(\d+)/);
          if (!idMatch) continue;

          const name = link.textContent.trim();
          const id = parseInt(idMatch[1]);

          // Country is in parentheses after the link
          const liText = link.parentElement.textContent;
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

          // Party placement in 5th cell
          let party = "";
          if (cells[4]) {
            party = cells[4].textContent.trim().replace(/[()]/g, "");
          }

          releases.push({ id, name, type, group: "", date, party });
        }

        return releases;
      } catch (err) {
        console.error("[csdbStore] parseGroupPage error:", err);
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

        // Name and group from title: "[CSDb] - NAME by GROUP (YEAR)"
        const titleMatch = html.match(
          /<title>\[CSDb\]\s*-\s*(.+?)\s+by\s+(.+?)\s*\(\d{4}\)/
        );
        const name = titleMatch ? this.decodeHtmlEntities(titleMatch[1].trim()) : "";
        const group = titleMatch ? this.decodeHtmlEntities(titleMatch[2].trim()) : "";

        // Type from <b>Type :</b><br><a ...>TYPE</a>
        const typeMatch = html.match(/<b>Type\s*:<\/b><br>\s*<a[^>]*>([^<]+)<\/a>/i);
        const type = typeMatch ? typeMatch[1].trim() : "";

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
          const url = new URL(href, "https://csdb.dk/release/").href;
          const filename = externalUrl.split("/").pop().split("?")[0] || externalUrl;
          downloads.push({ url, externalUrl, filename });
        }

        // Year from title: "[CSDb] - NAME by GROUP (YEAR)"
        const year = titleMatch ? titleMatch[0].match(/\((\d{4})\)/)?.[1] : "";

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
