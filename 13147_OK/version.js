(async function(global){
  // ── 版本變更記錄 ──────────────────────────────────────
  // v13147: 2026-04-02 12:04:47 | 安裝完成後改顯示關閉瀏覽器提醒頁，背景不顯示首頁以避免直接在瀏覽器操作
  // v13146: 2026-04-02 12:00:52 | 首次安裝頁色調改淺，降低深色感並維持首頁風格一致
  // v13145: 2026-04-02 11:57:00 | 首次安裝頁改為首頁色調；按我已連線自動檢測 AP，未連線提醒、已連線直接進入連線狀態頁
  // v13144: 2026-04-02 11:30:15 | iOS 區塊新增 Apple 圖示，Android 安裝完成後顯示提示並解除安裝頁鎖定
  // v13143: 2026-04-02 11:26:00 | 安裝頁文字再精簡：Android 改用圖示檔、移除「至主畫面」與 Android 請先提示、iOS 改為加入主畫面即完成安裝並移除官方教學卡
  // v13142: 2026-04-02 11:19:30 | 安裝頁再精簡：移除 PWA INSTALL 字樣、外開按鈕改為「使用預設瀏覽器開啟」、內建瀏覽器隱藏 Android 小綠人與安裝鍵
  // v13141: 2026-04-02 11:12:09 | 安裝流程升級：新增內建瀏覽器偵測與一鍵外開，iOS/Android 教學文字精簡，小綠人圖示重繪
  // v13140: 2026-04-02 11:06:30 | 安裝頁視覺再優化：Android 小綠人卡片升級，iPhone 教學區塊強化層次，標題統一為「安裝「HMK熱水助手」至主畫面」
  // v13139: 2026-04-02 11:03:39 | 安裝頁標題改為「安裝HMK 熱水助手至主畫面」，加入預設瀏覽器提醒，Android 說明改為同 UI 引導後跳系統安裝視窗
  // v13138: 2026-04-02 10:52:42 | 安裝頁改版：未安裝時不顯示首頁，Android 顯示小綠人安裝區，iOS 顯示 Apple 官方教學圖卡與步驟
  // v13137: 2026-04-02 10:44:35 | 首頁新增首次安裝導引頁：Android 顯示直接安裝鍵，iOS 顯示分享→更多內容→加入主畫面與模擬步驟圖
  // v13136: 2026-04-02 16:20:00 | 白線以下全內框，連線狀態頁直接載入原生 setup，避免切換與重疊問題
  // v13135: 2026-04-02 09:48:57 | 自動版更與備註更新（修正 AP 連線狀態頁與內嵌設定流程）
  // v13134: 2026-04-02 09:22:54 | $(cat .latest_commit_msg.txt)
  // v13133: 2026-04-02 09:11:52 | $(cat .latest_commit_msg.txt)
  // v13132: $msg='修正 AP 模式下頁面切換誤判斷線，跨頁保留最後成功 base 並優先 AP 連線'; $msg | powershell -ExecutionPolicy Bypass -File .\bump-version.ps1
  // v13131: 首頁新增使用者設定入口，新增 user.html 三鍵頁面，並納入 SW 快取
  // v13130: status頁面改為連線狀態標題與PWA標頭
  // v13129: 電費明細頁摘要區新增本期度數框，調整為本期時數/度數/電費三等分置中顯示
  // v13128: 電費明細頁移除功率/電費有色提示框，明細表格表頭與內容改為置中
  // v13127: 電費明細頁補齊舊版功能：功率改4kW/6kW/其它(可輸入)、電費單價可編輯、清單單位切換(小時/度/元)、表格顯示期間/本期/去年同期/差異
  // v13126: PWA 新增電費明細頁：產品狀態電費明細改直連用電記錄，頁首採 HMK+左箭頭標題並移除返回設備狀態鍵
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
  let version = '13147';

  try{
    const r = await fetch('./manifest.json?v=' + Date.now(), { cache:'no-store' });
    const m = await r.json();
    appName = String(m.name || fallbackName).trim() || fallbackName;
    const match = appName.match(/(\d+)/);
    version = match ? match[1] : '13147';
    console.log('Version loaded:', {appName, version});
  }catch(e){
    appName = fallbackName;
    version = '13147';
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