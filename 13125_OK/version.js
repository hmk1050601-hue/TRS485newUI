(async function(global){
  // ── 版本變更記錄 ──────────────────────────────────────
  // v13125: PWA 新增產品狀態頁，移除裝置名稱/連線狀態列並改為設備端處理電熱管校正
  // v13124: 註冊 SW 後主動執行 reg.update()，加速舊 SW 被新版本取代
  // v13123: 修正 SW 安裝容錯，避免預快取單檔失敗導致舊離線副本持續接管（AP 可用性修復）
  // v13122: AP 模式優先穩定連線：停用首頁自動還原導頁，並調整 SW 回退邏輯避免落入離線副本
  // v13121: 首頁圖中電費累計文案改為當月電費累計
  // v13120: 首頁電費累計改為快取預顯示與並行抓取，改善顯示延遲
  // v13119: sync loop 改用 readStatusQuiet 不碰全域狀態，避免輪詢中把樂觀更新的 currentMode 寫回舊值
  // v13118: 定時切換改為 sync-loop 同步迴圈（同 remote.html），狀態不符時自動重送指令最多 8 秒
  // v13117: shell 頁改為 network-first 避免 schedule 吃舊快取，並移除外層粗略切換失敗提示
  // v13116: 定時與儲存改為固定 base 的 no-cors 發送，並只向同一設備輪詢確認
  // v13115: 定時切換綁定到最後成功讀狀態的同一設備 base，修正第二次切換失效
  // v13114: 定時切換期間按鈕短暫 disable 並顯示切換中，避免連點造成競態
  // v13113: 修正定時切換後誤判失敗，改為樂觀更新 + 輪詢確認，避免把切換方向打回舊值
  // v13112: POST 改為 fire-and-forget + 讀狀態驗證（跟遙控器頁相同），避免 CORS 擋回應導致誤判失敗
  // v13111: 排程頁網路層改為與遙控器頁完全一致的 candidateBases + smartFetch 模式
  // v13110: 切換定時改為直接對已探測 base 發送，加大逾時至 3-5s，失敗時清除 base 並顯示具體錯誤
  // v13109: 修正定時只能啟動無法關閉，指令成功後樂觀更新 currentMode，避免狀態讀取失敗導致切換方向永遠錯誤
  // v13108: 修正第二次切換定時失敗，smartFetch 加入失敗重探測機制與加大逾時
  // v13107: 修正排程頁 API 連線失敗，改用探測式 base 解析，修 Arduino hTimerMode 缺 CORS
  // v13106: 首頁時間設定按鈕改為導向 PWA 本機 schedule.html（不再跳機板頁）
  // v13105: 排程頁加入 PWA 返回 header，主按鈕改為啟動中/關閉中，移除機板時間與校時
  // v13104: 新增 PWA schedule.html，內容同步目前時間設定頁
  // v13103: 修正首頁初次進入誤判 iframe 可見，恢復首頁數值讀取與輪詢
  // v13102: 修正首頁不再還原到時間設定頁，iframe 分頁改為回首頁啟動
  // v13101: 取消板頁容器模式，首頁恢復原本導頁，遙控器回到獨立頁，時間設定維持新版頁面
  // v13100: 排程頁改為 PWA 殼內的定時模式大按鈕、四段勾叉啟停與無星期時間編輯
  // v13099: AP 改為板頁容器模式，控制/排程/狀態直接走 TRS485 機板頁
  // v13098: 排程頁 UI 改為接近遙控器頁，頂部改為單主卡並避免上方網址感資訊列
  // v13097: 記住上次頁面並在恢復前景時重連，降低 AP 使用中回到離線首頁
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
  let version = '13125';

  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), { cache:'no-store' });
    const m = await r.json();
    appName = String(m.name || fallbackName).trim() || fallbackName;
    const match = appName.match(/(\d+)/);
    version = match ? match[1] : '13125';
    console.log('Version loaded:', {appName, version});
  }catch(e){
    appName = fallbackName;
    version = '13125';
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