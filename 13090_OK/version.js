(async function(global){
  // ── 版本變更記錄 ──────────────────────────────────────
  // v13090: 自動更新版次機制
  // v13089: 移除前端 CHANGELOG.json 讀取，改為程式碼內記錄
  // v13087: 連線狀態改為：連線產品中 / 連線家用網路 / 離線中
  // v13086: 增加自動版本遞增腳本（bump-version.ps1）
  // v13085: 修復版本號顯示邏輯（async timing）
  // v13084: AP 連線修復；版本號動態顯示；Service Worker 動態快取
  // ──────────────────────────────────────────────────────

  const fallbackName = 'HMK';
  let appName = fallbackName;
  let version = '13090';

  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), { cache:'no-store' });
    const m = await r.json();
    appName = String(m.name || fallbackName).trim() || fallbackName;
    const match = appName.match(/(\d+)/);
    version = match ? match[1] : '13090';
    console.log('Version loaded:', {appName, version});
  }catch(e){
    appName = fallbackName;
    version = '13090';
    console.warn('manifest.json load failed, using fallback', e);
  }

  // 設置全局變數
  global.HMK_APP_NAME = appName;
  global.HMK_VERSION = version;

  // 立即更新 DOM
  try{
    localStorage.setItem('hmk_pwa_name', appName);
    localStorage.setItem('hmk_pwa_version', version);
  }catch(e){}

  document.querySelectorAll('.hmk-version').forEach(e => e.textContent = version);
})(typeof window !== 'undefined' ? window : self);
