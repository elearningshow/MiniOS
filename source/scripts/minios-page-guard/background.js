/* Keep only a few pictures for the page that is open.
 * Drop the previous page cache when a new address starts. */
const MAX_IMAGES = 4;
const images = {};

function reset(tabId) {
  images[tabId] = 0;
}

browser.webNavigation.onCommitted.addListener((details) => {
  if (details.frameId !== 0 || details.tabId < 0)
    return;
  reset(details.tabId);
  browser.browsingData.removeCache({}).catch(() => {});
});

browser.tabs.onRemoved.addListener((tabId) => {
  delete images[tabId];
});

const HEAVY = [
  "news.google.com", "twitter.com", "x.com", "instagram.com",
  "facebook.com", "fb.com", "linkedin.com", "tiktok.com",
  "youtube.com", "m.youtube.com"
];

function hostHeavy(url) {
  let h = "";
  try { h = new URL(url).hostname.toLowerCase(); } catch (e) { return false; }
  if (h === "reddit.com" || h.endsWith(".reddit.com"))
    return h.indexOf("old.reddit.com") === -1;
  return HEAVY.some((x) => h === x || h.endsWith("." + x));
}

function metaContent(html, key) {
  const safe = key.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const tag = new RegExp("<meta\\b[^>]*(?:name|property)=[\"']" + safe + "[\"'][^>]*>", "i");
  const found = html.match(tag);
  let raw = "";
  if (found) {
    const c = found[0].match(/content=[\"']([^\"']*)[\"']/i);
    raw = c ? c[1] : "";
  } else {
    const alt = new RegExp("content=[\"']([^\"']*)[\"'][^>]*(?:name|property)=[\"']" + safe, "i");
    const m = html.match(alt);
    raw = m ? m[1] : "";
  }
  return raw.replace(/&amp;/g, "&").replace(/&quot;/g, "\"")
    .replace(/&#39;|&apos;/g, "'").replace(/&lt;/g, "<").replace(/&gt;/g, ">");
}

function useless(s) {
  if (!s || s.replace(/\s+/g, " ").trim().length < 40) return true;
  return /comprehensive up-to-date news coverage|aggregated from sources all over the world by google news/i.test(s);
}

function newsId(url) {
  const m = String(url).match(/\/(?:read|articles|rss\/articles)\/([^/?#]+)/);
  return m ? m[1] : "";
}

function b64url(s) {
  s = String(s).replace(/-/g, "+").replace(/_/g, "/");
  while (s.length % 4) s += "=";
  try {
    const bin = atob(s);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return new TextDecoder("utf-8", { fatal: false }).decode(bytes);
  } catch (e) {
    return "";
  }
}

function urlsIn(text) {
  const found = String(text).match(/https?:\/\/[^\s\x00-\x1f"'<>\\]+/g) || [];
  const out = [];
  for (let i = 0; i < found.length; i++) {
    const u = found[i].replace(/[),.;]+$/, "");
    if (/google\.|gstatic\.|schema\.org|googleapis|googleusercontent/.test(u))
      continue;
    if (out.indexOf(u) === -1) out.push(u);
  }
  return out;
}

function paragraphs(html) {
  const parts = [];
  const re = /<p\b[^>]*>([\s\S]*?)<\/p>/gi;
  let m;
  while ((m = re.exec(html)) && parts.length < 3) {
    const t = m[1].replace(/<[^>]+>/g, " ").replace(/\s+/g, " ").trim();
    if (t.length < 80) continue;
    if (/cookie|subscribe|sign up|enable javascript/i.test(t) && t.length < 180)
      continue;
    parts.push(t);
  }
  return parts;
}

async function readLimited(url) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 8000);
  try {
    const r = await fetch(url, {
      credentials: "omit",
      redirect: "follow",
      signal: ctrl.signal
    });
    const len = Number(r.headers.get("content-length") || 0);
    const reader = r.body.getReader();
    const chunks = [];
    let got = 0;
    while (got < 64000) {
      const step = await reader.read();
      if (step.done) break;
      chunks.push(step.value);
      got += step.value.length;
    }
    try { reader.cancel(); } catch (e) {}
    let n = 0;
    chunks.forEach((c) => { n += c.length; });
    const buf = new Uint8Array(n);
    let o = 0;
    chunks.forEach((c) => { buf.set(c, o); o += c.length; });
    return {
      len: len,
      html: new TextDecoder("utf-8", { fatal: false }).decode(buf)
    };
  } finally {
    clearTimeout(timer);
  }
}

async function storyFromNews(url) {
  const id = newsId(url);
  if (!id) return null;
  let source = urlsIn(b64url(id))[0] || "";
  let rssTitle = "";
  if (!source) {
    try {
      const rss = await readLimited("https://news.google.com/rss/articles/" + id);
      rssTitle = ((rss.html.match(/<title[^>]*>([^<]+)/i) || [])[1] || "").trim();
      const link = ((rss.html.match(/<link>([^<]+)<\/link>/i) || [])[1] || "").trim();
      source = (!/news\.google/.test(link) && link) || urlsIn(rss.html)[0] || "";
    } catch (e) {
      source = "";
    }
  }
  if (!source) return null;
  let host = source;
  try { host = new URL(source).hostname.replace(/^www\./, ""); } catch (e) {}
  try {
    const page = await readLimited(source);
    const title = ((page.html.match(/<title[^>]*>([^<]+)/i) || [])[1] || rssTitle).trim();
    const desc = metaContent(page.html, "og:description") ||
      metaContent(page.html, "description");
    const paras = paragraphs(page.html);
    let summary = paras.join("\n\n");
    if (!summary && !useless(desc)) summary = desc;
    return { title: title, source: host, summary: summary, url: source };
  } catch (e) {
    return { title: rssTitle, source: host, summary: "", url: source };
  }
}

async function peek(url) {
  const heavy = hostHeavy(url);
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 8000);
  try {
    const r = await fetch(url, {
      credentials: "omit",
      redirect: "follow",
      signal: ctrl.signal
    });
    const len = Number(r.headers.get("content-length") || 0);
    const reader = r.body.getReader();
    const chunks = [];
    let got = 0;
    while (got < 48000) {
      const step = await reader.read();
      if (step.done) break;
      chunks.push(step.value);
      got += step.value.length;
    }
    try { reader.cancel(); } catch (e) {}
    let n = 0;
    chunks.forEach((c) => { n += c.length; });
    const buf = new Uint8Array(n);
    let o = 0;
    chunks.forEach((c) => { buf.set(c, o); o += c.length; });
    const html = new TextDecoder("utf-8", { fatal: false }).decode(buf);
    const titleMatch = html.match(/<title[^>]*>([^<]+)/i);
    const title = titleMatch ? titleMatch[1].trim() : "";
    let description = metaContent(html, "og:description") ||
      metaContent(html, "description");
    if (useless(description)) description = "";
    const scripts = (html.match(/<script\b/gi) || []).length;
    const text = html.replace(/<script[\s\S]*?<\/script>/gi, " ")
      .replace(/<style[\s\S]*?<\/style>/gi, " ")
      .replace(/<[^>]+>/g, " ")
      .replace(/\s+/g, " ")
      .trim();
    const complex = heavy || len > 500000 || scripts >= 8 ||
      (scripts >= 2 && text.length < 240);
    let story = null;
    if (/news\.google\.com/i.test(url)) {
      try { story = await storyFromNews(url); } catch (err) { story = null; }
    }
    let summary = (story && story.summary) || description ||
      (useless(text) ? "" : text.slice(0, 700));
    summary = summary.replace(/\s+/g, " ").trim();
    if ((summary.match(/[{;}]/g) || []).length > 2 ||
        /webkit|font-family|body,html|position:absolute/.test(summary) ||
        useless(summary))
      summary = "";
    let reason = "This page can be opened.";
    if (heavy) reason = "This site draws the page with scripts. The story below is from the source page.";
    else if (len > 500000) reason = "This page is too large to open here.";
    else if (complex) reason = "This page needs scripts to show its content.";
    return {
      ok: true,
      title: (story && story.title) || title,
      description: summary,
      text: summary,
      source: (story && story.source) || "",
      summary: summary,
      complex: complex,
      reason: reason
    };
  } catch (e) {
    return {
      ok: false, title: "", description: "", text: "",
      complex: heavy, reason: heavy
        ? "This site draws the page with scripts."
        : "This page could not be read, so it was not opened."
    };
  } finally {
    clearTimeout(timer);
  }
}

browser.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (!msg) return;
  if (msg.type === "enable-js") {
    fetch("http://127.0.0.1:8791/on?u=" + encodeURIComponent(msg.url || ""))
      .then((r) => sendResponse({ ok: r.ok }))
      .catch(() => sendResponse({ ok: false }));
    return true;
  }
  if (msg.type !== "peek" || !msg.url) return;
  peek(msg.url).then(sendResponse);
  return true;
});

browser.webRequest.onBeforeRequest.addListener(
  (details) => {
    if (details.type !== "image" || details.tabId < 0)
      return {};
    const n = images[details.tabId] || 0;
    if (n >= MAX_IMAGES)
      return { cancel: true };
    images[details.tabId] = n + 1;
    return {};
  },
  { urls: ["<all_urls>"], types: ["image"] },
  ["blocking"]
);
