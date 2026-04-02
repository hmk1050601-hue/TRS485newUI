#ifndef STATUS_PAGE_H
#define STATUS_PAGE_H

#include <pgmspace.h>

// 把「設備狀態頁」整份 HTML 放在這裡
const char STATUS_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HMK 設備狀態</title>
<!--
v2 修正不同手機無法讀取
-->





<style>
:root{
  --bg:#f2f5fb;
  --blue:#004aad;
  --green:#0a8754;
  --red:#c62828;
  --cardRadius:18px;
  --shadowCard:0 6px 18px rgba(0,0,0,.10);
}
body{
  margin:0;
  background:var(--bg);
  font-family:"Noto Sans TC",system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
  color:#123;
}
.header{
  display:flex;
  justify-content:space-between;
  align-items:center;
  padding:16px 18px 8px;
  max-width:620px;
  margin:0 auto;
}

.headerBtns{
  display:flex;
  flex-direction:row;
  gap:8px;
  align-items:center;
  justify-content:flex-end;
  flex-wrap:wrap;
}

.header h2{
  margin:0;
  font-size:clamp(20px,5vw,26px);
  color:var(--blue);
}
.header button{
  border:0;
  border-radius:999px;
  padding:8px 16px;
  font-size:clamp(14px,3.5vw,16px);
  background:var(--blue);
  color:#fff;
  cursor:pointer;
  font-weight:700;
}
.main{
  padding:0 14px 20px;
  max-width:620px;
  margin:0 auto;
}

/* 總結框 */
.summaryCard{
  background:#fff;
  border-radius:var(--cardRadius);
  box-shadow:var(--shadowCard);
  padding:14px 16px 12px;
  margin-top:10px;
}
.summaryTitle{
  font-size:clamp(16px,4.3vw,18px);
  font-weight:800;
  margin-bottom:4px;
}
.summaryOk{ color:var(--green); }
.summaryErr{ color:var(--red); }
.summaryText{
  font-size:clamp(13px,3.4vw,15px);
  line-height:1.6;
}



/* 各項細部狀態 */
.itemCard{
  background:#fff;
  border-radius:var(--cardRadius);
  box-shadow:var(--shadowCard);
  padding:12px 14px 10px;
  margin-top:10px;
}
.itemHead{
  display:flex;
  justify-content:space-between;
  align-items:center;
  margin-bottom:4px;
}
.itemName{
  font-size:clamp(14px,3.8vw,16px);
  font-weight:700;
}
.badge{
  padding:2px 8px;
  border-radius:999px;
  font-size:11px;
  font-weight:700;
}
.badge-ok{
  background:rgba(10,135,84,.1);
  color:var(--green);
}
.badge-err{
  background:rgba(198,40,40,.1);
  color:var(--red);
}
.itemDesc{
  font-size:clamp(13px,3.4vw,15px);
  color:#555;
  line-height:1.6;
}

/* 電熱管健康度參考（進度條） */
.hhBarWrap{
  margin-top:10px;
  background:#eee;
  border-radius:999px;
  height:16px;
  overflow:hidden;
  position:relative;
}
.hhBar{
  height:100%;
  width:0%;
  background:var(--green);
  transition:width .35s ease;
}
.hhBar.bad{ background:var(--red); }
.hhBar.calib{ background:var(--blue); }
.hhBarText{
  position:absolute;
  left:50%;
  top:50%;
  transform:translate(-50%,-50%);
  font-size:12px;
  font-weight:800;
  color:#fff;
  white-space:nowrap;
  text-shadow:0 1px 2px rgba(0,0,0,.25);
}
.hhMeta{
  margin-top:8px;
  font-size:12px;
  color:#666;
  line-height:1.5;
}
.hhHint{
  margin-top:6px;
  font-size:12px;
  font-weight:800;
  color:var(--red);
}

.hhHeadRight{
  display:flex;
  gap:8px;
  align-items:center;
}
.hhResetBtn{
  border:0;
  border-radius:999px;
  padding:2px 10px;
  font-size:11px;
  font-weight:800;
  background:rgba(0,74,173,.12);
  color:var(--blue);
  cursor:pointer;
}
.hhResetBtn:active{ opacity:.85; }
.hhResetBtn:disabled{ opacity:.55; cursor:not-allowed; }

/* 下方說明 + IP */
#ipInfo{
  font-size:clamp(13px,3.3vw,15px);
  color:#555;
  margin-top:10px;
}
#msg{
  margin-top:8px;
  min-height:20px;
  font-size:clamp(13px,3.4vw,15px);
}
#msg.ok{ color:var(--green); }
#msg.err{ color:var(--red); }

</style>
</head>
<body>
<div class="header">
  <h2>設備狀態檢查</h2>

  <div class="headerBtns">
    <button type="button" id="refreshBtn">重新偵測</button>
    <button type="button" id="powerLogBtn">用電記錄</button>
      </div>
</div>

<div class="main">
  <div id="summaryCard" class="summaryCard">
 
    <div id="summaryTitle" class="summaryTitle">正在讀取設備狀態...</div>
    <div id="summaryText" class="summaryText">
      請確認手機已連線至設備（Wi-Fi AP 或原本的路由器）。
    </div>
  </div>

  <div id="itemsWrap"></div>

  <div id="msg"></div>
  <div id="ipInfo"></div>
</div>


<script>
(function(){
  const msgEl    = document.getElementById('msg');
  const ipInfoEl = document.getElementById('ipInfo');
  const powerLogBtn = document.getElementById('powerLogBtn');
  
  const summaryCard  = document.getElementById('summaryCard');
  const summaryTitle = document.getElementById('summaryTitle');
  const summaryText  = document.getElementById('summaryText');
  const itemsWrap    = document.getElementById('itemsWrap');
  const refreshBtn   = document.getElementById('refreshBtn');

  // 兼容舊 key（HMK_IP）與新 key（hmk_ip）

  // ✔ 修正：若本頁是從設備本機 (http://IP/...) 打開，優先使用目前主機 IP
  let devIp = "";
  (function(){
    try{
      const host = location.hostname || "";
      // 只要是 IPv4（例如 192.168.x.x、10.x.x.x、172.x.x.x…）就直接用
      if (/^\d+\.\d+\.\d+\.\d+$/.test(host)) {
        devIp = host;
        return;
      }
    }catch(e){}
    devIp = localStorage.getItem('hmk_ip') || localStorage.getItem('HMK_IP') || "";
  })();




  // 全部異常時的自動重試次數（防止無限重試）
  let noiseRetry = 0;


  function updateIpInfo(){
    if (!ipInfoEl) return;
    if (devIp) {
      ipInfoEl.textContent =
        "目前設備 IP：" + devIp + "（同時支援 AP：192.168.4.1）";
    } else {
      ipInfoEl.textContent =
        "尚未儲存設備 IP，將優先以 192.168.4.1 嘗試連線。";
    }
  }
  updateIpInfo();

  function showMsg(text, isErr){
    msgEl.textContent = text || "";
    msgEl.className = isErr ? "err" : "ok";
  }

  // ------------------------------------------------------------
  // 建立候選 base URL：AP 優先，再試記憶 IP
  // ------------------------------------------------------------
function candidateBases(){
  const list = [];
  const add = (b)=>{ if(b && !list.includes(b)) list.push(b); };
  const norm = (s)=> String(s || '').trim().replace(/^https?:\/\//,'').replace(/\/.*$/,'');

  let currentHost = '';
  try{
    currentHost = norm(location.hostname || '');
  }catch(e){}
  const currentBase = (/^(\d+\.){3}\d+$/.test(currentHost)) ? ('http://' + currentHost) : '';

  if (currentBase && currentHost !== '192.168.4.1') add(currentBase);

  if (devIp) {
    let host = norm(devIp);
    if (host && host !== currentHost && host !== '192.168.4.1') add('http://' + host);
  }

  add("http://192.168.4.1");
  return list;
}

async function fetchWithTimeout(url, options, ms){
  const ctrl = new AbortController();
  const t = setTimeout(()=>ctrl.abort(), ms);
  try{
    return await fetch(url, Object.assign({}, options || {}, {signal: ctrl.signal}));
  } finally {
    clearTimeout(t);
  }
}

// smartFetch：依序嘗試多個 base，成功就記住 IP（只要不是 AP）
async function smartFetch(path, options){
  const bases = candidateBases();
  let lastErr;
  for (const b of bases){
    const url = b + path;
    const timeoutMs = (b === 'http://192.168.4.1') ? 800 : 1400;
    try{
      const res = await fetchWithTimeout(url, options || {}, timeoutMs);
      if (!res.ok){
        lastErr = new Error("HTTP " + res.status);
        continue;
      }
      try{
        const u = new URL(url);
        const host = u.hostname;
        if (host !== "192.168.4.1" && host && host !== devIp){
          devIp = host;
          localStorage.setItem('hmk_ip', devIp);
          updateIpInfo();
        }
      }catch(e){}
      return res;
    }catch(e){
      lastErr = e;
    }
  }
  throw lastErr || new Error("無法連線設備");
}

  // ------------------------------------------------------------
  // 校時：把手機的 年/月/日 + 時:分 + 星期 同步到設備
  // （支援舊版：只帶 h/m 也 OK）
  // ------------------------------------------------------------
  async function syncTimeFromPhone(){
    try{
      const d = new Date();
      const y  = d.getFullYear();
      const mo = d.getMonth() + 1;
      const da = d.getDate();
      const hh = d.getHours();
      const mm = d.getMinutes();
      let wd = d.getDay(); // 0=Sun..6=Sat
      wd = (wd === 0) ? 7 : wd; // 1..7，週一=1..週日=7
      await smartFetch(`/api/time?y=${y}&mo=${mo}&d=${da}&h=${hh}&m=${mm}&wd=${wd}`, { method:'POST', cache:'no-store' });

    }catch(e){}
  }



  // ------------------------------------------------------------
  // 位址 37 bit 定義
  // ------------------------------------------------------------
  const faultDefs = [
    {
      bit:0,
      name:"時間 IC",
      okText:"時間 IC 正常",
      errText:"時間 IC 異常，請檢查 RTC 模組或電池"
    },
    {
      bit:1,
      name:"空燒保護",
      okText:"空燒保護正常",
      errText:"偵測到空燒異常，請檢查水位與感測器"
    },
    {
      bit:2,
      name:"過溫保護",
      okText:"過溫保護正常",
      errText:"偵測到過溫異常，請檢查溫度與安全裝置"
    },
    {
      bit:4,
      name:"上感溫開路",
      okText:"上感溫開路確認正常",
      errText:"上感溫開路異常，可能感測線路斷線"
    },
    {
      bit:5,
      name:"上感溫短路",
      okText:"上感溫短路確認正常",
      errText:"上感溫短路異常，請檢查感測元件是否短路"
    },
    {
      bit:6,
      name:"下感溫開路",
      okText:"下感溫開路確認正常",
      errText:"下感溫開路異常，可能感測線路斷線"
    },
    {
      bit:7,
      name:"下感溫短路",
      okText:"下感溫短路確認正常",
      errText:"下感溫短路異常，請檢查感測元件是否短路"
    }
  ];


  // 所有定義的異常 bit 的總合，用來判斷「全部都異常」
  const ALL_FAULT_MASK = faultDefs.reduce((mask, fd)=> mask | (1 << fd.bit), 0);





  function renderItems(raw,dev){
    itemsWrap.innerHTML = "";
    let errorCount = 0;

 // ----------------------------------------------------------
    // 電熱管健康度參考：在「時間 IC」上方顯示一個進度條卡片
    // ----------------------------------------------------------
    try{
      const stdSec  = Number(dev && dev.hhStdSec  || 0);
      const lastSec = Number(dev && dev.hhLastSec || 0);
      const cnt     = Number(dev && dev.hhCount   || 0);
      const pct     = Math.max(0, Math.min(100, Number(dev && dev.hhPct || 0)));

      const card = document.createElement('div');
      card.className = "itemCard";

      const head = document.createElement('div');
      head.className = "itemHead";

      const name = document.createElement('div');
      name.className = "itemName";
      name.textContent = "電熱管健康度參考";

      const badge = document.createElement('div');
      badge.className = "badge " + ((stdSec>0 && lastSec>0 && pct<60) ? "badge-err" : "badge-ok");
      badge.textContent = (stdSec>0 && lastSec>0) ? (pct + "%") : (Math.min(10,cnt) + "/10");
//============================
     const right = document.createElement('div');
right.className = "hhHeadRight";

const resetBtn = document.createElement('button');
resetBtn.type = "button";
resetBtn.className = "hhResetBtn";
resetBtn.textContent = "重設";

resetBtn.addEventListener('click', async function(ev){
  ev.preventDefault();
  ev.stopPropagation();

  if (!confirm("確定要重設電熱管健康度基準？\n重設後需重新完成 10 次 60→65°C 建立標準值。")) return;

  resetBtn.disabled = true;
  const oldText = resetBtn.textContent;
  resetBtn.textContent = "重設中…";

  try{
    await smartFetch("/api/hhreset", { method:"POST", cache:"no-store" });
    showMsg("已重設電熱管健康度基準（請重新完成 10 次 60→65°C 建立標準值）。", false);
    loadStatus();
  }catch(e){
    showMsg("重設失敗：" + (e && e.message ? e.message : e), true);
  }finally{
    resetBtn.textContent = oldText;
    resetBtn.disabled = false;
  }
});

right.appendChild(badge);
right.appendChild(resetBtn);

head.appendChild(name);
head.appendChild(right);

//============================
      const barWrap = document.createElement('div');
      barWrap.className = "hhBarWrap";

      const bar = document.createElement('div');
      bar.className = "hhBar";
      const w = (stdSec>0 && lastSec>0) ? pct : (Math.min(10,cnt) / 10 * 100);
      bar.style.width = w + "%";
      if (!(stdSec>0 && lastSec>0)) bar.classList.add("calib");
      if ((stdSec>0 && lastSec>0 && pct<60)) bar.classList.add("bad");

      const barText = document.createElement('div');
      barText.className = "hhBarText";
      barText.textContent = (stdSec>0 && lastSec>0)
        ? ("電熱管健康度參考 " + pct + "%")
        : ("建立標準值中 " + Math.min(10,cnt) + "/10");

      barWrap.appendChild(bar);
      barWrap.appendChild(barText);

      const meta = document.createElement('div');
      meta.className = "hhMeta";
      if (stdSec>0 && lastSec>0) {
        meta.textContent =
          `標準：${(stdSec/60).toFixed(1)} 分（10次平均）｜本次：${(lastSec/60).toFixed(1)} 分`;
      } else {
        meta.textContent =
          "剛安裝電熱管時：請完成 10 次 60→65°C 的加熱，系統會自動建立標準值。";
      }

      const hint = document.createElement('div');
      hint.className = "hhHint";
      if (stdSec>0 && lastSec>0 && pct<60) {
        hint.textContent = "健康度低於 60%：建議更換電熱管";
      }

      card.appendChild(head);
      card.appendChild(barWrap);
      card.appendChild(meta);
      if (hint.textContent) card.appendChild(hint);

      itemsWrap.appendChild(card);
    }catch(e){}

    faultDefs.forEach(fd=>{
      const mask = 1 << fd.bit;
      const isErr = (raw & mask) !== 0;

      if (isErr) errorCount++;

      const card = document.createElement('div');
      card.className = "itemCard";

      const head = document.createElement('div');
      head.className = "itemHead";

      const name = document.createElement('div');
      name.className = "itemName";
      name.textContent = fd.name;

      const badge = document.createElement('div');
      badge.className = "badge " + (isErr ? "badge-err" : "badge-ok");
      badge.textContent = isErr ? "異常" : "正常";

      head.appendChild(name);
      head.appendChild(badge);

      const desc = document.createElement('div');
      desc.className = "itemDesc";
      desc.textContent = isErr ? fd.errText : fd.okText;

      card.appendChild(head);
      card.appendChild(desc);
      itemsWrap.appendChild(card);
    });

    // 更新總結框
    if (errorCount === 0) {
      summaryTitle.textContent = "✅ 全部保護與感測功能正常";
      summaryTitle.className = "summaryTitle summaryOk";
      summaryText.textContent =
        "目前未偵測到主機異常，時間 IC、空燒、過溫以及上下感溫皆處於正常狀態。";
    } else {
      summaryTitle.textContent = "⚠ 偵測到 " + errorCount + " 項異常";
      summaryTitle.className = "summaryTitle summaryErr";
      summaryText.textContent =
        "請依下方各項異常說明檢查設備，如異常無法排除，建議聯繫服務人員處理。";
    }
  }

  // ------------------------------------------------------------
  // 讀取 /api/devstatus
  // ------------------------------------------------------------
  async function loadStatus(){
    showMsg("正在讀取設備狀態...", false);
    try{
      const res = await smartFetch("/api/devstatus", {cache:"no-store"});
      const data = await res.json().catch(()=>({}));
      if (!data.valid) {
        showMsg("尚未取得主機異常狀態，請稍後再試。", true);
        summaryTitle.textContent = "尚未取得資料";
        summaryTitle.className = "summaryTitle summaryErr";
        summaryText.textContent = "設備可能尚未回傳位址 37 的狀態封包，請稍後或重新整理再試一次。";
        return;
      }
            const raw = Number(data.raw || 0) & 0xFF;

      // 若所有定義的異常 bit 都是 1 → 先當作訊號雜訊，優先重試幾次
      if ((raw & ALL_FAULT_MASK) === ALL_FAULT_MASK) {
        if (noiseRetry < 5) {
          noiseRetry++;
          showMsg("偵測到全部異常，可能是暫時訊號雜訊，正在重新確認…", true);
          setTimeout(loadStatus, 600);  // 0.6 秒後自動再讀一次
          return;                       // ★ 不更新畫面（不 renderItems）
        } else {
          // 已經重試多次還是全部異常 → 當成真實狀態顯示給你看
          noiseRetry = 0;
        }
      } else {
        // 有正常 bit（不是全部異常）就重置計數
        noiseRetry = 0;
      }

      renderItems(raw, data);
      showMsg("設備狀態已更新。", false);
    }catch(e){
      noiseRetry = 0;  // 發生錯誤時也重置
      showMsg("無法讀取設備狀態：" + e.message, true);
      summaryTitle.textContent = "連線設備失敗";
      summaryTitle.className = "summaryTitle summaryErr";
      summaryText.textContent =
        "無法透過 AP 或已儲存的 IP 連線至設備，請確認手機目前有連到 HMK-Setup 或家用路由器。";
    }

  }

  // ------------------------------------------------------------
  // 刷新按鍵：手動重新讀取設備狀態
  // ------------------------------------------------------------
  if (refreshBtn) {
    refreshBtn.addEventListener('click', function(e){
      e.preventDefault();
      syncTimeFromPhone();
    loadStatus();
    });
  }






  // ------------------------------------------------------------
  // 返回主頁（通知 PWA 關閉 iframe）
  // ------------------------------------------------------------
  
  // 用電記錄頁
  if (powerLogBtn) {
    powerLogBtn.addEventListener('click', function(e){
      e.preventDefault();
      location.href = '/powerlog.html?_=' + Date.now();

    });
  }



  // 初始化：先等 1.5 秒讓主機自檢告一段落，再讀一次狀態
  showMsg("正在等待設備自檢完成，請稍候...", false);
  syncTimeFromPhone();
  setTimeout(loadStatus, 1500);  // 不夠的話可以改成 2000 或 3000

  // 每 10 秒自動刷新一次（避免背景頁不停打 API）
  setInterval(function(){
    if (document.visibilityState !== 'visible') return;
    loadStatus();
  }, 10000);
  
})();
</script>
</body>
</html>

)rawliteral";

inline String pageStatusHtml(){
  return FPSTR(STATUS_HTML);
}

#endif
