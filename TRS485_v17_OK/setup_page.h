/*
[變更摘要 - 2025-11-25]
- 進度條改為時間基準（35 秒跑至 95%），避免長時間掃描時用戶誤判卡住。
- 百分比顯示改為整數（不顯示小數點）。
- /savewifi 前端請求加 60 秒逾時，涵蓋 220~250 掃描耗時。
- 保留：quickCheck(150ms /ping)、自動 Retry 一次、秒數顯示。
該版本第一次運作都ok,但重新送電後會連不上ip
已修正3次重試,新增 /api/myip + PWA 自動補儲 IP
放寛qucikcheck
*/

const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#2aa6bf">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.json">
<title>HMK 連線狀態</title>
<style>
  :root{
    --top:#279bb3;
    --top2:#32acc5;
    --panel:#f3f3f1;
    --teal:#1a8ea5;
    --line:rgba(255,255,255,.35);
  }
  *{box-sizing:border-box}
  html,body{
    margin:0;
    min-height:100%;
    font-family:"Noto Sans TC",system-ui,sans-serif;
    background:var(--top);
  }
  body{color:#fff}
  .app{
    min-height:100vh;
    display:flex;
    flex-direction:column;
    background:linear-gradient(180deg,var(--top2) 0 42%,var(--panel) 42% 100%);
  }
  .top{
    width:100%;
    max-width:430px;
    margin:0 auto;
    padding:env(safe-area-inset-top,0) 0 0;
  }
  .header{
    height:66px;
    display:flex;
    align-items:center;
    justify-content:center;
    position:relative;
    border-bottom:1px solid rgba(255,255,255,.45);
    padding:0 16px;
  }
  .backBtn{
    position:absolute;
    left:10px;
    top:50%;
    transform:translateY(-50%);
    display:flex;
    align-items:center;
    gap:6px;
    border:0;
    background:transparent;
    color:#e8f6fb;
    font-size:16px;
    padding:8px 6px;
    cursor:pointer;
  }
  .backBtn svg{
    width:26px;
    height:26px;
    stroke:currentColor;
    fill:none;
    stroke-width:2.2;
    stroke-linecap:round;
    stroke-linejoin:round;
  }
  .brand{font-size:40px;font-weight:900;letter-spacing:.02em;line-height:1}
  .panel{
    max-width:430px;
    margin:0 auto;
    width:100%;
    padding:18px 18px calc(22px + env(safe-area-inset-bottom,0));
    color:var(--teal);
  }
  .title{
    color:#fff;
    text-align:center;
    font-size:22px;
    font-weight:800;
    margin:18px 0 6px;
  }
  .subtitle{
    text-align:center;
    margin:0 0 16px;
    color:rgba(255,255,255,.94);
    font-size:14px;
  }
  .formCard{
    background:#fff;
    border:1px solid #d8e9ed;
    border-radius:16px;
    padding:14px 14px 16px;
    box-shadow:0 10px 24px rgba(0,0,0,.08);
  }
  .stepLabel{
    display:block;
    margin:10px 0 8px;
    font-size:16px;
    font-weight:800;
    color:var(--teal);
    text-align:left;
  }
  input,select{
    padding:10px 12px;
    font-size:16px;
    width:100%;
    border:1px solid #b8d4db;
    border-radius:10px;
    color:#245c67;
    background:#f7fcfd;
  }
  .saveBtn{
    margin-top:14px;
    width:100%;
    min-height:44px;
    border:none;
    border-radius:999px;
    background:linear-gradient(180deg,#2aa6bf,#1a8ea5);
    color:#fff;
    font-size:17px;
    font-weight:800;
    cursor:pointer;
  }
  #progWrap{
    display:none;
    margin-top:14px;
    background:#fff;
    border-radius:12px;
    padding:12px;
    box-shadow:0 8px 24px rgba(0,0,0,.08);
    border:1px solid #d7e8ec;
  }
  #bar{height:14px;background:#e8eefb;border-radius:10px;overflow:hidden}
  #bar>i{display:block;height:100%;width:0;background:linear-gradient(90deg,#4f84ff,#2aa8ff)}
  #topline{display:flex;justify-content:space-between;align-items:center;margin-top:8px}
  #pct{font-weight:700;color:#286a78}
  #sec{font-variant-numeric:tabular-nums;opacity:.85;color:#4f7f8a}
  .hint{color:#5f8b95;font-size:13px;margin-top:6px}
  #msg{
    margin-top:12px;
    min-height:22px;
    color:#4f7f8a;
    font-size:14px;
    text-align:center;
  }
  @media (min-width:480px){
    .app{display:flex;flex-direction:column;align-items:center}
    .top,.panel{width:430px}
    .top{border-top-left-radius:18px;border-top-right-radius:18px;margin-top:10px}
    .panel{border-bottom-left-radius:18px;border-bottom-right-radius:18px}
  }
</style>
</head>
<body>
<div class="app">
  <div class="top">
    <div class="header">
      <button id="backBtn" class="backBtn" type="button" aria-label="返回首頁">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M15 5l-7 7 7 7"/></svg>
        <span>連線狀態</span>
      </button>
      <div class="brand" aria-label="HMK">HMK</div>
    </div>

    <div class="panel">
      <div class="title">Wi-Fi 設定</div>
      <p class="subtitle">請依序完成步驟 1、2、3</p>

      <form id="wifiForm" class="formCard">
        <label class="stepLabel">1. 選擇您的 Wi-Fi 名稱</label>
        <select id="ssidSelect" name="ssid"></select>

        <label class="stepLabel">2. 輸入您的 Wi-Fi 密碼</label>
        <input id="pass" name="pass" type="password" placeholder="輸入密碼">

        <button class="saveBtn" type="submit">3. 儲存設定並取得 IP</button>
      </form>

      <div id="progWrap">
        <div id="bar"><i id="barIn"></i></div>
        <div id="topline">
          <div id="pct">0%</div>
          <div id="sec">0.0 秒</div>
        </div>
        <div class="hint">配置 IP 期間約 30 秒，請保持連線至 HMK‑Setup…</div>
      </div>

      <p id="msg"></p>
    </div>
  </div>
</div>

<script>
function goBackHome(){
  try{
    if(window.parent && window.parent !== window){
      window.parent.postMessage('gohome', '*');
    }
  }catch(e){}
  setTimeout(function(){
    try{ location.href = '/'; }catch(e){}
  }, 120);
}

document.getElementById('backBtn')?.addEventListener('click', goBackHome);


//----------------------------------------------------
// 儲存前快速偵測 AP 是否正常 (150ms /ping)
//----------------------------------------------------
async function quickCheck() {
  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 1000);
    const r = await fetch('/ping', { method:'GET', cache:'no-store', signal: controller.signal });
    clearTimeout(timer);
    if (!r.ok) throw new Error("not ok");
    return true;
  } catch(e) {
    return false;
  }
}

//----------------------------------------------------
// 進度條（時間基準：35 秒跑至 95%）→ 百分比整數
//----------------------------------------------------
function setPct(p){
  p = Math.round(Math.max(0, Math.min(100, p)));
  const barIn=document.getElementById('barIn'); if(barIn){ barIn.style.width=p+'%'; }
  const pct=document.getElementById('pct'); if(pct){ pct.textContent=p+'%'; }
}
let t0=0, tTimer=null, animTimer=null;
const EST_MS = 35000;

function startProgress(){
  const pw = document.getElementById('progWrap');
  if(pw) pw.style.display='block';
  setPct(0);
  t0 = performance.now();

  clearInterval(tTimer);
  tTimer = setInterval(()=>{
    const s=(performance.now()-t0)/1000;
    const sec=document.getElementById('sec');
    if(sec) sec.textContent=s.toFixed(1)+' 秒';
  },100);

  clearInterval(animTimer);
  animTimer = setInterval(()=>{
    const el = performance.now()-t0;
    const p = Math.min(95, (el/EST_MS)*95);
    setPct(p);
  },120);
}

function stopProgressSuccess(){ clearInterval(animTimer); setPct(100); clearInterval(tTimer); }
function stopProgressFail(){ clearInterval(animTimer); clearInterval(tTimer); setPct(100); }

//----------------------------------------------------
// Wi‑Fi 清單掃描
//----------------------------------------------------
async function scan(){
  const sel=document.getElementById('ssidSelect');
  sel.innerHTML='<option>掃描中...</option>';
  try{
    const r=await fetch('/scan',{cache:'no-store'});
    const t=await r.text();
    sel.innerHTML=t;
  }catch(e){
    sel.innerHTML='<option>無法取得清單</option>';
  }
}
scan();

//----------------------------------------------------
// /savewifi（60 秒逾時）
//----------------------------------------------------
async function doSave(ssid, pass){
  const controller = new AbortController();
  const timer = setTimeout(()=>controller.abort(), 60000);
  try{
    const form=new FormData();
    form.append('ssid',ssid);
    form.append('pass',pass);
    const r=await fetch('/savewifi',{method:'POST',body:form,signal:controller.signal});
    return await r.text();
  } finally {
    clearTimeout(timer);
  }
}

//----------------------------------------------------
// 按下儲存設定
//----------------------------------------------------
document.getElementById('wifiForm').onsubmit = async (ev) => {
  ev.preventDefault();
  const ssid = document.getElementById('ssidSelect').value;
  const pass = document.getElementById('pass').value;
  document.getElementById('msg').textContent = '儲存中...';

  const ok = await quickCheck();
  if (!ok) {
    alert('偵測到 HMK-Setup 連線品質偏弱，或手機正在進行 Wi-Fi 驗證。\n\n請靠近設備，並確認手機仍在 HMK-Setup 後再試一次。');
    return;
  }

  const MAX_TRIES = 3;  // ★ 這裡決定總共要嘗試幾次（3 = 第1次 + 自動重試2次）
  let attempt = 0;

  while (attempt < MAX_TRIES) {
    attempt++;

    // 每次嘗試時都重新開始進度條
    startProgress();

    try {
      const t = await doSave(ssid, pass);
      stopProgressSuccess();
      document.open(); document.write(t); document.close();
      return; // 成功就直接離開
    } catch (e) {
      // 失敗的情況
      if (attempt >= MAX_TRIES) {
        // 已經是最後一次了 → 宣告失敗
        stopProgressFail();
        alert('Wi-Fi 設定傳送失敗或逾時。\n\n請確認手機仍連線到「HMK-Setup」，再重試。');
        location.reload();
        return;
      }
      // 還有下一輪就什麼都不用做，讓 while 再跑一次（自動第2次 / 第3次）
    }
  }
};

</script>
</body>
</html>
)rawliteral";

String pageSetupAsync(){ return FPSTR(SETUP_HTML); }

const char SETUP_MINI_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi 設定</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;padding:0;background:#fff;font-family:"Noto Sans TC",system-ui,sans-serif;color:#1a8ea5}
  .wrap{padding:14px}
  .stepLabel{display:block;margin:10px 0 8px;font-size:16px;font-weight:800;color:#1a8ea5;text-align:left}
  input,select{padding:10px 12px;font-size:16px;width:100%;border:1px solid #b8d4db;border-radius:10px;color:#245c67;background:#f7fcfd}
  .saveBtn{margin-top:14px;width:100%;min-height:44px;border:none;border-radius:999px;background:linear-gradient(180deg,#2aa6bf,#1a8ea5);color:#fff;font-size:17px;font-weight:800;cursor:pointer}
  #msg{margin-top:10px;min-height:20px;color:#5f8b95;font-size:13px;text-align:center}
</style>
</head>
<body>
<div class="wrap">
  <form id="wifiForm">
    <label class="stepLabel">1. 選擇您的 Wi-Fi 名稱</label>
    <select id="ssidSelect" name="ssid"></select>

    <label class="stepLabel">2. 輸入您的 Wi-Fi 密碼</label>
    <input id="pass" name="pass" type="password" placeholder="輸入密碼">

    <button class="saveBtn" type="submit">3. 儲存密碼完成設定</button>
  </form>
  <p id="msg"></p>
</div>

<script>
async function scan(){
  const sel=document.getElementById('ssidSelect');
  sel.innerHTML='<option>掃描中...</option>';
  try{
    const r=await fetch('/scan',{cache:'no-store'});
    const t=await r.text();
    sel.innerHTML=t;
  }catch(e){
    sel.innerHTML='<option>無法取得清單</option>';
  }
}

async function doSave(ssid, pass){
  const controller = new AbortController();
  const timer = setTimeout(()=>controller.abort(), 60000);
  try{
    const form=new FormData();
    form.append('ssid',ssid);
    form.append('pass',pass);
    const r=await fetch('/savewifi',{method:'POST',body:form,signal:controller.signal});
    return await r.text();
  } finally {
    clearTimeout(timer);
  }
}

document.getElementById('wifiForm').onsubmit = async (ev) => {
  ev.preventDefault();
  const ssid = document.getElementById('ssidSelect').value;
  const pass = document.getElementById('pass').value;
  document.getElementById('msg').textContent = '儲存中...';

  try{
    const t = await doSave(ssid, pass);
    document.open(); document.write(t); document.close();
  }catch(e){
    document.getElementById('msg').textContent = '儲存失敗，請再試一次。';
  }
};

scan();
</script>
</body>
</html>
)rawliteral";

String pageSetupMiniAsync(){ return FPSTR(SETUP_MINI_HTML); }