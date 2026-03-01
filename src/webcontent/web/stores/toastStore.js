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
