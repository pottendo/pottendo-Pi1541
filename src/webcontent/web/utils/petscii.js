// Shared PETSCII utility — exposes window.PetsciiUtils before Alpine stores load.
// Both proxyStore and discStore (and components) use these functions.

window.PetsciiUtils = {
  /**
   * Converts PETSCII-encoded HTML (U+0EExx codepoints) to readable ASCII.
   * HTML <br> tags become newlines; HTML entities are decoded via a textarea.
   * @param {string} petsciiHtml
   * @returns {string}
   */
  petsciiToAscii(petsciiHtml) {
    const text = petsciiHtml
      .replace(/<br\s*\/?>/gi, "\n")
      .replace(/<[^>]*>/g, "");

    const textarea = document.createElement("textarea");
    textarea.innerHTML = text;
    const decoded = textarea.value;

    let result = "";
    for (const char of decoded) {
      const code = char.codePointAt(0);

      if (code >= 0x0ee00 && code <= 0x0eeff) {
        let petscii = code - 0x0ee00;
        if (petscii >= 0x80) petscii -= 0x80;

        if (petscii === 0x20 || petscii === 0x60) {
          result += " ";
        } else if (petscii >= 0x30 && petscii <= 0x39) {
          result += String.fromCharCode(petscii);
        } else if (petscii >= 0x41 && petscii <= 0x5a) {
          result += String.fromCharCode(petscii);
        } else if (petscii >= 0x01 && petscii <= 0x1a) {
          result += String.fromCharCode(0x40 + petscii);
        } else if (".,:;!?()-+*/=<>\"'@#$%&".includes(String.fromCharCode(petscii))) {
          result += String.fromCharCode(petscii);
        } else {
          result += "?";
        }
      } else if (char === "\n") {
        result += "\n";
      } else if (code >= 0x20 && code <= 0x7e) {
        result += char;
      }
    }

    return result;
  },

  /**
   * Splits PETSCII-encoded HTML by <br> lines and extracts searchable ASCII
   * text from each line. Returns an array of { ascii, html } objects.
   * @param {string} html
   * @returns {{ ascii: string, html: string }[]}
   */
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
};
