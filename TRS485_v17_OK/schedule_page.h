#ifndef SCHEDULE_PAGE_H
#define SCHEDULE_PAGE_H

#include <pgmspace.h>

const char SCHEDULE_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#289fb8">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>HMK 時間設定</title>
<style>
:root{
  --top:#279bb3;
  --top2:#32acc5;
  --cream:#f5e4a1;
  --panel:#f3f3f1;
  --line:rgba(255,255,255,.36);
  --ink:#0d4a58;
  --inkSoft:#80afbb;
  --teal:#29a1ba;
  --tealDark:#1b8ca3;
  --danger:#da5b5b;
  --panelRadius:22px;
  --panelLift:28px;
  --shadow:0 14px 30px rgba(0,0,0,.12);
}
*{box-sizing:border-box}
html,body{
  margin:0;
  min-height:100%;
  background:var(--top);
  font-family:"Noto Sans TC",system-ui,sans-serif;
}
body{
  color:#fff;
}
button,select{
  font:inherit;
}
main{
  min-height:100vh;
  background:linear-gradient(180deg,var(--top2) 0 60%,var(--panel) 60% 100%);
}
.hero{
  max-width:430px;
  margin:0 auto;
  padding:env(safe-area-inset-top,0) 0 0;
  position:relative;
}
.heroInner{
  padding:16px 18px 18px;
}
.btnHiddenTap{
  position:absolute;
  left:50%;
  top:6px;
  transform:translateX(-50%);
  width:160px;
  min-height:44px;
  border:0;
  background:transparent;
  opacity:0;
  cursor:pointer;
}
.boardMeta{
  display:flex;
  align-items:center;
  justify-content:center;
  gap:8px;
  color:rgba(255,255,255,.88);
  font-size:13px;
  font-weight:800;
  letter-spacing:.08em;
}
.boardMetaValue{
  font-variant-numeric:tabular-nums;
}
.heroLabel{
  margin-top:18px;
  text-align:center;
  color:rgba(255,255,255,.88);
  font-size:26px;
  font-weight:700;
  letter-spacing:.22em;
}
.timerToggle{
  display:block;
  width:100%;
  border:0;
  background:transparent;
  color:var(--cream);
  font-size:clamp(76px,24vw,104px);
  font-weight:900;
  line-height:.92;
  letter-spacing:-.08em;
  padding:6px 0 0;
  cursor:pointer;
  text-shadow:0 10px 24px rgba(0,0,0,.12);
}
.timerToggle.running{color:#fff}
.timerToggle:disabled{opacity:.68;cursor:wait}
.timerToggle:active{transform:scale(.985)}
.heroHint{
  text-align:center;
  color:rgba(255,255,255,.95);
  font-size:14px;
  font-weight:700;
}
.slotStrip{
  display:grid;
  grid-template-columns:repeat(4,minmax(0,1fr));
  gap:0;
  margin-top:24px;
}
.slotCard{
  border:0;
  border-right:1px solid var(--line);
  background:transparent;
  color:#fff;
  min-width:0;
  min-height:102px;
  padding:0 6px;
  cursor:pointer;
  text-align:center;
}
.slotCard:last-child{border-right:0}
.slotCard.isSelected{background:rgba(255,255,255,.06)}
.slotName{
  font-size:clamp(13px,3.4vw,16px);
  font-weight:900;
}
.slotStatus{
  margin-top:4px;
  font-size:12px;
  color:rgba(255,255,255,.72);
}
.slotCard.isEnabled .slotStatus{color:var(--cream)}
.slotTime{
  margin-top:9px;
  color:#eefcff;
  font-size:clamp(13px,3.4vw,18px);
  font-weight:700;
  line-height:1.25;
  letter-spacing:.01em;
  font-variant-numeric:tabular-nums;
  word-break:keep-all;
}
.slotCard.isDisabled .slotTime{opacity:.56}
.panel{
  position:relative;
  max-width:430px;
  margin:calc(-1 * var(--panelLift)) auto 0;
  background:var(--panel);
  border-top-left-radius:var(--panelRadius);
  border-top-right-radius:var(--panelRadius);
  min-height:38vh;
  padding:18px 18px calc(22px + env(safe-area-inset-bottom,0));
  box-shadow:0 -12px 28px rgba(0,0,0,.05);
}
.toggleRow{
  display:grid;
  grid-template-columns:repeat(4,minmax(0,1fr));
  gap:12px;
}
.stateBtn{
  width:56px;
  height:56px;
  justify-self:center;
  border-radius:50%;
  border:2px solid var(--teal);
  background:#fff;
  color:var(--teal);
  box-shadow:0 10px 18px rgba(41,161,186,.16);
  font-size:31px;
  font-weight:900;
  line-height:1;
  cursor:pointer;
  transition:transform .08s ease,box-shadow .12s ease,border-color .12s ease,color .12s ease;
}
.stateBtn.disabled{
  color:#86aab4;
  border-color:#c5dae0;
  box-shadow:none;
}
.stateBtn.selected{
  transform:translateY(-2px);
  box-shadow:0 14px 22px rgba(41,161,186,.18);
}
.stateBtn:active{transform:translateY(1px)}
.editor{
  margin-top:18px;
}
.editorRow{
  display:grid;
  grid-template-columns:52px 1fr;
  align-items:center;
  gap:12px;
  margin:14px 0;
}
.editorRow label{
  color:var(--tealDark);
  font-size:16px;
  font-weight:900;
}
.selectWrap,.timeShell{
  display:flex;
  align-items:center;
  justify-content:center;
  min-height:44px;
  padding:0 14px;
  border:1.5px solid #bfe1e7;
  border-radius:999px;
  background:#fff;
}
.selectWrap select,.timeShell select{
  width:100%;
  border:0;
  background:transparent;
  color:var(--tealDark);
  font-size:18px;
  font-weight:800;
  outline:none;
  appearance:none;
  -webkit-appearance:none;
  -moz-appearance:none;
  text-align:center;
  text-align-last:center;
}
.timeShell{
  padding:0 10px;
}
.timeShellInner{
  width:100%;
  display:grid;
  grid-template-columns:1fr 14px 1fr;
  align-items:center;
  gap:0;
}
.timeColon{
  text-align:center;
  color:#72a7b4;
  font-size:19px;
  font-weight:900;
}
.timePair{
  display:grid;
  grid-template-columns:minmax(0,1fr) 22px minmax(0,1fr);
  gap:8px;
  align-items:center;
}
.dash{
  color:#7aa6b1;
  font-size:24px;
  font-weight:700;
  text-align:center;
}
.actionRow{
  display:grid;
  grid-template-columns:1fr 1.25fr;
  gap:12px;
  margin-top:20px;
}
.btnSub,.btnMain,.btnDev,.btn.primary{
  display:flex;
  align-items:center;
  justify-content:center;
  min-height:52px;
  border:0;
  border-radius:18px;
  color:#fff;
  font-size:17px;
  font-weight:800;
  cursor:pointer;
  box-shadow:inset 0 2px 0 rgba(255,255,255,.28),0 6px 0 rgba(0,0,0,.18),0 8px 18px rgba(0,0,0,.14);
  transition:transform .06s ease,box-shadow .1s ease,filter .1s ease;
}
.btnMain{background:linear-gradient(to bottom,#2aa6c1,#167b93)}
.btnSub{background:linear-gradient(to bottom,#9aaeb6,#708891)}
.btnDev{display:none;margin-top:12px;width:100%;background:linear-gradient(to bottom,#5c6df7,#3343c8)}
.btnDev.show{display:flex}
.btnMain:active,.btnSub:active,.btnDev:active,.btn.primary:active{
  transform:translateY(3px);
  box-shadow:inset 0 2px 0 rgba(255,255,255,.28),0 2px 0 rgba(0,0,0,.18),0 4px 10px rgba(0,0,0,.1);
}
#msg{
  min-height:22px;
  margin-top:14px;
  text-align:center;
  font-size:14px;
  font-weight:800;
}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.35);display:none;align-items:center;justify-content:center;padding:18px;z-index:999}
.modal.show{display:flex}
.modalCard{width:min(520px,100%);background:#fff;border-radius:20px;box-shadow:0 16px 30px rgba(0,0,0,.22);padding:16px 16px 14px}
.modalTitle{font-size:16px;font-weight:900;color:var(--ink);margin:0 0 8px}
.modalBody{font-size:14px;color:#223344;line-height:1.6}
.modalBody ul{margin:10px 0 0 18px;padding:0}
.modalBody li{margin:6px 0}
.modalActions{margin-top:14px;display:flex;justify-content:flex-end}
.btn.primary{background:linear-gradient(to bottom,#2aa6c1,#167b93)}
@media (min-width:480px){
  body{display:flex;justify-content:center}
  main{width:430px}
  .hero{margin-top:10px}
  .hero,.panel{width:430px}
  .hero{border-top-left-radius:18px;border-top-right-radius:18px;overflow:hidden}
  .panel{border-bottom-left-radius:18px;border-bottom-right-radius:18px}
}
@media (max-width:390px){
  .slotCard{padding:0 3px;min-height:96px}
  .slotName{font-size:12px}
  .slotTime{font-size:12px}
  .editorRow{grid-template-columns:46px 1fr}
  .actionRow{grid-template-columns:1fr}
}
</style>
</head>
<body>
<main>
  <section class="hero">
    <button id="devTapBtn" class="btnHiddenTap" type="button" aria-label="hidden"></button>
    <div class="heroInner">
      <div class="boardMeta">
        <span>機板時間</span>
        <span id="boardNow" class="boardMetaValue">--:--</span>
      </div>
      <div class="heroLabel">定時加熱</div>
      <button id="timerToggleBtn" class="timerToggle" type="button">啟動</button>
      <div id="timerToggleHint" class="heroHint">目前未啟用定時模式，按下可啟動</div>
      <div id="slotStrip" class="slotStrip"></div>
    </div>
  </section>

  <section class="panel">
    <div id="slotToggleRow" class="toggleRow"></div>

    <div class="editor">
      <div class="editorRow">
        <label for="slotSelect">時段</label>
        <div class="selectWrap">
          <select id="slotSelect"></select>
        </div>
      </div>

      <div class="editorRow">
        <label>時間</label>
        <div class="timePair">
          <div id="startPicker"></div>
          <div class="dash">-</div>
          <div id="endPicker"></div>
        </div>
      </div>

      <div class="actionRow">
        <button id="syncBtn" class="btnSub" type="button">時間校正</button>
        <button id="saveBtn" class="btnMain" type="button">儲存設定</button>
      </div>

      <button id="devBtn" class="btnDev" type="button">開發</button>
      <div id="msg"></div>
    </div>
  </section>

</main>

<div id="alertModal" class="modal" aria-hidden="true">
  <div class="modalCard" role="dialog" aria-modal="true" aria-labelledby="alertTitle">
    <div id="alertTitle" class="modalTitle">提醒</div>
    <div id="alertBody" class="modalBody"></div>
    <div class="modalActions">
      <button id="alertOk" class="btn primary" type="button">知道了</button>
    </div>
  </div>
</div>

<script>
(function(){
  const FIRST_EXTRA = 4;
  const LAST_EXTRA = 8;
  const SLOT_META = [
    { index:0, label:'殺菌模式', shortLabel:'殺菌' },
    { index:1, label:'時段1', shortLabel:'時段1' },
    { index:2, label:'時段2', shortLabel:'時段2' },
    { index:3, label:'時段3', shortLabel:'時段3' }
  ];
  const rows = SLOT_META.map((slot)=>({
    index: slot.index,
    label: slot.label,
    shortLabel: slot.shortLabel,
    enabled: false,
    start: slot.index === 0 ? '23:00' : '00:00',
    end: '00:00',
    summaryEl: null,
    toggleEl: null
  }));
  const slotStrip = document.getElementById('slotStrip');
  const slotToggleRow = document.getElementById('slotToggleRow');
  const slotSelect = document.getElementById('slotSelect');
  const startPickerHost = document.getElementById('startPicker');
  const endPickerHost = document.getElementById('endPicker');
  const msgEl = document.getElementById('msg');
  const boardNowEl = document.getElementById('boardNow');
  const timerToggleBtn = document.getElementById('timerToggleBtn');
  const timerToggleHint = document.getElementById('timerToggleHint');
  const devTapBtn = document.getElementById('devTapBtn');
  const devBtn = document.getElementById('devBtn');
  const startPicker = mkTimeSelect();
  const endPicker = mkTimeSelect();
  let devTapCount = 0;
  let devTapTimer = 0;
  let selectedIndex = 0;
  let syncEditor = false;
  let currentMode = null;
  let timerBusy = false;
  let lastScheduleJson = null;

  function setStatus(kind, text){
    msgEl.textContent = text || '';
    if (kind === 'ok') msgEl.style.color = '#0a8754';
    else if (kind === 'busy') msgEl.style.color = '#1877f2';
    else if (kind === 'err') msgEl.style.color = '#c62828';
    else msgEl.style.color = '#4b5b6a';
  }

  async function fetchWithTimeout(url, opt, ms){
    const ctrl = new AbortController();
    const id = setTimeout(()=>ctrl.abort(), ms || 1500);
    try{
      const r = await fetch(url, Object.assign({}, opt||{}, {signal: ctrl.signal}));
      clearTimeout(id);
      return r;
    }catch(e){
      clearTimeout(id);
      throw e;
    }
  }

  function normalizeHost(s){
    if(!s) return '';
    return String(s).trim().replace(/^https?:\/\//i,'').replace(/\/.*$/,'');
  }

  function rememberBoardIp(url){
    try{
      const u = new URL(url);
      const host = u.hostname;
      if(host && host !== '192.168.4.1') localStorage.setItem('hmk_ip', host);
    }catch(e){}
  }

  function candidateBases(){
    const out = [];
    const add = (b)=>{ if(b && !out.includes(b)) out.push(b); };

    let currentHost = '';
    try{
      currentHost = normalizeHost(location.hostname || '');
    }catch(e){}
    const currentBase = (/^(\d+\.){3}\d+$/.test(currentHost)) ? ('http://' + currentHost) : '';

    if (currentBase && currentHost !== '192.168.4.1') add(currentBase);

    const savedIp = normalizeHost(localStorage.getItem('hmk_ip') || localStorage.getItem('HMK_IP') || '');
    if (savedIp && savedIp !== '192.168.4.1') add('http://' + savedIp);

    if (currentBase === 'http://192.168.4.1') add('http://192.168.4.1');
    add('http://192.168.4.1');

    return out;
  }

  async function smartRequest(path, opt, settings){
    const bases = candidateBases();
    const allowHttpError = !!(settings && settings.allowHttpError);
    let lastErr = null;

    for (const base of bases){
      const url = base + path;
      const timeoutMs = (base === 'http://192.168.4.1') ? 700 : 1300;
      try{
        const r = await fetchWithTimeout(url, opt, timeoutMs);
        if (!r || !r.ok){
          if (r && allowHttpError && r.status !== 404) {
            rememberBoardIp(url);
            return r;
          }
          lastErr = new Error('HTTP ' + (r ? r.status : '0'));
          continue;
        }
        rememberBoardIp(url);
        return r;
      }catch(e){
        lastErr = e;
      }
    }

    throw lastErr || new Error('fetch failed');
  }

  function smartFetch(path, opt){
    return smartRequest(path, opt, null);
  }

  const alertModal = document.getElementById('alertModal');
  const alertTitle = document.getElementById('alertTitle');
  const alertBody  = document.getElementById('alertBody');
  const alertOk    = document.getElementById('alertOk');
  let alertAfter = null;

  function showAlert(title, html, after){
    alertTitle.textContent = title || '提醒';
    alertBody.innerHTML = html || '';
    alertAfter = (typeof after === 'function') ? after : null;
    alertModal.classList.add('show');
    alertModal.setAttribute('aria-hidden','false');
  }
  function hideAlert(){
    alertModal.classList.remove('show');
    alertModal.setAttribute('aria-hidden','true');
    const cb = alertAfter; alertAfter = null;
    if (cb) cb();
  }
  alertOk.addEventListener('click', hideAlert);
  alertModal.addEventListener('click', (e)=>{ if(e.target===alertModal) hideAlert(); });


  async function syncTimeFromPhone(){
    const d = new Date();
    const h = d.getHours();
    const m = d.getMinutes();
    const wd = ((d.getDay() + 6) % 7) + 1;
    const y = d.getFullYear();
    const mo = d.getMonth() + 1;
    const dd = d.getDate();

    const body = new URLSearchParams();
    body.set('h', String(h));
    body.set('m', String(m));
    body.set('wd', String(wd));
    body.set('y', String(y));
    body.set('mo', String(mo));
    body.set('d', String(dd));

    const r = await smartFetch('/api/time', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    return await r.json();
  }

  function pad2(n){ return String(n).padStart(2,'0'); }

  function normalizeTime(v, fallback){
    const text = String(v || '').trim();
    if (!/^\d{2}:\d{2}$/.test(text)) return fallback || '00:00';
    return text;
  }

  function parseHM(v){
    const text = normalizeTime(v, '00:00');
    const parts = text.split(':');
    const hh = Number(parts[0]);
    const mm = Number(parts[1]);
    if (!isFinite(hh) || !isFinite(mm) || hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
    return hh * 60 + mm;
  }

  function mkTimeSelect(){
    const wrap = document.createElement('div');
    wrap.className = 'timeShell';

    const inner = document.createElement('div');
    inner.className = 'timeShellInner';

    const selH = document.createElement('select');
    for(let h=0; h<24; h++){
      const o = document.createElement('option');
      o.value = pad2(h);
      o.textContent = pad2(h);
      selH.appendChild(o);
    }

    const colon = document.createElement('div');
  colon.className = 'timeColon';
    colon.textContent = ':';

    const selM = document.createElement('select');
    for(let m=0; m<60; m++){
      const o = document.createElement('option');
      o.value = pad2(m);
      o.textContent = pad2(m);
      selM.appendChild(o);
    }

    inner.appendChild(selH);
    inner.appendChild(colon);
    inner.appendChild(selM);
    wrap.appendChild(inner);

    function set(v){
      if(!v || v.indexOf(':') < 0) { selH.value='00'; selM.value='00'; return; }
      const a = v.split(':');
      selH.value = (a[0] || '00').padStart(2,'0');
      selM.value = (a[1] || '00').padStart(2,'0');
    }
    function get(){ return (selH.value||'00') + ':' + (selM.value||'00'); }

    return { wrap, set, get, selH, selM };
  }

  function bindPicker(picker, onChange){
    const fn = ()=> onChange();
    picker.selH.addEventListener('change', fn);
    picker.selM.addEventListener('change', fn);
  }

  function formatRangeText(row){
    return normalizeTime(row.start, '00:00') + '-' + normalizeTime(row.end, '00:00');
  }

  function updateTimerToggle(){
    const running = Number(currentMode) === 2;
    timerToggleBtn.textContent = running ? '關閉' : '啟動';
    timerToggleBtn.classList.toggle('running', running);
    timerToggleBtn.disabled = timerBusy;
    timerToggleHint.textContent = running
      ? '目前為定時模式，按下可關閉'
      : '目前未啟用定時模式，按下可啟動';
  }

  function updateRowVisual(row){
    if (row.summaryEl) {
      row.summaryEl.classList.toggle('isEnabled', !!row.enabled);
      row.summaryEl.classList.toggle('isDisabled', !row.enabled);
      row.summaryEl.classList.toggle('isSelected', row.index === selectedIndex);
      const st = row.summaryEl.querySelector('.slotStatus');
      const tm = row.summaryEl.querySelector('.slotTime');
      if (st) st.textContent = row.enabled ? '啟用' : '關閉';
      if (tm) tm.textContent = formatRangeText(row);
    }
    if (row.toggleEl) {
      row.toggleEl.textContent = row.enabled ? '✓' : '✕';
      row.toggleEl.classList.toggle('disabled', !row.enabled);
      row.toggleEl.classList.toggle('selected', row.index === selectedIndex);
      row.toggleEl.setAttribute('aria-pressed', row.enabled ? 'true' : 'false');
      row.toggleEl.setAttribute('aria-label', row.label + (row.enabled ? ' 已啟用' : ' 已關閉'));
    }
  }

  function renderAllRows(){
    rows.forEach(updateRowVisual);
  }

  function syncEditorFromSelected(){
    syncEditor = true;
    slotSelect.value = String(selectedIndex);
    startPicker.set(rows[selectedIndex].start);
    endPicker.set(rows[selectedIndex].end);
    syncEditor = false;
    renderAllRows();
  }

  function selectRow(index){
    const next = Number(index);
    if (!isFinite(next) || next < 0 || next >= rows.length) return;
    selectedIndex = next;
    syncEditorFromSelected();
  }

  function toggleRow(index){
    selectRow(index);
    rows[index].enabled = !rows[index].enabled;
    renderAllRows();
  }

  function buildUi(){
    startPickerHost.appendChild(startPicker.wrap);
    endPickerHost.appendChild(endPicker.wrap);

    slotStrip.innerHTML = '';
    slotToggleRow.innerHTML = '';
    slotSelect.innerHTML = '';

    rows.forEach((row)=>{
      const card = document.createElement('button');
      card.type = 'button';
      card.className = 'slotCard';
      card.innerHTML = '<div class="slotName"></div><div class="slotStatus"></div><div class="slotTime"></div>';
      card.querySelector('.slotName').textContent = row.shortLabel;
      card.addEventListener('click', ()=> selectRow(row.index));
      slotStrip.appendChild(card);
      row.summaryEl = card;

      const toggle = document.createElement('button');
      toggle.type = 'button';
      toggle.className = 'stateBtn disabled';
      toggle.addEventListener('click', ()=> toggleRow(row.index));
      slotToggleRow.appendChild(toggle);
      row.toggleEl = toggle;

      const opt = document.createElement('option');
      opt.value = String(row.index);
      opt.textContent = row.label;
      slotSelect.appendChild(opt);
    });

    bindPicker(startPicker, ()=>{
      if (syncEditor) return;
      rows[selectedIndex].start = startPicker.get();
      updateRowVisual(rows[selectedIndex]);
    });
    bindPicker(endPicker, ()=>{
      if (syncEditor) return;
      rows[selectedIndex].end = endPicker.get();
      updateRowVisual(rows[selectedIndex]);
    });

    slotSelect.addEventListener('change', ()=> selectRow(slotSelect.value));
    syncEditorFromSelected();
    updateTimerToggle();
  }

  function applyScheduleToUi(items){
    lastScheduleJson = items || [];
    rows.forEach((row)=>{
      const src = items[row.index] || {};
      row.enabled = !!src.enabled;
      row.start = normalizeTime(src.start, row.index === 0 ? '23:00' : '00:00');
      row.end = normalizeTime(src.end, '00:00');
    });
    syncEditorFromSelected();
  }

  async function loadSchedule(){
    const r = await smartFetch('/api/schedule', {cache:'no-store'});
    const j = await r.json();
    if (!j || !j.items) throw new Error('bad json');
    applyScheduleToUi(j.items);
    return j;
  }

  async function loadBoardSchedule(){
    const r = await smartFetch('/api/boardsched', {cache:'no-store'});
    const j = await r.json();
    if (!j) throw new Error('bad json');
    boardNowEl.textContent = j.boardTime || '--:--';
    if (j.valid && Array.isArray(j.items)) {
      for(let i=1;i<=3;i++){
        const it = j.items[i-1] || {};
        if (it.start) rows[i].start = normalizeTime(it.start === '--:--' ? '00:00' : it.start, '00:00');
        if (it.end) rows[i].end = normalizeTime(it.end === '--:--' ? '00:00' : it.end, '00:00');
      }
      syncEditorFromSelected();
    }
    return j;
  }

  async function loadStatus(){
    const r = await smartFetch('/status?_=' + Date.now(), {cache:'no-store'});
    const j = await r.json();
    currentMode = Number(j && j.m);
    updateTimerToggle();
    return j;
  }

  async function toggleTimerMode(){
    if (timerBusy) return;
    const turnOn = Number(currentMode) !== 2;
    timerBusy = true;
    updateTimerToggle();
    setStatus('busy', turnOn ? '切換為定時模式…' : '關閉定時模式…');
    try{
      const r = await smartRequest(turnOn ? '/timermode' : '/shutdown', { method:'POST' }, { allowHttpError:true });
      if (!r.ok) {
        const text = (await r.text()) || '切換失敗';
        showAlert('提醒', text);
        setStatus('err', text);
        return;
      }
      await loadStatus().catch(()=>{});
      setStatus('ok', turnOn ? '已切換為定時模式' : '已關閉定時模式');
    }catch(e){
      setStatus('err', '切換失敗');
    }finally{
      timerBusy = false;
      updateTimerToggle();
    }
  }

  function validateBeforeSave(){
    for (let i = 0; i < rows.length; i++) {
      const row = rows[i];
      if (!row.enabled) continue;
      if (parseHM(row.start) === parseHM(row.end)) {
        showAlert('提醒', row.label + ' 的開始與結束時間不能相同。');
        return false;
      }
    }
    return true;
  }

  async function saveSchedule(){
    if (!validateBeforeSave()) return;
    setStatus('busy', '儲存中…');

    const body = new URLSearchParams();

    for(let i=0;i<=3;i++){
      const row = rows[i];
      body.set('en' + i, row.enabled ? '1' : '0');
      body.set('s' + i, row.start);
      body.set('e' + i, row.end);
      body.set('w' + i, '');
    }

    for(let i=FIRST_EXTRA;i<=LAST_EXTRA;i++){
      body.set('en' + i, '0');
      body.set('s' + i, '00:00');
      body.set('e' + i, '00:00');
      body.set('w' + i, '');
    }

    const r = await smartRequest('/api/schedule', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body: body.toString()
    }, { allowHttpError:true });

    if (!r.ok) {
      const text = (await r.text()) || ('儲存失敗：HTTP ' + r.status);
      setStatus('err', text);
      return;
    }

    await loadSchedule();
    try{ await loadBoardSchedule(); }catch(e){}
    try{ await loadStatus(); }catch(e){}
    setStatus('ok', '已儲存');
  }

  timerToggleBtn.addEventListener('click', ()=>{ toggleTimerMode().catch(()=> setStatus('err', '切換失敗')); });
  document.getElementById('saveBtn').addEventListener('click', ()=>{ saveSchedule().catch(e=>setStatus('err', '儲存失敗：' + (e && e.message ? e.message : String(e)))); });

  if (devTapBtn && devBtn) {
    devTapBtn.addEventListener('click', ()=>{
      devTapCount++;
      clearTimeout(devTapTimer);
      devTapTimer = setTimeout(()=>{ devTapCount = 0; }, 1200);
      if (devTapCount >= 3) {
        devTapCount = 0;
        devTapBtn.style.display = 'none';
        devBtn.classList.add('show');
        setStatus('ok', '已顯示開發入口');
      }
    });
    devBtn.addEventListener('click', ()=>{
      location.href = '/est.html?_=' + Date.now();
    });
  }

  document.getElementById('syncBtn').addEventListener('click', async ()=>{
    try{
      setStatus('busy', '校正機板時間…');
      const ret = await syncTimeFromPhone();
      const target = (ret && ret.target) ? ret.target : null;
      let matched = false;
      for (let i = 0; i < 8; i++) {
        const j = await loadBoardSchedule();
        const now = (j && j.boardTime) ? String(j.boardTime) : '';
        if (target && now === target) { matched = true; break; }
        await new Promise(r => setTimeout(r, 500));
      }
      setStatus(matched ? 'ok' : 'err', matched ? '機板時間已校正' : '設備端時間未更新');
    }catch(e){
      setStatus('err', '機板時間校正失敗');
    }
  });


  async function init(){
    buildUi();
    setStatus('busy', '讀取中…');
    await loadSchedule();
    try{ await loadBoardSchedule(); }catch(e){}
    try{ await loadStatus(); }catch(e){ updateTimerToggle(); }
    setStatus('ok', '已讀取');
  }

  init().catch(e=>{
    setStatus('err', '載入失敗：' + (e && e.message ? e.message : String(e)));
  });
})();
</script>
</body>
</html>
)rawliteral";

inline String pageScheduleHtml(){
  return FPSTR(SCHEDULE_HTML);
}

#endif
