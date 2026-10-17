/* MiniOS: page images stay blocked. A selected image still opens. */
function isLocal(url) {
  return /^(blob:|data:|moz-extension:|about:)/i.test(url || "");
}

function isImagePath(url) {
  try {
    const u = new URL(url);
    return /\.(jpe?g|png|gif|webp|bmp|svg|avif|ico)(\?|#|$)/i.test(u.pathname);
  } catch (e) {
    return false;
  }
}

function contentTypeIsImage(headers) {
  if (!headers) return false;
  for (let i = 0; i < headers.length; i++) {
    if (String(headers[i].name).toLowerCase() === "content-type") {
      return /^image\//i.test(String(headers[i].value || ""));
    }
  }
  return false;
}

const busy = {};

async function showInTab(tabId, url) {
  if (!url || isLocal(url) || busy[tabId]) return;
  busy[tabId] = true;
  try {
    const r = await fetch(url, { credentials: "omit", redirect: "follow" });
    if (!r.ok) return;
    const b = await r.blob();
    const t = String(b.type || "");
    if (t.indexOf("text/html") !== -1) return;
    const obj = URL.createObjectURL(b);
    await browser.tabs.update(tabId, { url: obj });
  } catch (e) {
    /* leave the tab as-is */
  } finally {
    delete busy[tabId];
  }
}

browser.tabs.onUpdated.addListener((tabId, change) => {
  if (!change.url || isLocal(change.url)) return;
  if (isImagePath(change.url)) showInTab(tabId, change.url);
});

browser.webRequest.onHeadersReceived.addListener(
  (d) => {
    if (d.type !== "main_frame" || d.tabId < 0) return;
    if (isLocal(d.url)) return;
    if (!contentTypeIsImage(d.responseHeaders)) return;
    showInTab(d.tabId, d.url);
  },
  { urls: ["<all_urls>"], types: ["main_frame"] },
  ["responseHeaders"]
);

browser.contextMenus.create({
  id: "minios-view-image",
  title: "View this image",
  contexts: ["image"]
});

browser.contextMenus.onClicked.addListener((info) => {
  if (info.menuItemId !== "minios-view-image" || !info.srcUrl) return;
  browser.tabs.create({ url: "about:blank" }).then((tab) => {
    showInTab(tab.id, info.srcUrl);
  });
});
