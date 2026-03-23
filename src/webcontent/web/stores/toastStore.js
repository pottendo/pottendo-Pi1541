// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 skyie
//
// This file is part of pi1541ui.
// pi1541ui is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// See the LICENSE file for details.

// Alpine.js Toast Store
// Wraps iziToast for app-wide toast notifications

document.addEventListener("alpine:init", () => {
  Alpine.store("toast", {
    _defaults: {
      position: "topRight",
      timeout: 3000,
    },

    info(message, title = "Info") {
      iziToast.info({
        title,
        message,
        position: this._defaults.position,
        timeout: this._defaults.timeout,
      });
    },

    success(message, title = "Success") {
      iziToast.success({
        title,
        message,
        position: this._defaults.position,
        timeout: this._defaults.timeout,
      });
    },

    error(message, title = "Error") {
      iziToast.error({
        title,
        message,
        position: this._defaults.position,
        timeout: this._defaults.timeout,
      });
    },
  });
});
