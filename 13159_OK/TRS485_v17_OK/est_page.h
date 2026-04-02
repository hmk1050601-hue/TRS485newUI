#ifndef EST_PAGE_H
#define EST_PAGE_H

#include <pgmspace.h>

const char EST_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Est</title>
<style>
:root{
  --bg:#f2f5fb;
  --blue:#004aad;
  --green:#0a8754;
  --red:#c62828;
  --cardRadius:18px;
  --shadowCard:0 6px 18px rgba(0,0,0,.10);
  --shadowBtn:0 8px 18px rgba(0,0,0,.12);
}
*{box-sizing:border-box}
body{
  margin:0;
  background:var(--bg);
  font-family:system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial;
  color:#102030;
}
header{padding:18px 14px 10px}
h1{margin:0;font-size:clamp(18px,5.5vw,22px);letter-spacing:.2px}
main{padding:0 14px 22px;max-width:620px;margin:0 auto}
.card{
  background:#fff;
  border-radius:var(--cardRadius);
  box-shadow:var(--shadowCard);
  padding:14px;
  margin:12px 0;
}
.cardTitle{font-size:16px;font-weight:900;color:var(--blue);margin:0 0 10px}
.note{font-size:13px;line-height:1.7;color:#4b5b6a}
.infoGrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.infoItem{background:#f7f9ff;border:1px solid #dbe5ff;border-radius:14px;padding:10px 12px}
.infoLabel{font-size:12px;color:#5f6c7b;font-weight:900}
.infoVal{font-size:18px;font-weight:900;margin-top:4px}
.row{display:grid;grid-template-columns:120px 1fr;gap:10px;align-items:center;margin:10px 0}
.row label{font-size:14px;font-weight:900;color:#223344}
.selWrap{padding:8px 10px;border:1px solid #d7deea;border-radius:12px;background:#fff}
select{
  width:100%;border:0;background:transparent;font-size:16px;font-weight:900;padding:8px 0;outline:none;
  appearance:none;-webkit-appearance:none;-moz-appearance:none;text-align:center;text-align-last:center;
}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}
button{
  border:0;border-radius:16px;padding:13px 16px;font-weight:900;font-size:15px;cursor:pointer;box-shadow:var(--shadowBtn)
}
.btnBlue{background:var(--blue);color:#fff}
.btnGreen{background:var(--green);color:#fff}
.btnGray{background:#fff;color:var(--blue);border:1px solid #d5def0;box-shadow:none}
#toggleBtn.off{background:#fff;color:var(--red);border:1px solid #f2c5c5;box-shadow:none}
#msg{margin:10px 2px 0;font-size:13px;font-weight:900;min-height:20px}
#msg.ok{color:var(--green)}
#msg.err{color:var(--red)}
@media (max-width:520px){
  .infoGrid,.actions{grid-template-columns:1fr}
  .row{grid-template-columns:1fr}
}
</style>
</head>
<body>
<header>
  <h1>Est</h1>
</header>

<main>
  <div class="card">
    <div class="cardTitle">功能說明</div>
    <div class="note">
      重新送電後，系統會從第 1 次重新開始計數。<br>
      當設備再次進入加熱時，會依序套用第 1～第 4 次的設定溫度。<br>
      此功能預設關閉，需按下「啟動鍵」後才會生效。
    </div>
  </div>

  <div class="card">
    <div class="cardTitle">目前狀態</div>
    <div class="infoGrid">
      <div class="infoItem">
        <div class="infoLabel">功能狀態</div>
        <div class="infoVal" id="stateText">--</div>
      </div>
      <div class="infoItem">
        <div class="infoLabel">本次開機已觸發</div>
        <div class="infoVal" id="seqText">--</div>
      </div>
      <div class="infoItem">
        <div class="infoLabel">目前加熱</div>
        <div class="infoVal" id="heatText">--</div>
      </div>
      <div class="infoItem">
        <div class="infoLabel">最近套用</div>
        <div class="infoVal" id="lastText">--</div>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="cardTitle">加熱設定值</div>
    <div class="row"><label for="t1">第一次加熱</label><div class="selWrap"><select id="t1"></select></div></div>
    <div class="row"><label for="t2">第二次加熱</label><div class="selWrap"><select id="t2"></select></div></div>
    <div class="row"><label for="t3">第三次加熱</label><div class="selWrap"><select id="t3"></select></div></div>
    <div class="row"><label for="t4">第四次加熱</label><div class="selWrap"><select id="t4"></select></div></div>

    <div class="actions">
      <button id="saveBtn" class="btnBlue" type="button">儲存設定</button>
      <button id="toggleBtn" class="btnGreen" type="button">啟動鍵</button>
      <button id="backBtn" class="btnGray" type="button" style="grid-column:1/-1">返回時間設定</button>
    </div>
    <div id="msg"></div>
  </div>
</main>

<script>
(function(){
  const $ = (s)=>document.querySelector(s);
  const ids = ['t1','t2','t3','t4'];
  let current = null;
  let tempDirty = false;

  function fillTempSelect(sel){
    sel.innerHTML = '';
    for(let t=40;t<=70;t++){
      const o = document.createElement('option');
      o.value = String(t);
      o.textContent = t + '°C';
      sel.appendChild(o);
    }
  }
  ids.forEach(id => fillTempSelect($('#' + id)));

  function showMsg(text, ok){
    const el = $('#msg');
    el.textContent = text || '';
    el.className = ok ? 'ok' : 'err';
  }

  function renderStatus(j){
    current = j || {};
    $('#stateText').textContent = j.enabled ? '已啟用' : '未啟用';
    $('#seqText').textContent = String(j.seq || 0) + ' / 4';
    $('#heatText').textContent = j.heating ? '加熱中' : '未加熱';
    $('#lastText').textContent = (j.lastSeq && j.lastTemp) ? ('第' + j.lastSeq + '次 / ' + j.lastTemp + '°C') : '--';

    const btn = $('#toggleBtn');
    btn.textContent = j.enabled ? '停用功能' : '啟動鍵';
    btn.className = j.enabled ? 'btnGreen' : 'off';
  }

  function renderTemps(j){
    $('#t1').value = String(j.t1 || 50);
    $('#t2').value = String(j.t2 || 50);
    $('#t3').value = String(j.t3 || 50);
    $('#t4').value = String(j.t4 || 50);
    tempDirty = false;
  }

  function render(j, overwriteTemps){
    renderStatus(j || {});
    if (overwriteTemps !== false) renderTemps(j || {});
  }

  async function loadEst(silent, overwriteTemps){
    const r = await fetch('/api/est', {cache:'no-store'});
    const j = await r.json();
    render(j, overwriteTemps);
    if (!silent) showMsg('已讀取', true);
  }

  async function saveTemps(){
    showMsg('儲存中…', true);
    const body = new URLSearchParams();
    ids.forEach(id => body.set(id, $('#' + id).value));
    if (current && current.enabled) body.set('enabled', '1');
    const r = await fetch('/api/est', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    const j = await r.json();
    render(j, true);
    showMsg('已儲存設定', true);
  }

  async function toggleEst(){
    const nextEnabled = !(current && current.enabled);
    showMsg(nextEnabled ? '啟用中…' : '停用中…', true);
    const body = new URLSearchParams();
    ids.forEach(id => body.set(id, $('#' + id).value));
    body.set('enabled', nextEnabled ? '1' : '0');
    const r = await fetch('/api/est', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    const j = await r.json();
    render(j, true);
    showMsg(nextEnabled ? '已啟用，並已重新開始計數' : '已停用', true);
  }

  ids.forEach(id => {
    $('#' + id).addEventListener('change', ()=>{ tempDirty = true; });
  });

  $('#saveBtn').addEventListener('click', ()=> saveTemps().catch(()=>showMsg('儲存失敗', false)));
  $('#toggleBtn').addEventListener('click', ()=> toggleEst().catch(()=>showMsg('切換失敗', false)));
  $('#backBtn').addEventListener('click', ()=>{ location.href = '/schedule.html?_=' + Date.now(); });

  loadEst(false, true).catch(()=>showMsg('載入失敗', false));
  setInterval(()=>{ loadEst(true, !tempDirty).catch(()=>{}); }, 2000);
})();
</script>
</body>
</html>
)rawliteral";

inline String pageEstHtml(){
  return FPSTR(EST_HTML);
}

#endif
