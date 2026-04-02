;(function(){
  if (window.__HMK_SIM_MOCK_API__) return;
  window.__HMK_SIM_MOCK_API__ = true;

  const STORE_KEY = 'hmk_sim_state_v1';
  const AP_HOST = '192.168.4.1';
  const KNOWN_PATHS = new Set([
    '/status',
    '/ping',
    '/scan',
    '/savewifi',
    '/settemp',
    '/poweron',
    '/shutdown',
    '/super',
    '/sterilize',
    '/timermode',
    '/setappass',
    '/disableappass',
    '/factoryreset',
    '/api/myip',
    '/api/time',
    '/api/schedule',
    '/api/boardsched',
    '/api/powerlog',
    '/api/devstatus',
    '/api/hhreset'
  ]);

  function nowYm(){
    const d = new Date();
    return (d.getFullYear() * 12) + d.getMonth();
  }

  function daysInMonthByYm(ym){
    const y = Math.floor(ym / 12);
    const m0 = ym % 12;
    return new Date(y, m0 + 1, 0).getDate();
  }

  function buildDefaultPowerlog(){
    const endYm = nowYm();
    const baseYM = endYm - 11;
    const months = [];
    const daysByYM = {};

    for(let i = 0; i < 12; i++){
      const ym = baseYM + i;
      const days = daysInMonthByYm(ym);
      const dayArr = [];
      let total = 0;
      for(let d = 0; d < days; d++){
        const sec = 3600 + Math.floor(Math.random() * 9000);
        dayArr.push(sec);
        total += sec;
      }
      daysByYM[String(ym)] = dayArr;
      months.push(total);
    }

    const now = new Date();
    return {
      ok: true,
      validDate: true,
      y: now.getFullYear(),
      mo: now.getMonth() + 1,
      baseYM,
      months,
      daysByYM
    };
  }

  function buildDefaultState(){
    return {
      status: {
        m: 1,
        heat: 1,
        heatValid: 1,
        tSet: 55,
        tUp: 51
      },
      schedule: {
        items: [
          { enabled: false, start: '23:00', end: '00:00' },
          { enabled: true,  start: '06:30', end: '08:30' },
          { enabled: false, start: '12:00', end: '13:00' },
          { enabled: true,  start: '18:00', end: '22:00' }
        ]
      },
      powerlog: buildDefaultPowerlog(),
      devstatus: {
        raw: 0,
        hhStdSec: 100,
        hhLastSec: 86,
        hhCount: 10,
        hhPct: 86
      },
      wifi: {
        ssid: 'Office-Guest',
        pass: ''
      },
      apPasswordEnabled: false,
      apPassword: ''
    };
  }

  function loadState(){
    try{
      const raw = localStorage.getItem(STORE_KEY);
      if(!raw) return buildDefaultState();
      const parsed = JSON.parse(raw);
      if(!parsed || typeof parsed !== 'object') return buildDefaultState();
      return Object.assign(buildDefaultState(), parsed);
    }catch(e){
      return buildDefaultState();
    }
  }

  function saveState(st){
    try{ localStorage.setItem(STORE_KEY, JSON.stringify(st)); }catch(e){}
  }

  function resetSimState(){
    const fresh = buildDefaultState();
    saveState(fresh);
    return fresh;
  }

  function jsonResponse(payload, status){
    return new Response(JSON.stringify(payload), {
      status: status || 200,
      headers: { 'Content-Type': 'application/json; charset=utf-8' }
    });
  }

  function textResponse(text, status){
    return new Response(String(text || ''), {
      status: status || 200,
      headers: { 'Content-Type': 'text/plain; charset=utf-8' }
    });
  }

  function normalizePath(urlObj){
    return String(urlObj.pathname || '/').replace(/\/+$/, '') || '/';
  }

  function shouldMock(urlObj){
    const path = normalizePath(urlObj);
    if (KNOWN_PATHS.has(path)) return true;
    if (path === '/api/powerlog' || path === '/api/devstatus' || path === '/api/schedule') return true;
    return false;
  }

  function parseBodyParams(input, init){
    const out = new URLSearchParams();
    const body = init && init.body;

    if (typeof body === 'string'){
      return new URLSearchParams(body);
    }
    if (body instanceof URLSearchParams){
      return body;
    }
    if (typeof FormData !== 'undefined' && body instanceof FormData){
      body.forEach((v, k) => out.set(k, String(v)));
      return out;
    }
    if (input && typeof Request !== 'undefined' && input instanceof Request){
      const ctype = input.headers && input.headers.get ? (input.headers.get('content-type') || '') : '';
      if (/application\/x-www-form-urlencoded/i.test(ctype)){
        // Fallback path, mostly not used in this project.
        return out;
      }
    }
    return out;
  }

  function stepTemperature(st){
    const s = st.status;
    const mode = Number(s.m || 0);
    const target = mode === 0 ? 38 : Number(s.tSet || 55);
    const current = Number(s.tUp || 40);
    const drift = (target - current) * 0.22;
    const jitter = (Math.random() - 0.5) * 0.6;
    const next = Math.max(20, Math.min(85, current + drift + jitter));
    s.tUp = Number(next.toFixed(1));
    s.heat = (mode !== 0 && s.tUp < (target - 0.4)) ? 1 : 0;
    s.heatValid = 1;
  }

  function parseSetTemp(params){
    const t = Number(params.get('t'));
    if(!Number.isFinite(t)) return null;
    return Math.max(40, Math.min(70, Math.round(t)));
  }

  function scheduleFromParams(params, st){
    const items = [];
    for(let i = 0; i <= 3; i++){
      items.push({
        enabled: String(params.get('en' + i) || '0') === '1',
        start: String(params.get('s' + i) || (i === 0 ? '23:00' : '00:00')),
        end: String(params.get('e' + i) || '00:00')
      });
    }
    st.schedule.items = items;
  }

  function zeroPowerlog(st){
    const g = st.powerlog;
    g.months = g.months.map(() => 0);
    const keys = Object.keys(g.daysByYM || {});
    keys.forEach((k) => {
      const arr = g.daysByYM[k] || [];
      g.daysByYM[k] = arr.map(() => 0);
    });
  }

  function resetFactory(st){
    const fresh = buildDefaultState();
    Object.keys(st).forEach((k) => delete st[k]);
    Object.assign(st, fresh);
  }

  function methodOf(input, init){
    if (init && init.method) return String(init.method).toUpperCase();
    if (input && typeof Request !== 'undefined' && input instanceof Request) return String(input.method || 'GET').toUpperCase();
    return 'GET';
  }

  function toUrl(input){
    if (input && typeof Request !== 'undefined' && input instanceof Request) return new URL(input.url, location.href);
    return new URL(String(input), location.href);
  }

  function handleMock(input, init){
    const urlObj = toUrl(input);
    if (!shouldMock(urlObj)) return null;

    const path = normalizePath(urlObj);
    const method = methodOf(input, init);
    const params = parseBodyParams(input, init);
    const st = loadState();

    if (path === '/ping') return textResponse('pong');

    if (path === '/api/myip') return jsonResponse({ ok: true, ip: '192.168.50.88' });

    if (path === '/status'){
      stepTemperature(st);
      saveState(st);
      return jsonResponse(st.status);
    }

    if (path === '/poweron' && method === 'POST'){
      st.status.m = 1;
      st.status.heat = 1;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/shutdown' && method === 'POST'){
      st.status.m = 0;
      st.status.heat = 0;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/timermode' && method === 'POST'){
      st.status.m = 2;
      st.status.heat = 1;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/super' && method === 'POST'){
      st.status.m = 1;
      st.status.tSet = 70;
      st.status.heat = 1;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/sterilize' && method === 'POST'){
      st.status.m = 1;
      st.status.tSet = 65;
      st.status.heat = 1;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/settemp' && method === 'POST'){
      const t = parseSetTemp(params);
      if (t == null) return textResponse('Bad Request', 400);
      st.status.tSet = t;
      if (Number(st.status.m) !== 0) st.status.heat = 1;
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/api/schedule' && method === 'GET'){
      return jsonResponse({ items: st.schedule.items });
    }

    if (path === '/api/schedule' && method === 'POST'){
      scheduleFromParams(params, st);
      saveState(st);
      return jsonResponse({ ok: true });
    }

    if (path === '/api/boardsched'){
      return jsonResponse({
        valid: true,
        items: [
          st.schedule.items[1] || { start: '00:00', end: '00:00' },
          st.schedule.items[2] || { start: '00:00', end: '00:00' },
          st.schedule.items[3] || { start: '00:00', end: '00:00' }
        ]
      });
    }

    if (path === '/api/powerlog' && method === 'GET'){
      return jsonResponse(st.powerlog);
    }

    if (path === '/api/powerlog' && method === 'POST'){
      if (String(params.get('reset') || '') === 'all'){
        zeroPowerlog(st);
        saveState(st);
      }
      return jsonResponse({ ok: true });
    }

    if (path === '/api/devstatus' && method === 'GET'){
      const hhStdSec = Number(st.devstatus.hhStdSec || 100);
      const hhLastSec = Number(st.devstatus.hhLastSec || 86);
      const hhPct = hhStdSec > 0 ? Math.max(0, Math.min(100, Math.round((hhLastSec / hhStdSec) * 100))) : Number(st.devstatus.hhPct || 0);
      st.devstatus.hhPct = hhPct;
      saveState(st);
      return jsonResponse({
        valid: true,
        raw: Number(st.devstatus.raw || 0),
        hhStdSec,
        hhLastSec,
        hhCount: Number(st.devstatus.hhCount || 10),
        hhPct
      });
    }

    if (path === '/api/hhreset' && method === 'POST'){
      st.devstatus.hhCount = 0;
      st.devstatus.hhStdSec = 0;
      st.devstatus.hhLastSec = 0;
      st.devstatus.hhPct = 0;
      saveState(st);
      return jsonResponse({ ok: true });
    }

    if (path === '/scan' && method === 'GET'){
      return new Response(
        '<option value="Office-Guest">Office-Guest</option>' +
        '<option value="Home-WiFi-2.4G">Home-WiFi-2.4G</option>' +
        '<option value="TestLab">TestLab</option>',
        { status: 200, headers: { 'Content-Type': 'text/html; charset=utf-8' } }
      );
    }

    if (path === '/savewifi' && method === 'POST'){
      st.wifi.ssid = String(params.get('ssid') || st.wifi.ssid || 'Office-Guest');
      st.wifi.pass = String(params.get('pass') || '');
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/api/time' && method === 'POST'){
      return jsonResponse({ ok: true });
    }

    if (path === '/setappass' && method === 'POST'){
      st.apPasswordEnabled = true;
      st.apPassword = String(params.get('pass') || '');
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/disableappass' && method === 'POST'){
      st.apPasswordEnabled = false;
      st.apPassword = '';
      saveState(st);
      return textResponse('OK');
    }

    if (path === '/factoryreset' && method === 'POST'){
      resetFactory(st);
      saveState(st);
      return textResponse('OK');
    }

    return null;
  }

  const nativeFetch = window.fetch.bind(window);
  window.fetch = function(input, init){
    try{
      const mocked = handleMock(input, init);
      if (mocked) return Promise.resolve(mocked);
    }catch(e){
      // If mock routing fails for any reason, fall back to native fetch.
    }
    return nativeFetch(input, init);
  };

  window.HMK_SIM = window.HMK_SIM || {};
  window.HMK_SIM.reset = function(){
    return resetSimState();
  };
})();