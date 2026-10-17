// MiniOS BROWSER-AP-03 — Firefox ESR homepage
user_pref("browser.startup.homepage", "https://lite.duckduckgo.com/lite/");
user_pref("browser.startup.page", 1);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("startup.homepage_welcome_url", "");
user_pref("startup.homepage_welcome_url.additional", "");
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("browser.tabs.warnOnClose", false);
user_pref("gfx.webrender.all", false);
user_pref("gfx.webrender.software", true);
user_pref("layers.acceleration.disabled", true);
user_pref("media.hardware-video-decoding.enabled", false);
user_pref("media.ffmpeg.vaapi.enabled", false);
user_pref("browser.startup.firstrunSkipsHomepage", false);
user_pref("media.autoplay.default", 1);

// BROWSER-PERF-AP-01 — low-memory prefs (Gateway ~441 MB + swap)
user_pref("browser.sessionhistory.max_entries", 1);
user_pref("browser.cache.disk.enable", false);
user_pref("browser.sessionhistory.max_total_viewers", 0);
user_pref("javascript.options.mem.high_water_mark", 64);
user_pref("image.mem.max_decoded_image_kb", 8192);
user_pref("image.mem.surfacecache.max_size_kb", 16384);
user_pref("image.mem.surfacecache.min_expiration_ms", 30000);
user_pref("ui.use_activity_cursor", false);
user_pref("image.downscale-during-decode.enabled", true);
user_pref("image.high_quality_downscaling.enabled", false);
user_pref("image.decode-immediately.enabled", false);
user_pref("image.mem.discardable", true);
user_pref("image.animation_mode", "once");
user_pref("browser.cache.memory.capacity", 8192);
user_pref("browser.cache.memory.enable", true);
user_pref("dom.ipc.processCount", 1);
user_pref("accessibility.force_disabled", 1);
user_pref("general.smoothScroll", false);
user_pref("toolkit.cosmeticAnimations.enabled", false);
user_pref("ui.prefersReducedMotion", 1);

// Pictures are on. Firefox decodes them at the size on the page,
// not the full camera size, and drops them from RAM when unused.
// JavaScript stays off. Turning it on locks this 512 MB machine.
user_pref("permissions.default.image", 1);
user_pref("javascript.enabled", false);
user_pref("xpinstall.signatures.required", false);
user_pref("extensions.langpacks.signatures.required", false);
user_pref("extensions.autoDisableScopes", 0);
user_pref("extensions.enabledScopes", 15);
user_pref("extensions.startupScanScopes", 15);
user_pref("media.autoplay.blocking_policy", 2);
user_pref("browser.search.defaultenginename", "DuckDuckGo");
user_pref("browser.search.order.1", "DuckDuckGo");
user_pref("browser.urlbar.suggest.searches", false);
user_pref("browser.search.suggest.enabled", false);
