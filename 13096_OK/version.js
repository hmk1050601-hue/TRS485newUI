(async function(global){
  // ── 版本變更記錄 ──────────────────────────────────────
  // v13096: 首頁導覽改為快取優先，避免 AP 進入時先顯示無法連線
  // v13095: AP 切換時優先使用當前主機，並避免遙控器背景輪詢重疊
  // v13094: 遙控器頁面移除連線狀態與連線內容顯示
  // v13093: fix AP display: parallel AP probe to detect AP+STA mode
  // v13092: SW fix: skip no-store requests to prevent stale cache on AP mode
  // v13091: 連線狀態顯示修正：產品中/家用網路中/離線中
  // v13090: 自動更新版次機制
  // v13089: 移除前端 CHANGELOG.json 讀取，改為程式碼內記錄
  // v13087: 連線狀態改為：連線產品中 / 連線家用網路 / 離線中
  // v13086: 增加自動版本遞增腳本（bump-version.ps1）
  // v13085: 修復版本號顯示邏輯（async timing）
  // v13084: AP 連線修復；版本號動態顯示；Service Worker 動態快取
  // ──────────────────────────────────────────────────────

  const fallbackName = 'HMK';
  let appName = fallbackName;
  let version = '13096';

  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), { cache:'no-store' });
    const m = await r.json();
    appName = String(m.name || fallbackName).trim() || fallbackName;
    const match = appName.match(/(\d+)/);
    version = match ? match[1] : '13096';
    console.log('Version loaded:', {appName, version});
  }catch(e){
    appName = fallbackName;
    version = '13096';
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