// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 skyie
//
// This file is part of pi1541ui.
// pi1541ui is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// See the LICENSE file for details.

(function injectPiFont(){
  function apply(){
    if (window.Alpine && Alpine.store && Alpine.store('proxy')){
      var endpoint = Alpine.store('proxy').effectivePiEndpoint() || 'http://localhost:8000/pi-proxy';
      var fontUrl = endpoint.replace(/\/+$/, '') + '/C64_Pro_Mono.ttf';
      // set CSS variable for fallback
      document.documentElement.style.setProperty('--pi-font-url', fontUrl);
      // inject @font-face to force load from runtime URL
      var styleId = 'pi-runtime-font';
      if (!document.getElementById(styleId)){
        var s = document.createElement('style');
        s.id = styleId;
        s.type = 'text/css';
        s.textContent = "@font-face { font-family: 'C64 Pro Mono'; src: url('" + fontUrl + "') format('truetype'); font-weight: normal; font-style: normal; }";
        document.head.appendChild(s);
      }
    } else {
      setTimeout(apply, 50);
    }
  }
  apply();
})();