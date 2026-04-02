#ifndef POWERLOG_PAGE_H
#define POWERLOG_PAGE_H

#include <pgmspace.h>

// 用電記錄頁（2 年 / 日-月-季-年 + 上年度同期比較）
const char POWERLOG_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HMK 用電記錄</title>
<style>
:root{
  --bg:#f7f7f9;
  --card:#fff;
  --txt:#111;
  --muted:#666;
  --blue:#0b5bd3;
  --green:#1a8f3a;
  --red:#c63737;
  --border:#e8e8ee;
  --shadow:0 6px 16px rgba(0,0,0,.08);
}
*{box-sizing:border-box;}
body{
  margin:0;
  font-family: system-ui,-apple-system,"Segoe UI",Roboto,"Noto Sans TC","PingFang TC","Microsoft JhengHei",sans-serif;
  background:var(--bg);
  color:var(--txt);
}
.header{
  display:flex;
  justify-content:space-between;
  align-items:flex-start;
  padding:16px 18px 8px;
  max-width:720px;
  margin:0 auto;
}
.header h2{
  margin:0;
  font-size:clamp(18px,4.8vw,22px);
  letter-spacing:.5px;
}
.headerBtns{
  display:flex;
  flex-direction:row;
  gap:8px;
  align-items:flex-end;
}
.header button{
  border:none;
  background:#222;
  color:#fff;
  padding:10px 14px;
  border-radius:12px;
  cursor:pointer;
  font-size:14px;
}
.header button:active{ opacity:.85; }

.main{
  max-width:720px;
  margin:0 auto;
  padding:0 14px 18px;
}
.card{
  background:var(--card);
  border:1px solid var(--border);
  border-radius:16px;
  box-shadow:var(--shadow);
  padding:14px 16px;
  margin-top:10px;
}

.ctrlGrid{
  display:grid;
  grid-template-columns:1fr 1fr;
  gap:10px;
  align-items:center;
}
.ctrlItem{
  display:flex;
  align-items:center;
  gap:6px;
  min-width:0;
}
label{
  font-size:13px;
  color:var(--muted);
  font-weight:800;
  white-space:nowrap;
}
select, input[type="number"]{
  width:100%;
  max-width:140px;     /* 預設窄 */
  padding:8px 10px;
  border-radius:12px;
  border:1px solid var(--border);
  background:#fff;
  font-size:14px;
  min-width:0;
}

/* 期間選單加寬 30%（避免卡字） */
#periodSel{ max-width:185px; }

@media (max-width:360px){
  select, input[type="number"]{ max-width:120px; font-size:13px; padding:7px 9px; }
  #periodSel{ max-width:165px; }
}

.kpis{
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:10px;
  margin-top:10px;
}
.kpi{
  background:#fff;
  border:1px solid var(--border);
  border-radius:16px;
  padding:10px 12px;
  min-width:0;
}
.kpi .t{ font-size:13px; color:var(--muted); font-weight:900; }
.lines{ margin-top:8px; display:flex; flex-direction:column; gap:6px; }
.line{ display:flex; justify-content:space-between; gap:8px; }
.lab{ font-size:12px; color:var(--muted); font-weight:900; }
.val{ font-size:15px; font-weight:900; white-space:nowrap; }
.pos{ color:var(--green); }
.neg{ color:var(--red); }

.tableWrap{ overflow:auto; }
table{
  width:100%;
  border-collapse:collapse;
  font-size:14px;
}
th,td{
  text-align:left;
  padding:10px 8px;
  border-bottom:1px solid var(--border);
  vertical-align:top;
  white-space:nowrap;
}
th{ color:var(--muted); font-size:13px; }

.mcell{
  display:flex;
  flex-direction:column;
  gap:4px;
}
.mline{
  font-weight:900;
  line-height:1.25;
}

.badge{
  display:inline-block;
  padding:2px 8px;
  border-radius:999px;
  font-size:12px;
  font-weight:900;
  border:1px solid var(--border);
  background:#fff;
}
.badge.ok{ color:var(--green); }
.badge.err{ color:var(--red); }

.actions{
  display:flex;
  gap:8px;
  flex-wrap:wrap;
  margin-top:10px;
}
.actions button{
  border:none;
  border-radius:12px;
  padding:10px 12px;
  cursor:pointer;
  font-weight:900;
  font-size:14px;
}
.actions .gray{ background:#777; color:#fff; }
.actions .blue{ background:var(--blue); color:#fff; }
.actions .red{ background:var(--red); color:#fff; }
.actions button:active{ opacity:.85; }
</style>
</head>
<body>
<div class="header">
  <h2>用電記錄</h2>
  <div class="headerBtns">
    <button type="button" id="statusBtn">返回設備狀態</button>
  </div>
</div>

<div class="main">

  <div class="card">
    <div class="ctrlGrid">
      <div class="ctrlItem">
        <label>模式</label>
<select id="modeSel">
  <option value="day" selected>日</option>
  <option value="month">月</option>
 
  <option value="year">年</option>
</select>

      </div>

      <div class="ctrlItem">
        <label>期間</label>
        <select id="periodSel"></select>
      </div>

      <div class="ctrlItem">
        <label>功率</label>
        <select id="pwrSel">
          <option value="4">4 kW</option>
          <option value="6">6 kW</option>
          <option value="custom">自訂</option>
        </select>
      </div>

      <div class="ctrlItem">
        <label>電費(元/度)</label>
        <input id="rateInp" type="number" step="0.01" min="0" inputmode="decimal" value="1.68">
      </div>
    </div>

    <div class="ctrlItem" style="margin-top:10px; display:none" id="pwrCustomWrap">
      <label>自訂</label>
      <input id="pwrCustom" type="number" step="0.1" min="0.1" inputmode="decimal" placeholder="kW">
    </div>

    <div class="kpis">
      <div class="kpi">
        <div class="t" id="k1t">本期</div>
        <div class="lines">
          <div class="line"><span class="lab"></span><span class="val" id="k1h">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k1k">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k1c">--</span></div>
        </div>
      </div>

      <div class="kpi">
        <div class="t" id="k2t">去年同期</div>
        <div class="lines">
          <div class="line"><span class="lab"></span><span class="val" id="k2h">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k2k">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k2c">--</span></div>
        </div>
      </div>

      <div class="kpi">
        <div class="t">差異</div>
        <div class="lines">
          <div class="line"><span class="lab"></span><span class="val" id="k3h">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k3k">--</span></div>
          <div class="line"><span class="lab"></span><span class="val" id="k3c">--</span></div>
        </div>
      </div>
    </div>

    <div class="actions">
      <button class="blue" type="button" id="reloadBtn">刷新</button>
      <button class="gray" type="button" id="resetTodayBtn">清除今日</button>
      <button class="gray" type="button" id="resetMonthBtn">清除當月</button>
      <button class="red" type="button" id="resetAllBtn">清除全部</button>
    </div>
  </div>

  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap">
      <div><span class="badge" id="dateBadge">日期未校正</span></div>

      <div class="ctrlItem">
        <label>清單</label>
        <select id="unitSel">
          <option value="h" selected>小時</option>
          <option value="k">度</option>
          <option value="c">元</option>
        </select>
      </div>

      <div style="font-size:13px;color:var(--muted);font-weight:800" id="metaText">--</div>
    </div>

    <div class="tableWrap" style="margin-top:10px">
      <table>
        <thead>
          <tr>
            <th>期間</th>
            <th>本期</th>
            <th>去年同期</th>
            <th>差異</th>
          </tr>
        </thead>
        <tbody id="tbody"></tbody>
      </table>
    </div>
  </div>

</div>

<script>
(async function(){
  let devIp = localStorage.getItem('hmk_ip') || '';

function normalizeHost(s){
  if(!s) return '';
  s = (''+s).trim();
  s = s.replace(/^https?:\/\//i,'');
  s = s.replace(/\/.*$/,'');
  return s;
}

function candidateBases(){
  const bases = [];
  const add = (b)=>{ if(b && !bases.includes(b)) bases.push(b); };

  // 1) 永遠先試 AP（但等一下 smartFetch 會給它很短 timeout）
  add('http://192.168.4.1');

  // 2) 再試 localStorage 記住的 IP（只存 host）
  devIp = normalizeHost(devIp);
  if (devIp) add('http://' + devIp);

  // 3) 最後補目前頁面 host（保底）
  try{
    const host = location.hostname;
    if (host && host.match(/^(\d+\.){3}\d+$/)){
      add('http://' + host);
    }
  }catch(e){}

  return bases;
}

async function fetchWithTimeout(url, options, ms){
  const ctrl = new AbortController();
  const t = setTimeout(()=>ctrl.abort(), ms);
  try{
    return await fetch(url, {...(options||{}), signal: ctrl.signal});
  } finally {
    clearTimeout(t);
  }
}

async function smartFetch(path, options){
  const bases = candidateBases();
  let lastErr;

  for (const b of bases){
    const url = b + path;
    const ms = (b === 'http://192.168.4.1') ? 700 : 2500; // ✅ AP 很快放棄，IP 給久一點
    try{
      const res = await fetchWithTimeout(url, options || {}, ms);
      if (!res.ok){
        lastErr = new Error('HTTP ' + res.status);
        continue;
      }

      // ✅ 成功且不是 AP → 記住 host（只存 host）
      try{
        const u = new URL(url);
        const host = u.hostname;
        if (host && host !== '192.168.4.1'){
          devIp = host;
          localStorage.setItem('hmk_ip', devIp);
        }
      }catch(e){}

      return res;
    }catch(e){
      lastErr = e;
    }
  }
  throw lastErr || new Error('fetch failed');
}


  async function syncTimeFromPhone(){
    try{
      const d = new Date();
      const y  = d.getFullYear();
      const mo = d.getMonth() + 1;
      const da = d.getDate();
      const hh = d.getHours();
      const mm = d.getMinutes();
      let wd = d.getDay();
      wd = (wd === 0) ? 7 : wd;
      await smartFetch(`/api/time?y=${y}&mo=${mo}&d=${da}&h=${hh}&m=${mm}&wd=${wd}`, { method:'POST', cache:'no-store' });
    }catch(e){}
  }

  const modeSel   = document.getElementById('modeSel');
  const periodSel = document.getElementById('periodSel');
  const tbody     = document.getElementById('tbody');

  const pwrSel = document.getElementById('pwrSel');
  const pwrCustomWrap = document.getElementById('pwrCustomWrap');
  const pwrCustom = document.getElementById('pwrCustom');
  const rateInp = document.getElementById('rateInp');

  const unitSel = document.getElementById('unitSel');

  const k1t = document.getElementById('k1t');
  const k2t = document.getElementById('k2t');
  const k1h = document.getElementById('k1h');
  const k1k = document.getElementById('k1k');
  const k1c = document.getElementById('k1c');
  const k2h = document.getElementById('k2h');
  const k2k = document.getElementById('k2k');
  const k2c = document.getElementById('k2c');
  const k3h = document.getElementById('k3h');
  const k3k = document.getElementById('k3k');
  const k3c = document.getElementById('k3c');

  const dateBadge = document.getElementById('dateBadge');
  const metaText  = document.getElementById('metaText');

  const reloadBtn     = document.getElementById('reloadBtn');
  const resetTodayBtn = document.getElementById('resetTodayBtn');
  const resetMonthBtn = document.getElementById('resetMonthBtn');
  const resetAllBtn   = document.getElementById('resetAllBtn');

  const statusBtn = document.getElementById('statusBtn');
  if (statusBtn) statusBtn.onclick = ()=>{ location.href='/status.html?_=' + Date.now(); };

  const K_MODE = 'pl_mode';
  const K_PERIOD_DAY = 'pl_period_day';
  const K_PERIOD_MONTH = 'pl_period_month';
  
  const K_PERIOD_YEAR = 'pl_period_year';
  const K_PWR = 'pl_pwr';
  const K_PWR_CUSTOM = 'pl_pwr_custom';
  const K_RATE = 'pl_rate';
  const K_UNIT = 'pl_unit';

  function getPeriodKey(mode){
    if (mode==='day') return K_PERIOD_DAY;
    if (mode==='month') return K_PERIOD_MONTH;
    
    return K_PERIOD_YEAR;
  }

  function pad2(n){ return String(n).padStart(2,'0'); }

  function ymToObj(ym){
    const y = Math.floor(ym / 12);
    const m = (ym % 12) + 1;
    return {y, m};
  }
  function fmtYM(ym){
    const o = ymToObj(ym);
    return `${o.y}-${pad2(o.m)}`;
  }
  function fmtN(x){
    const n = Number(x);
    if (!isFinite(n)) return '--';
    const a = Math.abs(n);
    if (a < 10) return n.toFixed(2);
    if (a < 100) return n.toFixed(1);
    return n.toFixed(0);
  }

  function getPowerKw(){
    const v = pwrSel.value;
    if (v === 'custom'){
      const x = parseFloat(pwrCustom.value);
      return (isFinite(x) && x>0) ? x : 4;
    }
    const k = parseFloat(v);
    return (isFinite(k) && k>0) ? k : 4;
  }
  function getRate(){
    const r = parseFloat(rateInp.value);
    return (isFinite(r) && r>=0) ? r : 1.68;
  }

  function getListUnit(){
    return (unitSel && unitSel.value) ? unitSel.value : 'h';
  }

  function updatePowerUI(){
    pwrCustomWrap.style.display = (pwrSel.value === 'custom') ? 'flex' : 'none';
  }

  function savePrefs(){
    localStorage.setItem(K_MODE, modeSel.value);
    localStorage.setItem(getPeriodKey(modeSel.value), periodSel.value || '');
    localStorage.setItem(K_PWR, pwrSel.value);
    localStorage.setItem(K_PWR_CUSTOM, pwrCustom.value || '');
    localStorage.setItem(K_RATE, rateInp.value || '');
    if (unitSel) localStorage.setItem(K_UNIT, unitSel.value || 'h');
  }

  function loadPrefs(){
    //const m = localStorage.getItem(K_MODE);
    //if (m) modeSel.value = m;

    const pw = localStorage.getItem(K_PWR);
    if (pw) pwrSel.value = pw;

    const pc = localStorage.getItem(K_PWR_CUSTOM);
    if (pc) pwrCustom.value = pc;

    const rt = localStorage.getItem(K_RATE);
    if (rt) rateInp.value = rt;

    const un = localStorage.getItem(K_UNIT);
    if (unitSel && un) unitSel.value = un;

    updatePowerUI();
  }

  let g = null;
  let lastMode = '';
  let firstRender = true;


  async function loadData(){
     const res = await smartFetch('/api/powerlog', {cache:'no-store'});
     g = await res.json();
     if (!g || !g.ok) throw new Error('bad json');

     // ✅ 相容不同韌體欄位名：把 monthSec 對齊成 months（季/月/日模式都吃 months）
     if (!Array.isArray(g.months)){
      if (Array.isArray(g.monthSec)) g.months = g.monthSec;
      else if (Array.isArray(g.monthSecs)) g.months = g.monthSecs;
     }
  }


  function secToH(sec){ return (Number(sec)||0) / 3600; }

  function metricsFromSec(sec){
    const h = secToH(sec);
    const kwh = h * getPowerKw();
    const cost = kwh * getRate();
    return {h, kwh, cost};
  }

  function diffPct(curSec, prevSec){
    const c = Number(curSec)||0;
    const p = Number(prevSec)||0;
    const d = c - p;
    const pct = (p > 0) ? (d / p * 100) : null;
    return {d, pct};
  }

  // ------------------------------------------------------------
  // 取日資料（支援多種後端欄位命名）
  // 期望格式之一：
  //  - g.daysByMonth["2025-12"] = [secDay1, secDay2, ...]
  //  - g.daysByYM["24311"] = [...]
  //  - g.days (object) { "2025-12": [...] } 或 { "24311": [...] }
  // ------------------------------------------------------------
  function ymKeyStr(ym){
    const o = ymToObj(ym);
    return `${o.y}-${pad2(o.m)}`;
  }

  function getDaysArrayForYm(ym){
    if (!g) return null;
    const kNum = String(ym);
    const kStr = ymKeyStr(ym);

    if (g.daysByYM && g.daysByYM[kNum]) return g.daysByYM[kNum];
    if (g.daysByYm && g.daysByYm[kNum]) return g.daysByYm[kNum];

    if (g.daysByMonth && g.daysByMonth[kStr]) return g.daysByMonth[kStr];
    if (g.daysMap && g.daysMap[kStr]) return g.daysMap[kStr];

    if (Array.isArray(g.days) && Number(g.daysYM) === ym) return g.days;

    if (g.days && typeof g.days === 'object'){
      if (g.days[kNum]) return g.days[kNum];
      if (g.days[kStr]) return g.days[kStr];
    }
    return null;
  }

  function buildDaysFromToday(ym, daysInMonth){
    if (!g || !g.validDate) return null;
    const curYM = (Number(g.y)||0) * 12 + ((Number(g.mo)||1) - 1);
    if (ym !== curYM) return null;
    const arr = new Array(daysInMonth).fill(0);
    const di = Math.max(0, Math.min(daysInMonth - 1, (Number(g.d)||1) - 1));
    arr[di] = Number(g.todaySec||0);
    return arr;
  }


  function getDayModeYM(){
    const months = (g && g.months) ? g.months : [];
    const baseYM = g.baseYM || 0;

    // 有日期就用「現在這個月」
    if (g && g.validDate){
      const y = Number(g.y)||0;
      const mo = Number(g.mo)||1;
      return y * 12 + (mo - 1);
    }

    // 沒日期：退而求其次用「資料最後一個月」
    if (months.length) return baseYM + months.length - 1;
    return baseYM;
  }

  function daysInMonthFromYM(ym){
    const o = ymToObj(ym);
    return new Date(o.y, o.m, 0).getDate(); // o.m=1..12 OK
  }
  // ------------------------------------------------------------
  // Data periods
  // ------------------------------------------------------------
  function buildPeriods(mode){
    const items = [];
    const months = (g && g.months) ? g.months : [];
    const baseYM = g.baseYM || 0;

 // ✅ 日模式：期間改為「月份（近 24 個月）」，清單用 renderTableDaysForMonth(ym) 來畫
if (mode === 'day'){
  // 以資料/日期推算「最新可選月份」
  let endYM = getDayModeYM();
  if (months.length) endYM = Math.min(endYM, baseYM + months.length - 1);

  // 近 24 個月（2 年）
  let startYM = endYM - 23;
  if (startYM < baseYM) startYM = baseYM;

  for (let ym = startYM; ym <= endYM; ym++){
    const idx = ym - baseYM;

    const sec = (idx >= 0 && idx < months.length) ? Number(months[idx] || 0) : 0;

    const prevIdx = idx - 12;
    const prevSec = (prevIdx >= 0 && prevIdx < months.length) ? Number(months[prevIdx] || 0) : null;

    items.push({
      id: String(ym),                 // ✅ 下拉選單 value = ym
      label: fmtYM(ym),               // ✅ 顯示 YYYY-MM
      sec,
      prevSec,
      prevLabel: (prevSec != null) ? fmtYM(ym - 12) : '--'
    });
  }
  return items;
}




    if (mode === 'month'){
      for (let i=0;i<months.length;i++){
        const ym = baseYM + i;
        const sec = Number(months[i]||0);
        const prevSec = (i>=12) ? Number(months[i-12]||0) : null;
        items.push({
          id: String(ym),
          label: fmtYM(ym),
          sec,
          prevSec,
          prevLabel: (i>=12) ? fmtYM(ym-12) : '--'
        });
      }
      return items;
    }



    if (mode === 'year'){
      const years = new Set();
      for (let i=0;i<months.length;i++){
        const ym = baseYM + i;
        years.add(ymToObj(ym).y);
      }
      const yearArr = Array.from(years).sort((a,b)=>a-b);

      let curY = null, curMo = null;
      if (g && g.validDate){
        curY = Number(g.y);
        curMo = Number(g.mo);
      } else if (months.length){
        const o = ymToObj(baseYM + months.length - 1);
        curY = o.y; curMo = o.m;
      }

      function sumYearRange(y, endMo){
        let sum = 0;
        for (let i=0;i<months.length;i++){
          const ym = baseYM + i;
          const o = ymToObj(ym);
          if (o.y !== y) continue;
          if (endMo && o.m > endMo) continue;
          sum += Number(months[i]||0);
        }
        return sum;
      }

      for (const y of yearArr){
        if (curY !== null && y < curY - 1) continue;

        const endMo = (y === curY) ? curMo : 12;
        if (y !== curY){
          let cnt = 0;
          for (let i=0;i<months.length;i++){
            const o = ymToObj(baseYM + i);
            if (o.y === y) cnt++;
          }
          if (cnt < 12) continue;
        }

        const sec = sumYearRange(y, endMo);
        const prevSec = sumYearRange(y-1, endMo);
        items.push({
          id: String(y) + '-' + endMo,
          label: (y === curY) ? `${y} 1~${pad2(endMo)} (今年)` : `${y} 全年`,
          sec,
          prevSec: prevSec,
          prevLabel: (y-1) + ((y===curY) ? ` 1~${pad2(endMo)}` : ' 全年')
        });
      }
      return items;
    }

    return items;
  }

   function fillPeriodOptions(mode, forceId){
  const items = buildPeriods(mode);

  // ✅ 顯示用：由近到遠（最新在最上）
  const view = items.slice().reverse();

  periodSel.innerHTML = '';
  for (const it of view){
    const opt = document.createElement('option');
    opt.value = it.id;
    opt.textContent = it.label;
    periodSel.appendChild(opt);
  }

  // ✅ 強制預設（用於：進頁面月=當月、切到日=當日）
  const fid = (forceId != null) ? String(forceId) : '';
  if (fid && view.some(x => x.id === fid)){
    periodSel.value = fid;
    return items;
  }

  // ✅ 其餘模式/情況才吃記憶
  const saved = localStorage.getItem(getPeriodKey(mode));
  if (saved && view.some(x=>x.id===saved)){
    periodSel.value = saved;
    return items;
  }

  if (view.length){
    periodSel.value = view[0].id;
  }
  return items;
}



  function pickCurrent(items){
    const id = periodSel.value;
    return items.find(x=>x.id===id) || (items.length ? items[items.length-1] : null);
  }

  function renderMeta(){
    if (g && g.validDate){
      dateBadge.textContent = `日期已校正：${g.y}-${pad2(g.mo)}-${pad2(g.d)}`;
      dateBadge.className = 'badge ok';
    } else {
      dateBadge.textContent = '日期未校正';
      dateBadge.className = 'badge err';
    }
    const today = metricsFromSec(g ? (g.todaySec||0) : 0);
    const total = metricsFromSec(g ? (g.totalSec||0) : 0);
    metaText.textContent = `今日：${fmtN(today.h)} 小時 / ${fmtN(today.kwh)} 度 / ${fmtN(today.cost)} 元｜累計：${fmtN(total.h)} 小時`;
  }

  function setKPI(item, mode){
  if (!item) return;

  let curSec = Number(item.sec||0);
  let prevSec = (item.prevSec != null) ? Number(item.prevSec||0) : null;

  if (mode === 'day'){
    // ✅ 日模式：本期改用「今日」(todaySec)
    k1t.textContent = '本日';
    k2t.textContent = '去年同日';

    const ymSel = parseInt(periodSel.value, 10);
    const curYM = (g && g.validDate)
      ? ((Number(g.y)||0) * 12 + ((Number(g.mo)||1) - 1))
      : getDayModeYM();

    if (g && g.validDate && isFinite(ymSel) && ymSel === curYM){
      curSec = Number(g.todaySec || 0);

      // 去年同日（若有 days array 就用，沒有就顯示 --）
      const daysPrev = getDaysArrayForYm(curYM - 12);
      const di = Math.max(0, (Number(g.d)||1) - 1);
      prevSec = (daysPrev && daysPrev.length) ? Number(daysPrev[di]||0) : null;
    } else {
      // 選到非本月：本日就顯示 0（你如果想顯示該月某一天，可再延伸）
      curSec = 0;
      prevSec = null;
    }
  } else {
    if (mode==='month') k1t.textContent = '當月';
    else if (mode==='quarter') k1t.textContent = '當季';
    else k1t.textContent = '當年';
    k2t.textContent = '去年同期';
  }

  const cur = metricsFromSec(curSec);
  const hasPrev = (prevSec != null);
  const prev = metricsFromSec(hasPrev ? prevSec : 0);

  k1h.textContent = `${fmtN(cur.h)} 小時`;
  k1k.textContent = `${fmtN(cur.kwh)} 度`;
  k1c.textContent = `${fmtN(cur.cost)} 元`;

  if (!hasPrev){
    k2h.textContent = '--';
    k2k.textContent = '--';
    k2c.textContent = '--';
    k3h.textContent = '--';
    k3k.innerHTML = '';
    k3c.innerHTML = '';
    return;
  }

  k2h.textContent = `${fmtN(prev.h)} 小時`;
  k2k.textContent = `${fmtN(prev.kwh)} 度`;
  k2c.textContent = `${fmtN(prev.cost)} 元`;

  const {pct} = diffPct(curSec, prevSec);
  const pctTxt = (pct==null) ? '--' : `${pct>0?'+':''}${pct.toFixed(1)}%`;
  const cls = (pct==null) ? '' : (pct>=0 ? 'pos' : 'neg');

  k3h.innerHTML = `<span class="${cls}">${pctTxt}</span>`;
  k3k.innerHTML = ``;
  k3c.innerHTML = ``;
}


  // ✅ 清單依「小時/度/元」篩選顯示（只顯示一個單位）
  function cellHtmlFromSec(sec){
    const m = metricsFromSec(sec||0);
    const u = getListUnit();
    let v = m.h, unitTxt = ' 小時';
    if (u === 'k'){ v = m.kwh; unitTxt = ' 度'; }
    if (u === 'c'){ v = m.cost; unitTxt = ' 元'; }

    return `
      <div class="mcell">
        <div class="mline">${fmtN(v)}${unitTxt}</div>
      </div>
    `;
  }

  // ✅ 所有差異改成「百分比」顯示
  function diffCellHtml(curSec, prevSec){
    if (prevSec == null) return '--';

    const {pct} = diffPct(curSec||0, prevSec||0);
    const pctTxt = (pct==null) ? '--' : `${pct>0?'+':''}${pct.toFixed(1)}%`;
    const cls = (pct==null) ? '' : (pct>=0 ? 'pos' : 'neg');

    return `
      <div class="mcell">
        <div class="mline ${cls}">${pctTxt}</div>
      </div>
    `;
  }

  function renderTableMonthLike(items){
    tbody.innerHTML = '';
    const arr = items.slice().reverse(); // 由近到遠（最新在最上）
    for (const it of arr){
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td>${it.label}</td>
        <td>${cellHtmlFromSec(it.sec||0)}</td>
        <td>${(it.prevSec==null) ? '--' : cellHtmlFromSec(it.prevSec||0)}</td>
        <td>${diffCellHtml(it.sec||0, it.prevSec)}</td>
      `;
      tbody.appendChild(tr);
    }
  }

  // ✅ 日模式清單：顯示當月 1~30/31 天（最新在最上）
  function renderTableDaysForMonth(ym){
    tbody.innerHTML = '';

    const o = ymToObj(ym);
    const daysInMonth = new Date(o.y, o.m, 0).getDate(); // o.m 是 1..12，OK

    let daysCur = getDaysArrayForYm(ym);
    if (!daysCur) daysCur = buildDaysFromToday(ym, daysInMonth);

    const daysPrev = getDaysArrayForYm(ym - 12);

    for (let d = daysInMonth; d >= 1; d--){
      const idx = d - 1;
      const curSec = daysCur ? Number(daysCur[idx]||0) : 0;
      const prevSec = daysPrev ? Number(daysPrev[idx]||0) : null;

      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td>${d}</td>
        <td>${cellHtmlFromSec(curSec)}</td>
        <td>${(prevSec==null) ? '--' : cellHtmlFromSec(prevSec)}</td>
        <td>${diffCellHtml(curSec, prevSec)}</td>
      `;
      tbody.appendChild(tr);
    }
  }

   async function render(){
  if (!g) return;

  renderMeta();

  const mode = modeSel.value;

  // ✅ 只在「第一次進頁」或「剛切換模式」時，強制用手機當月/當日當預設
function getCurrentYMo(){
  const months = (g && g.months) ? g.months : [];
  const baseYM = g.baseYM || 0;

  if (g && g.validDate){
    return {y:Number(g.y)||0, mo:Number(g.mo)||1};
  }
  if (months.length){
    const o = ymToObj(baseYM + months.length - 1);
    return {y:o.y, mo:o.m};
  }
  const d = new Date();
  return {y:d.getFullYear(), mo:d.getMonth()+1};
}

let forceId = null;
if (firstRender || mode !== lastMode){
  const cur = getCurrentYMo();

  if (mode === 'month' || mode === 'day'){
    const curYM = cur.y * 12 + (cur.mo - 1);
    forceId = String(curYM);
  } else if (mode === 'quarter'){
    const q = Math.floor((cur.mo - 1) / 3) + 1;
    forceId = `${cur.y}-Q${q}`;
  }
}


  const items = fillPeriodOptions(mode, forceId);
  const cur = pickCurrent(items);

  setKPI(cur, mode);

  // ✅ 清單一定要畫（你現在就是少了這行所以看不到）
 if (mode === 'day') {
    const ym = parseInt(periodSel.value, 10);
    renderTableDaysForMonth(isFinite(ym) ? ym : getDayModeYM());
  } else {
    renderTableMonthLike(items);
  }

  savePrefs();

  lastMode = mode;
  firstRender = false;
}



  async function postReset(type){
    const body = 'reset=' + encodeURIComponent(type);
    await smartFetch('/api/powerlog', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body
    });
  }

  modeSel.addEventListener('change', async ()=>{
    localStorage.setItem(K_MODE, modeSel.value);
    await render();
  });

  periodSel.addEventListener('change', ()=>{
    localStorage.setItem(getPeriodKey(modeSel.value), periodSel.value || '');
    render();
  });

  pwrSel.addEventListener('change', ()=>{
    updatePowerUI();
    savePrefs();
    render();
  });

  pwrCustom.addEventListener('input', ()=>{
    savePrefs();
    render();
  });

  rateInp.addEventListener('input', ()=>{
    savePrefs();
    render();
  });

  if (unitSel){
    unitSel.addEventListener('change', ()=>{
      savePrefs();
      render();
    });
  }

  reloadBtn.onclick = async ()=>{
    try{
      await syncTimeFromPhone();
      await loadData();
      await render();
    }catch(e){
      alert('刷新失敗：' + (e && e.message ? e.message : e));
    }
  };

  resetTodayBtn.onclick = async ()=>{
    if (!confirm('確定要清除「今日」用電記錄？')) return;
    try{
      await postReset('today');
      await loadData();
      await render();
    }catch(e){
      alert('清除失敗：' + (e && e.message ? e.message : e));
    }
  };

  resetMonthBtn.onclick = async ()=>{
    if (!confirm('確定要清除「當月」用電記錄？')) return;
    try{
      await postReset('month');
      await loadData();
      await render();
    }catch(e){
      alert('清除失敗：' + (e && e.message ? e.message : e));
    }
  };

  resetAllBtn.onclick = async ()=>{
    if (!confirm('確定要清除「全部」用電記錄？（會清空近兩年月資料與累計）')) return;
    try{
      await postReset('all');
      await loadData();
      await render();
    }catch(e){
      alert('清除失敗：' + (e && e.message ? e.message : e));
    }
  };

  // init
  loadPrefs();
  await syncTimeFromPhone();
  await loadData();

  //const savedMode = localStorage.getItem(K_MODE);
  //if (savedMode) modeSel.value = savedMode;

  updatePowerUI();
  await render();

})();
</script>
</body>
</html>
)rawliteral";

inline String pagePowerLogHtml(){
  return FPSTR(POWERLOG_HTML);
}

#endif
