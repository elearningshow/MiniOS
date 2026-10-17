// Nightly low-memory preferences for MiniOS (BROWSER-PERF-AP-01)
// Apply with: nightly -P default /path/to/nightly-lowmem.js
// or import via about:config / user.js method

user_pref("browser.sessionhistory.max_entries", 5);
user_pref("browser.sessionhistory.max_total_viewers", 0);
user_pref("javascript.options.mem.high_water_mark", 64);
user_pref("image.mem.max_decoded_image_kb", 16384);
user_pref("browser.cache.memory.capacity", 16384);
user_pref("browser.cache.memory.enable", true);
user_pref("dom.ipc.processCount", 1);
