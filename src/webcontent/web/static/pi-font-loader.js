(function injectPiFont(){
  function apply(){
    if (window.Alpine && Alpine.store && Alpine.store('proxy')){
      var endpoint = Alpine.store('proxy').pi_endpoint || 'http://localhost:8000/proxy';
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