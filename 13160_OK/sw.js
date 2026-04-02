// HMK PWA Service Worker（高度獨立第一版：首頁 + 遙控頁都可離線開啟）
$113160 ← bump-version.ps1 自動更新此行，觸發瀏覽器重新安裝 SW
$113160';

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
  "./user.html",
  "./status.html",
  "./powerlog.html",
  "./manifest.json",
  "./sw.js",
  "./version.js",
  "./CHANGELOG.json",
  "./icon-192.png",
  "./icon-512.png",
  "./schedule.html",
  "./setup.html",
  "./apple-touch-icon-180.png"
];

// 安裝：先快取靜態殼頁
self.addEventListener("install", (e) => {
  e.waitUntil(
    getCacheVersion().then(async ver => {
      CACHE = ver;
      const cache = await caches.open(CACHE);

      await Promise.all(
        ASSETS.map(async (asset) => {
          try{
            const resp = await fetch(asset, { cache: "no-store" });
            if (resp && resp.ok) {
              await cache.put(asset, resp.clone());
            }
          }catch(err){
            console.warn("SW: cache warmup skipped", asset, err);
          }
        })
      );
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

const SHELL_FALLBACKS = {
  "/": "./index.html",
  "/index.html": "./index.html",
  "/remote.html": "./remote.html",
  "/schedule.html": "./schedule.html",
  "/status.html": "./status.html",
  "/powerlog.html": "./powerlog.html",
  "/user.html": "./user.html",
  "/setup.html": "./setup.html"
};

function getShellCacheKey(pathname){
  const path = String(pathname || "/").toLowerCase();
  return SHELL_FALLBACKS[path] || null;
}

// fetch：
// 1) 只攔截 PWA 自己的 same-origin 檔案
// 2) 192.168.4.1 / 192.168.xxx.xxx 這些設備請求不攔截
// 3) shell 頁面採 network-first；失敗才回快取，避免頁面長期吃到舊版 JS
self.addEventListener("fetch", (e) => {
  const url = new URL(e.request.url);
  const shellKey = getShellCacheKey(url.pathname);

  // 只處理自己來源的 PWA 殼頁/靜態檔
  if (url.origin !== self.location.origin) return;

  if (e.request.method !== "GET") return;

  // 動態 API 請求（cache:'no-store'）不攔截，讓瀏覽器直接打網路
  // 避免 AP 模式下回傳舊快取而誤判連線狀態
  if (e.request.cache === "no-store") return;

  e.respondWith(
    fetch(e.request, { cache: 'no-store' })
      .then(async (resp) => {
        if (!resp || !resp.ok) throw new Error("Bad response status");

        const copy = resp.clone();
        const cache = await caches.open(CACHE);
        if (shellKey) {
          await cache.put(shellKey, copy);
        } else {
          await cache.put(e.request, copy);
        }
        return resp;
      })
      .catch(async () => {
        const cache = await caches.open(CACHE);

        // shell 頁僅回退到目前版本的 canonical key，避免 query 版舊殼交錯
        if (shellKey) {
          const shell = await cache.match(shellKey);
          if (shell) return shell;
        }

        let match = await cache.match(e.request, { ignoreSearch: true });
        if (match) return match;

        // 最終保底首頁
        match = await cache.match("./index.html");
        if (match) return match;

        // 最後保底
        return Response.error();
      })
  );
});