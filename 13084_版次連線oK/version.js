(async function(global){
  const fallbackName = 'HMK';
  let appName = fallbackName;
  let version = '13084';
  
  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), { cache:'no-store' });
    const m = await r.json();
    appName = String(m.name || fallbackName).trim() || fallbackName;
    const match = appName.match(/(\d+)/);
    version = match ? match[1] : '13084';
    console.log('Version loaded:', {appName, version});
  }catch(e){
    appName = fallbackName;
    version = '13084';
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
