// HMK PWA Service Worker（高度獨立第一版：首頁 + 遙控頁都可離線開啟）

let CACHE = "hmk-wireless-default";

async function getCacheVersion(){
  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), {cache: 'no-store'});
    const m = await r.json();
    const name = String(m.name || '').trim();
    const match = name.match(/(\d+)/);
    if(match){
      return "hmk-wireless-v" + match[1];
    }
  }catch(e){
    console.warn('SW: Failed to read manifest version', e);
  }
  return CACHE;
}

const ASSETS = [
  "./",
  "./index.html",
  "./remote.html",
  "./manifest.json",
  "./sw.js",
  "./version.js",
  "./CHANGELOG.json",
  "./icon-192.png",
  "./icon-512.png",
  "./status.html",
  "./schedule.html",
  "./apple-touch-icon-180.png"
];

const SHELL_PATHS = new Set([
  "/",
  "/index.html",
  "/remote.html",
  "/status.html",
  "/schedule.html"
]);

async function getShellFallback(cache, path, request){
  let match = await cache.match(request, { ignoreSearch: true });
  if (match) return match;

  if (path.endsWith("/remote.html") || path === "/remote.html") {
    match = await cache.match("./remote.html", { ignoreSearch: true });
    if (match) return match;
  }

  if (path.endsWith("/status.html") || path === "/status.html") {
    match = await cache.match("./status.html", { ignoreSearch: true });
    if (match) return match;
  }

  if (path.endsWith("/schedule.html") || path === "/schedule.html") {
    match = await cache.match("./schedule.html", { ignoreSearch: true });
    if (match) return match;
  }

  return cache.match("./index.html", { ignoreSearch: true });
}

// 安裝：先快取靜態殼頁
self.addEventListener("install", (e) => {
  e.waitUntil(
    getCacheVersion().then(ver => {
      CACHE = ver;
      return caches.open(CACHE).then(c => c.addAll(ASSETS));
    })
  );
  self.skipWaiting();
});

// 啟動：清掉舊 cache，立即接管
self.addEventListener("activate", (e) => {
  e.waitUntil(
    getCacheVersion().then(ver => {
      CACHE = ver;
      return caches.keys().then(keys =>
        Promise.all(
          keys.map(k => (k !== CACHE ? caches.delete(k) : Promise.resolve()))
        )
      ).then(() => self.clients.claim());
    })
  );
});

// fetch：
// 1) 只攔截 PWA 自己的 same-origin 檔案
// 2) 192.168.4.1 / 192.168.xxx.xxx 這些設備請求不攔截
// 3) shell 頁面採 cache-first；背景再更新快取
self.addEventListener("fetch", (e) => {
  const url = new URL(e.request.url);
  const path = url.pathname || "/";

  // 只處理自己來源的 PWA 殼頁/靜態檔
  if (url.origin !== self.location.origin) return;

  if (e.request.method !== "GET") return;

  // 動態 API 請求（cache:'no-store'）不攔截，讓瀏覽器直接打網路
  // 避免 AP 模式下回傳舊快取而誤判連線狀態
  if (e.request.cache === "no-store") return;

  const isShellRequest = e.request.mode === "navigate" || SHELL_PATHS.has(path);

  if (isShellRequest) {
    const cachePromise = caches.open(CACHE);
    const fallbackPromise = cachePromise.then(cache => getShellFallback(cache, path, e.request));
    const networkUpdate = cachePromise
      .then(cache => fetch(e.request)
        .then(async (resp) => {
          if (!resp || !resp.ok) throw new Error("Bad response status");
          await cache.put(e.request, resp.clone());
          return resp;
        })
      )
      .catch(() => null);

    e.respondWith(
      (async () => {
        const fallback = await fallbackPromise;
        if (fallback) return fallback;

        const networkResp = await networkUpdate;
        if (networkResp) return networkResp;

        const cache = await cachePromise;
        return getShellFallback(cache, path, e.request);
      })()
    );
    e.waitUntil(networkUpdate.then(() => undefined));
    return;
  }

  e.respondWith(
    fetch(e.request)
      .then(async (resp) => {
        if (!resp || !resp.ok) throw new Error("Bad response status");

        const copy = resp.clone();
        const cache = await caches.open(CACHE);
        cache.put(e.request, copy);
        return resp;
      })
      .catch(async () => {
        const cache = await caches.open(CACHE);

        // 先找原請求
        let match = await cache.match(e.request, { ignoreSearch: true });
        if (match) return match;

        // 遙控頁失敗時，優先回 remote.html
        if (path.endsWith("/remote.html") || path === "/remote.html") {
          match = await cache.match("./remote.html", { ignoreSearch: true });
          if (match) return match;
        }

        // 首頁 / 根路徑 / 其它 shell 導航，回首頁殼
        match = await cache.match("./index.html", { ignoreSearch: true });
        if (match) return match;

        // 最後保底
        return Response.error();
      })
  );
});