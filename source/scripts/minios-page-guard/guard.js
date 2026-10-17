/* Loading note, a summary when a page stays blank, and a text view
 * when the page is too large for this computer. */
(function () {
  const BIG = 280000;

  function note() {
    let el = document.getElementById("minios-page-note");
    if (el) return el;
    el = document.createElement("div");
    el.id = "minios-page-note";
    el.setAttribute("style",
      "position:fixed;left:16px;top:16px;z-index:2147483647;" +
      "max-width:560px;background:#10243a;color:#ffffff;" +
      "padding:14px 16px;border:2px solid #7eb6ff;" +
      "font:16px sans-serif;line-height:1.35;");
    const close = document.createElement("button");
    close.textContent = "Close";
    close.setAttribute("style",
      "float:right;margin:0 0 8px 8px;font:14px sans-serif;");
    close.addEventListener("click", function () { el.remove(); });
    const body = document.createElement("div");
    body.id = "minios-page-note-body";
    const js = document.createElement("div");
    js.setAttribute("style", "margin-top:10px;");
    js.appendChild(document.createTextNode(
      "This browser has been optimized for speed by turning off Javascript. "));
    const link = document.createElement("a");
    link.href = "#";
    link.textContent = "Click here";
    link.setAttribute("style", "color:#7eb6ff;");
    link.addEventListener("click", function (ev) {
      ev.preventDefault();
      body.textContent = "Turning JavaScript on.";
      browser.runtime.sendMessage({ type: "enable-js", url: location.href });
    });
    js.appendChild(link);
    js.appendChild(document.createTextNode(" to renable JavaScript"));
    el.appendChild(close);
    el.appendChild(body);
    el.appendChild(js);
    (document.documentElement || document).appendChild(el);
    return el;
  }

  function setText(el, text) {
    const body = el.querySelector("#minios-page-note-body");
    if (body) body.textContent = text;
    else el.textContent = text;
  }

  note();
  setText(note(), "Loading.");

  function metaText() {
    const m = document.querySelector('meta[name="description"]');
    return m ? (m.getAttribute("content") || "") : "";
  }

  function pageText() {
    const body = document.body;
    if (!body) return "";
    return (body.innerText || "").replace(/\s+/g, " ").trim();
  }

  function uselessText(s) {
    if (!s || s.replace(/\s+/g, " ").trim().length < 40) return true;
    return /comprehensive up-to-date news coverage|aggregated from sources all over the world by google news/i.test(s);
  }

  function showStory(info) {
    const el = note();
    const title = (info && info.title) || document.title || "Untitled page";
    const source = (info && info.source) ? ("Source: " + info.source + "\n\n") : "";
    const body = (info && (info.summary || info.description || info.text)) || "";
    const readable = uselessText(body) ? "" : body;
    setText(el, title + "\n\n" + source +
      (readable || "The source page did not include a readable opening.") +
      "\n\nThe full page was not loaded. Scripts stay off on this computer.");
  }

  function failed() {
    const el = note();
    setText(el, "Loading finished. Reading the story from its source.");
    browser.runtime.sendMessage({ type: "peek", url: location.href }).then(function (info) {
      showStory(info || {});
    }).catch(function () {
      showStory({});
    });
  }

  function showTextOnly() {
    const text = pageText();
    const box = document.createElement("pre");
    box.setAttribute("style",
      "white-space:pre-wrap;font:16px sans-serif;color:#111;background:#fff;" +
      "margin:0;padding:72px 16px 16px;");
    box.textContent = text;
    document.documentElement.replaceChild(box, document.body);
    const el = note();
    setText(el, "Text only. Pictures on this page were not loaded.");
  }

  function finish() {
    const text = pageText();
    const size = (document.documentElement.outerHTML || "").length;
    if (document.querySelector("input, textarea, img")) {
      const el = document.getElementById("minios-page-note");
      if (el) el.remove();
      return;
    }
    if (text.length < 60 || uselessText(text)) {
      failed();
      return;
    }
    if (size > BIG) {
      const el = note();
      setText(el, "This page is large (" + Math.round(size / 1000) +
        " KB). You can read it without the rest of its pictures.");
      const btn = document.createElement("button");
      btn.textContent = "View text only";
      btn.setAttribute("style", "display:block;margin-top:10px;font:16px sans-serif;");
      btn.addEventListener("click", showTextOnly);
      el.appendChild(btn);
      return;
    }
    const el = document.getElementById("minios-page-note");
    if (el) el.remove();
  }

  function heavyHost(url) {
    let h = "";
    try { h = new URL(url).hostname.toLowerCase(); } catch (e) { return false; }
    if (h === "reddit.com" || h.endsWith(".reddit.com"))
      return h.indexOf("old.reddit.com") === -1;
    return /(^|\.)((news\.google|twitter|x|instagram|facebook|fb|linkedin|tiktok|youtube|m\.youtube)\.com)$/.test(h);
  }

  window.addEventListener("load", function () {
    if (onSearch()) {
      const links = document.querySelectorAll("a[href]");
      for (let i = 0; i < links.length; i++) {
        const url = realUrl(links[i].href);
        if (!url || !heavyHost(url)) continue;
        if (links[i].getAttribute("data-minios")) continue;
        links[i].setAttribute("data-minios", "heavy");
        const mark = document.createElement("div");
        mark.textContent = "Needs scripts. Opens a short description instead of the page.";
        mark.setAttribute("style", "color:#10243a;font:14px sans-serif;margin:2px 0 8px;");
        if (links[i].parentNode)
          links[i].parentNode.insertBefore(mark, links[i].nextSibling);
      }
    }
    window.setTimeout(finish, 600);
  });

  function onSearch() {
    return location.hostname.indexOf("duckduckgo.com") !== -1;
  }

  function realUrl(href) {
    try {
      const u = new URL(href, location.href);
      if (u.hostname.indexOf("duckduckgo.com") !== -1) {
        const uddg = u.searchParams.get("uddg");
        return uddg || "";
      }
      if (u.protocol !== "http:" && u.protocol !== "https:") return "";
      return u.href;
    } catch (e) {
      return "";
    }
  }

  function snippetFor(a) {
    let n = a;
    for (let i = 0; i < 5 && n; i++) {
      const snip = n.querySelector &&
        n.querySelector(".result__snippet, .result-snippet, td");
      if (snip && snip !== a && snip.textContent.trim().length > 40)
        return snip.textContent.replace(/\s+/g, " ").trim();
      n = n.parentElement;
    }
    return (a.textContent || "").replace(/\s+/g, " ").trim();
  }

  function showResult(url, info, snippet) {
    const el = note();
    const title = (info && info.title) || snippet || url;
    const source = (info && info.source) ? ("Source: " + info.source + "\n\n") : "";
    let desc = (info && (info.summary || info.description || info.text)) || "";
    if (uselessText(desc)) desc = "";
    if (!desc) desc = snippet || "";
    if (uselessText(desc)) desc = "No opening sentences were found on the source page.";
    const reason = (info && info.reason) || "This page was not opened.";
    setText(el, title + "\n\n" + source + desc + "\n\n" + reason);
  }

  document.addEventListener("click", function (ev) {
    if (!onSearch()) return;
    const a = ev.target && ev.target.closest && ev.target.closest("a");
    if (!a || !a.href) return;
    const url = realUrl(a.href);
    if (!url) return;
    ev.preventDefault();
    ev.stopPropagation();
    const snippet = snippetFor(a);
    const el = note();
    setText(el, "Checking this result.\n" + snippet);
    browser.runtime.sendMessage({ type: "peek", url: url }).then(function (info) {
      if (info && !info.complex) {
        location.href = url;
        return;
      }
      showResult(url, info || {}, snippet);
    }).catch(function () {
      showResult(url, {
        reason: "This page could not be read, so it was not opened."
      }, snippet);
    });
  }, true);
})();
