/*
 HMK_D1mini_TRS485_StaticProbe_v17.ino
 Version: v17.2(2026-3-31)

 【必要修改】
 - 開機背景 STA 連線（不阻塞 AP）
 - 保留既有靜態 IP 掃描策略
 - 新增 AP 密碼設定/停用/工廠清空與使用者頁
 - 連線錯誤時,自動重連2次
 - 新增：軟體 RTC + 每日 10 組排程（開始/結束時間，區間內自動開機，區間外自動關機）
 - 新增：/api/time（手機時間校正）、/api/schedule（排程讀寫）
 -手勢修正已可以使用
 =修正為清空 AP 的帳密
 -AP時可以讀取設備情狀,ipok
 -改ssid掃描,可以ap和ip同時在(可以進入,但沒清空ip會失敗)
 -AP或ip都可以進入前三鍵
 -6.4修正排程一至日失效
 6.5修正24小時
 6.6新增 /api/myip + PWA 自動補儲 IP
 6.7修正非192.168的連接
 7.0新增IR和蜂鳴器
 //ir 二鍵的ON=0x1FE20DF  , OFF=0x1FEE01F(六鍵中的2和3鍵)
 7.1畢畢OK
 7.2 多了遙控器三段調溫
 7.3多了殺菌排程
 7.4修網址由220變225,250變245,另一個是開啟控制頁時,去讀機台情況
 7.5修正多了super功能
 8.0用電記錄
 8.1修正用電頁進不去
 8.2修正用電頁的排版
 8.3加快beep速度
 8.4修正用電頁顯示篩選小時,度,元
 8.5修正前天無資料,改用分計
 8.6多個電熱管健康度參考
 8.7 已改好用電記錄問題.改電熱管健康度
 8.8 多電熱管健康度重設鍵
 8.9 beep改無源
 9.0 增加機板shcedule合併至手機系統
 10.0 修正排程季問題,調整24小時制.off只按off.
 10.1 修正設備開關,手機未連動顯示
 10.2 修正顯示溫度,不要隨設定溫馬上變動
 11.0 改用433,包含開後3蜂嗚後,依序super,on,off,65,55,45度記錄,且還可以手動40-45重配對
 12.0 改用新433邏輯,開機5秒內,只要按下off鍵1鍵,就配對id.其它鍵不用按.
 12.1 改成黑白遙控器的邏輯
 13.0 自動修正機版時間.
 14.0 同時校正設備時間和外拉板時間
 15.0 可以自動連外部網路校正義AT+外立面板
 15.01 對於定時,進行同步.(send42Write16(0x0013, 0x0702);OK.....send42Write16Pair(0x0013, 2, 7);NG)
 15.02  修正off或自動.關閉定時狀態的問題.
 16.0  修正為3個排程,且可以通用於T和AT型
 16.1  修正加熱狀態
 17.0  多了est開發頁.
 17.1  排程頁 UI 改為接近遙控器頁，頂部改為單主卡並避免上方網址感資訊列
 17.2  排程頁改為大定時按鈕、四段勾叉啟停、移除星期選項並維持 PWA 殼切換
 
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>
#include <time.h>
#include "control_page.h"
#include "setup_page.h"
#include "schedule_page.h"
#include "status_page.h"
#include "powerlog_page.h"
#include "est_page.h"

#include <RCSwitch.h>



static volatile uint32_t gRs485LastByteMs = 0;


// ---- 型別 ----
struct TargetAP {
  uint8_t bssid[6];
  int32_t channel;
  int rssi;
  bool ok;
};

// ---- 腳位 ----
#define LED_PIN D4
#define LED_ON()  digitalWrite(LED_PIN, LOW)
#define LED_OFF() digitalWrite(LED_PIN, HIGH)
#define PIN_RO   D7  //,綠6
#define PIN_DI   D6 //,綠7
#define PIN_REDE D5

const uint8_t  RF_RX_PIN = 4;   // GPIO4 = D2 (取代原本 IR 接腳，433 接收器 DATA)

const uint8_t BUZZER_PIN = 5;  // = D1

//#define BUZZER_PIN D1

#define EEPROM_SIZE_PWRLOG 4096

#define PWR2YDAY_MARK_OLD 0x50324459UL // 'P2DY'

typedef struct __attribute__((packed)){
  uint32_t mark;
  uint16_t baseYM;           // 跟月資料一致
  uint16_t reserved;
  uint32_t daySec[24][31];   // 24 個月，每月最多 31 天
} Pwr2YDay_t_OLD;

static Pwr2YDay_t_OLD gPwr2YDay_OLD;
static bool gPwr2YDayDirty_OLD = false;

// ⚠️ 位址請避開你既有 EEPROM 使用區，這裡示意：放在月資料後面
#define EEPROM_ADDR_PWR2YDAY_OLD  (EEPROM_ADDR_PWR2Y + sizeof(gPwr2Y) + 16)


SoftwareSerial RS485(PIN_RO, PIN_DI);

// ---- AP 名稱生成 ----
String makeApSsid() {
  uint32_t id = ESP.getChipId();
  String s = "HMK-Setup-";
  s += String(id, HEX);
  s.toUpperCase();
  return s;
}

// ---- 延後 AP 變更機制 ----
enum PendingApAction {
  AP_ACT_NONE,
  AP_ACT_SET_PASS,
  AP_ACT_DISABLE_PASS,
  AP_ACT_FACTORY_RESET
};
static PendingApAction PENDING_AP_ACTION = AP_ACT_NONE;
static unsigned long   PENDING_AP_TS     = 0;

// ---- Wi-Fi / AP ----
String AP_SSID = makeApSsid();

// ---- RS485 舊手勢狀態（保留，不使用） ----
static uint8_t  PATTERN_STAGE = 0;
static int      LAST_MODE      = -1;
static uint16_t LAST_TEMP_RAW  = 0;
static bool     LAST_TEMP_VALID= false;
static unsigned long PATTERN_START_MS = 0;
static const unsigned long PATTERN_WINDOW_MS = 8000;
static bool DEBUG_GESTURE = true;
static bool NEED_CLEAR_PWA_IP = false;


// ==== 新手勢：20秒內 40/41→≥44→回到≤41 觸發工廠清空 ====
enum SweepPhase { SW_IDLE, SW_UP, SW_DOWN };
static SweepPhase SW_PHASE = SW_IDLE;
static unsigned long SW_T0 = 0;
static int SW_LAST = -1;
static const unsigned long SWEEP_WINDOW_MS = 20000; // 20 秒


// 前置宣告（供 parseHostStatus 使用）
static void updateGesture(uint8_t evtType); // 目前未實作，僅保留宣告以供未來擴充

// 前置宣告（供殺菌手動模式 restoreSterManual() 呼叫）
void checkSchedule(bool force);
static void rebuildBoardTimerPlanFromSaved();
static void rebuildBoardTimerPlanFromBoardRaw();
static void syncBoardSchedulesFromPhone();
static bool isSterScheduleActiveNow();
static void estOnHeatStateUpdated();

static void beep(uint8_t times=1, uint16_t onMs=250, uint16_t offMs=30){
  for(uint8_t i=0;i<times;i++){
    digitalWrite(BUZZER_PIN, HIGH); // 2N2222A：HIGH=導通=叫
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if(i+1<times) delay(offMs);
  }
}
// ============================================================
// Board Schedule Snapshot (Host addr=0x0007, regs 7..20, 14 bytes)
// reg7..18 = 定時1~3 開始/結束（BCD；0xFF=未設定）
// reg19    = 工作模式（0=關機,1=自動,2=定時）
// reg20    = 定時1~3 啟用 bit（bit0..2）
// ============================================================
static uint8_t  gBoardSchedRaw[14];     // reg7..reg20
static bool     gBoardSchedValid = false;
static uint32_t gBoardSchedLastMs = 0;

static const uint16_t BOARD_T1_START_ADDR = 0x0007;
static const uint16_t BOARD_T1_END_ADDR   = 0x0009;
static const uint16_t BOARD_T2_START_ADDR = 0x000B;
static const uint16_t BOARD_T2_END_ADDR   = 0x000D;
static const uint16_t BOARD_T3_START_ADDR = 0x000F;
static const uint16_t BOARD_T3_END_ADDR   = 0x0011;
static const uint16_t BOARD_MODE_ADDR     = 0x0013; // reg19
static const uint16_t BOARD_MASK_ADDR     = 0x0014; // reg20
static const uint8_t  BOARD_MODE_OFF      = 0;
static const uint8_t  BOARD_MODE_AUTO     = 1;
static const uint8_t  BOARD_MODE_TIMER    = 2;

struct BoardUnionRange {
  uint16_t startMin;
  uint16_t endMin;
};
static BoardUnionRange gBoardUnion[6];
static uint8_t gBoardUnionCount = 0;

struct BoardMinuteEvent {
  uint16_t minute;
  uint8_t  setMaskByDay[7];   // 依星期(一..日)到點要設為 1 的 bit
  uint8_t  clearMaskByDay[7]; // 依星期(一..日)到點要設為 0 的 bit
  uint8_t  sendTimerDays;     // bit0=一 .. bit6=日
  uint8_t  sendOffDays;       // bit0=一 .. bit6=日
};
static BoardMinuteEvent gBoardMinuteEvents[8];
static uint8_t gBoardMinuteEventCount = 0;

// ============================================================
// Board Time Snapshot (Host addr=0x0001, regs 1..6, 6 bytes)
// reg1=年 reg2=星期 reg3=月 reg4=日 reg5=時 reg6=分
// ============================================================
static uint8_t  gBoardTimeRaw[6] = {0};
static bool     gBoardTimeValid = false;
static uint32_t gBoardTimeLastMs = 0;

// 機板時間欄位（reg5/reg6）實測為 BCD：
// 09 -> 0x09，10 -> 0x10，12 -> 0x12。
// 所以顯示時要先解碼；寫入時也要先編成 BCD。
static inline uint8_t bcdToDec(uint8_t v){
  return (uint8_t)(((v >> 4) * 10) + (v & 0x0F));
}
static inline uint8_t decToBcd(uint8_t v){
  return (uint8_t)(((v / 10) << 4) | (v % 10));
}
static inline bool boardTimeHMValidRaw(uint8_t hhRaw, uint8_t mmRaw){
  uint8_t hh = bcdToDec(hhRaw);
  uint8_t mm = bcdToDec(mmRaw);
  return (hh <= 23 && mm <= 59);
}

static inline String boardTimeHM(){
  if (!gBoardTimeValid) return "--:--";
  uint8_t hhRaw = gBoardTimeRaw[4];
  uint8_t mmRaw = gBoardTimeRaw[5];
  if (!boardTimeHMValidRaw(hhRaw, mmRaw)) return "--:--";
  uint8_t hh = bcdToDec(hhRaw);
  uint8_t mm = bcdToDec(mmRaw);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)hh, (unsigned)mm);
  return String(buf);
}

static inline bool boardTimeValid4(uint8_t h1,uint8_t m1,uint8_t h2,uint8_t m2){
  return (h1 != 0xFF && m1 != 0xFF && h2 != 0xFF && m2 != 0xFF);
}

static inline bool boardActiveNow(uint16_t nowMin, uint16_t startMin, uint16_t endMin){
  if (startMin == endMin) return false; // 避免怪設定（你也可改成 true=全天）
  if (startMin < endMin)  return (nowMin >= startMin && nowMin < endMin);
  // 跨日
  return (nowMin >= startMin || nowMin < endMin);
}

static inline String hhmm(uint8_t hh, uint8_t mm){
  if (hh == 0xFF || mm == 0xFF) return "--:--";
  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)hh, (unsigned)mm);
  return String(buf);
}




// ===== 單聲播放（無源蜂鳴器）=====
static void beepOnce(int freq, int ms = 90){
  if (freq <= 0) {
    noTone(BUZZER_PIN);
    delay(ms);
    return;
  }
  tone(BUZZER_PIN, freq);
  delay(ms);
  noTone(BUZZER_PIN);
  delay(18);
  yield();
}

// ===== 兩聲（同頻率）=====
static void beep2(int freq = 2000, int onMs = 80, int gapMs = 80){
  for (int i = 0; i < 2; i++){
    tone(BUZZER_PIN, freq);
    delay(onMs);
    noTone(BUZZER_PIN);
    if (i == 0) delay(gapMs);
    yield();
  }
}



volatile uint32_t irEdges = 0;
ICACHE_RAM_ATTR void irISR() { irEdges++; }



// ---- 狀態 ----
float tUp = 0;
int targetTemp = 50, currentMode = -1;
static const uint8_t  MY_ID     = 7;
static const uint16_t TEMP_ADDR_PRIMARY   = 0x0015; // 溫度狀態所在（d[10]）
static const uint16_t TEMP_ADDR_FALLBACK  = 0x001F; // 保留舊假設

// 主機異常狀態（位址 37, 只讀 0x00~0xFF）
static uint8_t  hostFaultReg   = 0;
static bool     hostFaultValid = false;
static const uint16_t HOST_FAULT_ADDR = 0x0025; // reg37

// ===== 加熱判定：位址 39 (=0x0027) bit0 =====
static const uint16_t HEAT_ADDR = 0x0027;   // 39 decimal
static uint8_t  hostHeatReg   = 0;          // 位址39原始值(0~255)
static bool     hostHeatValid = false;      // 是否曾成功讀到
static bool     HEATING_NOW   = false;      // bit0=1 => 加熱中
static uint32_t HEAT_LAST_MS  = 0;          // 最後一次更新時間


// ==== 寄存器常數（寫入用）====
// 模式寫入寄存器：原始為 0x0013
static const uint16_t MODE_ADDR = 0x0013;

// 溫度寫入寄存器：原始假設 0x001F；若確認實際是 0x0015 可改成 0x0015
static const uint16_t TEMP_ADDR_WRITE = 0x001F;

// AP 密碼（EEPROM 128..159；flag 160 == 0xA5）
static char AP_PASS_BUF[33] = {0};
static bool AP_PASS_SET = false;
static const int AP_PASS_OFF  = 128;
static const int AP_PASS_FLAG = 160;

// =============================================================
// Est：重新送電後第 1~4 次加熱設定值（實驗用）
// =============================================================
#define EEPROM_ADDR_ESTCFG 896
#define EST_CFG_MARK       0xEC

struct EstCfg {
  uint8_t mark;
  uint8_t enabled;
  uint8_t t1;
  uint8_t t2;
  uint8_t t3;
  uint8_t t4;
};

static EstCfg gEstCfg = { EST_CFG_MARK, 0, 50, 50, 50, 50 };
static uint8_t gEstHeatSeq = 0;
static uint8_t gEstLastAppliedSeq = 0;
static uint8_t gEstLastAppliedTemp = 0;
static bool    gEstPrevHeating = false;
static bool    gEstHeatStateInit = false;

static uint8_t clampEstTemp(int t){
  if (t < 40) t = 40;
  if (t > 70) t = 70;
  return (uint8_t)t;
}

static void estSave(){
  EEPROM.begin(EEPROM_SIZE_PWRLOG);
  EEPROM.put(EEPROM_ADDR_ESTCFG, gEstCfg);
  EEPROM.commit();
}

static void estResetRuntime(bool syncCurrentHeat = false){
  gEstHeatSeq = 0;
  gEstLastAppliedSeq = 0;
  gEstLastAppliedTemp = 0;
  if (syncCurrentHeat && hostHeatValid) {
    gEstPrevHeating = HEATING_NOW;
    gEstHeatStateInit = true;
  } else {
    gEstPrevHeating = false;
    gEstHeatStateInit = false;
  }
}

static void estLoad(){
  EEPROM.begin(EEPROM_SIZE_PWRLOG);
  EEPROM.get(EEPROM_ADDR_ESTCFG, gEstCfg);
  if (gEstCfg.mark != EST_CFG_MARK) {
    gEstCfg.mark = EST_CFG_MARK;
    gEstCfg.enabled = 0;
    gEstCfg.t1 = 50;
    gEstCfg.t2 = 50;
    gEstCfg.t3 = 50;
    gEstCfg.t4 = 50;
    estSave();
  }
  gEstCfg.t1 = clampEstTemp(gEstCfg.t1);
  gEstCfg.t2 = clampEstTemp(gEstCfg.t2);
  gEstCfg.t3 = clampEstTemp(gEstCfg.t3);
  gEstCfg.t4 = clampEstTemp(gEstCfg.t4);
  estResetRuntime(false);
}

static uint8_t estTempForSeq(uint8_t seq){
  if (seq == 1) return gEstCfg.t1;
  if (seq == 2) return gEstCfg.t2;
  if (seq == 3) return gEstCfg.t3;
  return gEstCfg.t4;
}

static void estApplySeqTemp(uint8_t seq){
  if (seq < 1 || seq > 4) return;
  uint8_t t = estTempForSeq(seq);
  targetTemp = (int)t;
  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
  gEstLastAppliedSeq = seq;
  gEstLastAppliedTemp = t;
  Serial.printf("[EST] heat #%u -> set %uC\n", (unsigned)seq, (unsigned)t);
}

// WebServer
ESP8266WebServer server(80);

// AP 時間窗口
static unsigned long AP_WINDOW_DEADLINE = 0;
static bool AP_WINDOW_ON = false;

// Wi-Fi Event Flags
static WiFiEventHandler EH_GOTIP, EH_DISC;
static volatile bool WIFI_GOTIP = false, WIFI_FAIL = false;

// ---- NTP 自動校時（上電 / 重連 / 每24小時） ----
static WiFiUDP NTP_UDP;
static bool STA_LINK_WAS_UP = false;
static bool NTP_HAS_SYNC = false;
static unsigned long NTP_LAST_SYNC_MS = 0;
static unsigned long NTP_LAST_TRY_MS  = 0;
static char NTP_LAST_SOURCE[32] = {0};

static const char* NTP_SERVER_1 = "time.windows.com";
static const char* NTP_SERVER_2 = "time.cloudflare.com";
static const char* NTP_SERVER_3 = "pool.ntp.org";

static const unsigned long NTP_RESYNC_MS        = 86400000UL; // 24h
static const unsigned long NTP_QUERY_TIMEOUT_MS = 2500UL;
static const unsigned long NTP_RETRY_GAP_MS     = 10000UL;
static const long          TZ_OFFSET_SEC         = 8L * 3600L; // Asia/Taipei

// ---- 手勢觸發工廠清空 ----
static void clearWiFi();
static void clearApPass();

// ==== 新手勢內部偵測門檻 ====
// 使用者操作指示：將溫度調到 40 或 41，升到 ≥44 再降回 40 或 41（20 秒內完成）
// 內部實際判定：40/41（起點）→ 升到 ≥44 → 降回 ≤41
static const uint8_t SW_START_TEMP = 41;   // 起點上限 41（實際允許 40 或 41）
static const uint8_t SW_PEAK_TEMP  = 44;   // 峰值 ≥44
static const uint8_t SW_END_TEMP   = 41;   // 結束需回到 ≤41


// ---- 開機 STA 背景嘗試旗標 ----
static bool BOOT_STA_PENDING = false;
static unsigned long BOOT_STA_TS = 0;
static char BOOT_SSID[32] = {0};
static char BOOT_PASS[64] = {0};

// ---- 最近一次成功的 STA IP（給 PWA 補抓用）----
static IPAddress LAST_STA_IP;
static bool      LAST_STA_IP_VALID = false;


// =============================================================
// 軟體 RTC（每日 0..1439 分）+ 10 組排程
// =============================================================
static uint32_t rtcLastMs   = 0;
static uint16_t rtcMinOfDay = 0;  // 0..1439
static uint8_t  rtcSec      = 0;  // 0..59
static uint8_t  rtcWday     = 0;  // 1..7 = 週一..週日, 0 = 未設定（視為每天）

// ---- RTC 日期（年/月/日；0=未設定）----
static uint16_t rtcYear  = 0;
static uint8_t  rtcMonth = 0;
static uint8_t  rtcDay   = 0;

static inline bool rtcDateValid(){
  return (rtcYear >= 2020 && rtcYear <= 2099 && rtcMonth >= 1 && rtcMonth <= 12 && rtcDay >= 1 && rtcDay <= 31);
}

static inline bool isLeapYear(uint16_t y){
  return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

static inline uint8_t daysInMonth(uint16_t y, uint8_t m){
  static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m < 1 || m > 12) return 30;
  if (m == 2 && isLeapYear(y)) return 29;
  return mdays[m-1];
}

static void rtcAdvanceOneDay(){
  if (!rtcDateValid()) return;
  uint8_t dim = daysInMonth(rtcYear, rtcMonth);
  if (rtcDay < dim) {
    rtcDay++;
    return;
  }
  rtcDay = 1;
  if (rtcMonth < 12) {
    rtcMonth++;
    return;
  }
  rtcMonth = 1;
  rtcYear++;
}

void addCors();   // ★★ 宣告在這 ★★
// =============================================================
// 用電時間記錄（2 年 / 24 個月滾動）
// - 記錄的是「設備處於開機(Mode!=0)」的時間（秒）
// - 需要手機 /api/time 校時含 y/mo/d 才能做月/季/年與去年同期
// =============================================================
struct Power2YLog {
  uint8_t  mark;         // PWR2Y_MARK
  uint16_t baseYM;       // year*12 + (month-1) ; index0 = 最舊月份
  uint32_t monthSec[24]; // 24 個月累計秒數（連續月份）
  uint32_t totalSec;     // 全期間累計秒數（不歸零）
  uint32_t todaySec;     // 今日累計秒數（跨日歸零）
  uint16_t lastY;        // 上次判斷用的日期
  uint8_t  lastM;
  uint8_t  lastD;
};

static Power2YLog gPwr2Y;
static bool     gPwr2YDirty      = false;
static uint32_t gPwr2YLastTickMs = 0;
static uint32_t gPwr2YLastSaveMs = 0;
static const uint8_t PWR2Y_MARK  = 0xD2;
static const int EEPROM_ADDR_PWR2Y = 320;

static inline uint16_t ymKey(uint16_t y, uint8_t m){
  return (uint16_t)(y * 12 + (m - 1)); // m=1..12
}

// ===== [新增] 用電每日統計結構 =====
#define EEPROM_BYTES            4096   // 原本 512 不夠，請確保全程都用這個大小
#define EEPROM_ADDR_PWR2Y_DAY   1024   // 日資料區段位址

#define PWR2YDAY_MARK 0x59444D59UL    // 'YDMY' 標記碼

// =============================================================
// 電熱管健康度參考（60→65°C 取樣）
//  - 標準值：剛裝電熱管時，收集 10 次 60→65°C 的加熱秒數，取平均存 EEPROM
//  - 健康度：後續每次 60→65°C 的時間與標準相比：pct = std / last * 100
// =============================================================
#define EEPROM_ADDR_HEATER_HEALTH 280
#define HEATER_HEALTH_MARK        0xE7

struct HeaterHealthRef {
  uint8_t  mark;     // HEATER_HEALTH_MARK
  uint8_t  count;    // 已收集標準次數（0..10）
  uint16_t stdSec;   // 標準秒數（10 次平均後寫入）
  uint16_t lastSec;  // 最近一次 60→65°C 秒數
  uint32_t sumSec;   // 校正階段累加用
};

static HeaterHealthRef gHeaterHealth;

static int      HH_LAST_TEMP   = -100;
static bool     HH_MEASURING   = false;
static uint32_t HH_START_MS    = 0;

static void heaterHealthSave(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.put(EEPROM_ADDR_HEATER_HEALTH, gHeaterHealth);
  EEPROM.commit();
}

static void heaterHealthLoad(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.get(EEPROM_ADDR_HEATER_HEALTH, gHeaterHealth);
  if (gHeaterHealth.mark != HEATER_HEALTH_MARK) {
    memset(&gHeaterHealth, 0, sizeof(gHeaterHealth));
    gHeaterHealth.mark = HEATER_HEALTH_MARK;
    heaterHealthSave();
  }
}

static uint8_t heaterHealthPct(){
  if (gHeaterHealth.stdSec == 0 || gHeaterHealth.lastSec == 0) return 0;
  uint32_t pct = (uint32_t)gHeaterHealth.stdSec * 100UL / (uint32_t)gHeaterHealth.lastSec;
  if (pct > 100) pct = 100;
  return (uint8_t)pct;
}

static void heaterHealthAddSample(uint16_t durSec){
  gHeaterHealth.lastSec = durSec;

  // 尚未建立標準值 → 收集 10 次
  if (gHeaterHealth.stdSec == 0 && gHeaterHealth.count < 10) {
    gHeaterHealth.sumSec += durSec;
    gHeaterHealth.count++;
    if (gHeaterHealth.count >= 10) {
      // 四捨五入
      gHeaterHealth.stdSec = (uint16_t)((gHeaterHealth.sumSec + (gHeaterHealth.count/2)) / gHeaterHealth.count);
    }
  }

  heaterHealthSave();
}

static void heaterHealthTick(int tempNow){
  // 只在「開機中」且設定溫 >= 65 才跟（避免關機、低溫設定誤觸發）
  const bool heaterOn = (currentMode > 0);
  const bool allow    = heaterOn && (targetTemp >= 65);

  if (tempNow < 0 || tempNow > 120) return;

  if (HH_LAST_TEMP == -100) { HH_LAST_TEMP = tempNow; return; }

  if (!allow) {
    HH_MEASURING = false;
    HH_LAST_TEMP = tempNow;
    return;
  }

  // 進入 60°C 觸發、到達 65°C 結束
  if (!HH_MEASURING) {
    if (HH_LAST_TEMP < 60 && tempNow >= 60 && tempNow < 65) {
      HH_MEASURING = true;
      HH_START_MS  = millis();
    }
  } else {
    if (tempNow >= 65) {
      uint32_t durSec = (millis() - HH_START_MS) / 1000UL;
      // 合理範圍保護：5 秒~60 分鐘（避免雜訊，並避免快加熱被誤濾掉）
      if (durSec >= 5 && durSec <= 3600UL) {
        heaterHealthAddSample((uint16_t)durSec);
      }
      HH_MEASURING = false;
    } else if ((millis() - HH_START_MS) > 7200000UL) {
      // 超過 2 小時視為失敗，避免卡死
      HH_MEASURING = false;
    }
  }

  HH_LAST_TEMP = tempNow;
}
//===================================================================


struct Pwr2YDayLog {
  uint32_t mark;
  uint16_t baseYM;               // 月視窗基準
  uint32_t daySec[24][31];       // [月索引][日]
};

static Pwr2YDayLog gPwr2YDay;
static bool gPwr2YDayDirty = false;

static void pwr2yDayInit(uint16_t baseYM){
  memset(&gPwr2YDay, 0, sizeof(gPwr2YDay));
  gPwr2YDay.mark = PWR2YDAY_MARK;
  gPwr2YDay.baseYM = baseYM;
  gPwr2YDayDirty = true;
}

static void pwr2yDayShiftWindow(uint16_t newBaseYM){
  if (newBaseYM == gPwr2YDay.baseYM) return;
  int32_t shift = (int32_t)newBaseYM - (int32_t)gPwr2YDay.baseYM;
  if (shift <= 0 || shift >= 24) {
    pwr2yDayInit(newBaseYM);
    return;
  }
  for (int i=0;i<24;i++){
    int src = i + shift;
    if (src < 24){
      memcpy(gPwr2YDay.daySec[i], gPwr2YDay.daySec[src], sizeof(gPwr2YDay.daySec[i]));
    }else{
      memset(gPwr2YDay.daySec[i], 0, sizeof(gPwr2YDay.daySec[i]));
    }
  }
  gPwr2YDay.baseYM = newBaseYM;
  gPwr2YDayDirty = true;
}

static void pwr2yDayLoad(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.get(EEPROM_ADDR_PWR2Y_DAY, gPwr2YDay);
  if (gPwr2YDay.mark != PWR2YDAY_MARK){
    memset(&gPwr2YDay, 0, sizeof(gPwr2YDay));
    gPwr2YDay.mark = PWR2YDAY_MARK;
    gPwr2YDay.baseYM = gPwr2Y.baseYM;
    gPwr2YDayDirty = true;
  }
}

static void pwr2yDaySave(){
  if (!gPwr2YDayDirty) return;
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.put(EEPROM_ADDR_PWR2Y_DAY, gPwr2YDay);
  EEPROM.commit();
  EEPROM.end();
  gPwr2YDayDirty = false;
}

static void pwr2yDayTouchToday(){
  if (!rtcDateValid()) return;
  int ym = (int)rtcYear*12 + ((int)rtcMonth-1);
  int mi = ym - (int)gPwr2Y.baseYM;
  int di = (int)rtcDay - 1;
  if (mi < 0 || mi >= 24) return;
  if (di < 0 || di >= 31) return;

  gPwr2YDay.daySec[mi][di] = gPwr2Y.todaySec;  // todaySec 你原本就有
  gPwr2YDayDirty = true;
}


static void pwr2yInitWindow(uint16_t curYM){
  // 讓 window 結尾對齊當前月份（curYM）
  memset(&gPwr2Y, 0, sizeof(gPwr2Y));
  gPwr2Y.mark = PWR2Y_MARK;
  gPwr2Y.baseYM = (curYM >= 23) ? (uint16_t)(curYM - 23) : curYM; // 年份很小時就不倒扣
  gPwr2YDirty = true;
}

static void pwr2yEnsureWindow(uint16_t curYM){
  if (gPwr2Y.mark != PWR2Y_MARK || gPwr2Y.baseYM == 0) {
    pwr2yInitWindow(curYM);
    return;
  }
  // 時鐘被調回去：直接重建，避免索引錯亂
  if (curYM < gPwr2Y.baseYM) {
    pwr2yInitWindow(curYM);
    return;
  }
  uint16_t endYM = (uint16_t)(gPwr2Y.baseYM + 23);
  if (curYM <= endYM) return;

  uint16_t shift = (uint16_t)(curYM - endYM); // 需要往前推 shift 個月
  if (shift >= 24) {
    // 相差超過 2 年：清空重建
    pwr2yInitWindow(curYM);
    return;
  }
  // 左移
  for (uint8_t i=0;i<24-shift;i++){
    gPwr2Y.monthSec[i] = gPwr2Y.monthSec[i + shift];
  }
  for (uint8_t i=24-shift;i<24;i++){
    gPwr2Y.monthSec[i] = 0;
  }
  gPwr2Y.baseYM = (uint16_t)(gPwr2Y.baseYM + shift);
  gPwr2YDirty = true;
}

static void pwr2yLoad(){
  
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.get(EEPROM_ADDR_PWR2Y, gPwr2Y);
  if (gPwr2Y.mark != PWR2Y_MARK) {
    // 未初始化：等有日期再建 window；先清空
    memset(&gPwr2Y, 0, sizeof(gPwr2Y));
    gPwr2Y.mark = PWR2Y_MARK;
    gPwr2YDirty = true;
  }
  pwr2yDayLoad(); 
}

static void pwr2ySaveNow(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.put(EEPROM_ADDR_PWR2Y, gPwr2Y);
  EEPROM.commit();
  gPwr2YDirty = false;
  gPwr2YLastSaveMs = millis();
}

static void pwr2yRolloverIfNeeded(){
  if (!rtcDateValid()) return;

  if (gPwr2Y.lastY == 0) {
    gPwr2Y.lastY = rtcYear;
    gPwr2Y.lastM = rtcMonth;
    gPwr2Y.lastD = rtcDay;
    uint16_t curYM = ymKey(rtcYear, rtcMonth);
    pwr2yEnsureWindow(curYM);
    gPwr2YDirty = true;
    return;
  }

  if (gPwr2Y.lastY != rtcYear || gPwr2Y.lastM != rtcMonth || gPwr2Y.lastD != rtcDay) {
    // 跨日：今日歸零
    gPwr2Y.todaySec = 0;
    gPwr2Y.lastY = rtcYear;
    gPwr2Y.lastM = rtcMonth;
    gPwr2Y.lastD = rtcDay;

    uint16_t curYM = ymKey(rtcYear, rtcMonth);
    pwr2yEnsureWindow(curYM);
    gPwr2YDirty = true;
  }
}

const uint32_t sec = 1;  // ★補這行，讓 sec 有宣告


static void tickPower2Y(){
  uint32_t now = millis();
  if (gPwr2YLastTickMs == 0) {
    gPwr2YLastTickMs = now;
    return;
  }

  uint32_t delta = now - gPwr2YLastTickMs;

  // ★改成每分鐘累加：60000ms
  if (delta < 60000) {
    // 仍定期落盤（不用太頻繁）
    if ((gPwr2YDirty || gPwr2YDayDirty) && (now - gPwr2YLastSaveMs) > 60000) {
      pwr2ySaveNow();
      pwr2yDaySave();
    }
    return;
  }

  uint32_t addMin = delta / 60000;          // ★本次增加「分鐘」
  gPwr2YLastTickMs += addMin * 60000;       // 保留餘數在下次累加

  // 先處理跨日/月 window（跨日會把 todaySec 歸零）
  pwr2yRolloverIfNeeded();

  bool running = false;

  // 0=關機：不累加（不用判定位址39）
  if (currentMode == 0) {
    running = false;
  }
  // 1=自動、2=定時：一定要判定位址39 bit0
  else if (currentMode == 1 || currentMode == 2) {
    running = (hostHeatValid && HEATING_NOW);
  }
  // 其它/未知：不累加（保守）
  else {
    running = false;
  }

  if (running) {
    // ===== ✅ 這段就是你缺的「累加」 =====
    gPwr2Y.todaySec += addMin;   // ※注意：你現在用「分鐘」當單位存
    gPwr2Y.totalSec += addMin;

    if (rtcDateValid()) {
      uint16_t curYM = ymKey(rtcYear, rtcMonth);
      pwr2yEnsureWindow(curYM);

      uint16_t midx = (uint16_t)(curYM - (uint16_t)gPwr2Y.baseYM);
      if (midx < 24) {
        gPwr2Y.monthSec[midx] += addMin;
      }

      // 日資料 window 跟月資料對齊
      if (gPwr2YDay.baseYM != (uint16_t)gPwr2Y.baseYM) {
        pwr2yDayShiftWindow((uint16_t)gPwr2Y.baseYM);
      }

      // 把 todaySec 同步到 daySec[月][日]
      pwr2yDayTouchToday();
    }

    gPwr2YDirty = true;
  }

  if ((gPwr2YDirty || gPwr2YDayDirty) && (now - gPwr2YLastSaveMs) > 60000) {
    pwr2ySaveNow();
    pwr2yDaySave();
  }
}


static void handleApiPowerLog(){
  addCors();
  pwr2yRolloverIfNeeded();
  pwr2yDayLoad();

  String out;
  out.reserve(16000);
  out = "{\"ok\":true";
  out += ",\"validDate\":" + String(rtcDateValid() ? 1 : 0);
  if (rtcDateValid()) {
    out += ",\"y\":" + String((int)rtcYear);
    out += ",\"mo\":" + String((int)rtcMonth);
    out += ",\"d\":" + String((int)rtcDay);
  }
  out += ",\"todaySec\":" + String(gPwr2Y.todaySec * 60UL);
  out += ",\"totalSec\":" + String(gPwr2Y.totalSec * 60UL);
  out += ",\"baseYM\":" + String((int)gPwr2Y.baseYM);

  // 月資料
  out += ",\"months\":[";
  for (uint8_t i=0;i<24;i++){
    out += String(gPwr2Y.monthSec[i]* 60UL);
    if (i<23) out += ",";
  }
  out += "]";

  // 日資料（當前年度與去年同期）
  out += ",\"daysByYM\":{";
  for (uint8_t i=0;i<24;i++){
    uint16_t ym = (uint16_t)gPwr2Y.baseYM + i;
    out += "\"" + String(ym) + "\":[";
    for (uint8_t d=0; d<31; d++){
      out += String(gPwr2YDay.daySec[i][d]* 60UL);
      if (d<30) out += ",";
    }
    out += "]";
    if (i<23) out += ",";
  }
  out += "}";

  out += ",\"daysByYM_py\":{";
  for (uint8_t i=0;i<24;i++){
    uint16_t ym = (uint16_t)gPwr2Y.baseYM + i;
    out += "\"" + String(ym) + "\":[";
    if (i >= 12){
      uint8_t src = i - 12;
      for (uint8_t d=0; d<31; d++){
        out += String(gPwr2YDay.daySec[src][d]* 60UL);
        if (d<30) out += ",";
      }
    } else {
      for (uint8_t d=0; d<31; d++){
        out += "0";
        if (d<30) out += ",";
      }
    }
    out += "]";
    if (i<23) out += ",";
  }
  out += "}";

  out += "}";
  server.send(200, "application/json", out);
}


static void handleApiPowerLogPost(){
  addCors();
  String reset = "";
  if (server.hasArg("reset")) reset = server.arg("reset");
  pwr2yDayLoad();

  if (reset == "today") {
    gPwr2Y.todaySec = 0;
    if (rtcDateValid()){
      uint16_t curYM = ymKey(rtcYear, rtcMonth);
      pwr2yEnsureWindow(curYM);
      if (gPwr2YDay.baseYM != (uint16_t)gPwr2Y.baseYM) {
        pwr2yDayShiftWindow((uint16_t)gPwr2Y.baseYM);
      }
      uint16_t midx = (uint16_t)(curYM - (uint16_t)gPwr2Y.baseYM);
      uint8_t didx = (rtcDay >= 1 && rtcDay <= 31) ? (uint8_t)(rtcDay - 1) : 0;
      if (midx < 24 && didx < 31){
        gPwr2YDay.daySec[midx][didx] = 0;
        gPwr2YDayDirty = true;
      }
    }
    gPwr2YDirty = true;
    pwr2ySaveNow();
    pwr2yDaySave();
    server.send(200, "application/json", "{\"ok\":true,\"reset\":\"today\"}");
    return;
  }

  if (reset == "month") {
    if (rtcDateValid()) {
      uint16_t curYM = ymKey(rtcYear, rtcMonth);
      pwr2yEnsureWindow(curYM);
      uint16_t idx = (uint16_t)(curYM - (uint16_t)gPwr2Y.baseYM);
      if (idx < 24) gPwr2Y.monthSec[idx] = 0;
      if (gPwr2YDay.baseYM != (uint16_t)gPwr2Y.baseYM) {
        pwr2yDayShiftWindow((uint16_t)gPwr2Y.baseYM);
      }
      if (idx < 24){
        memset(gPwr2YDay.daySec[idx], 0, sizeof(gPwr2YDay.daySec[idx]));
        gPwr2YDayDirty = true;
      }
    }
    gPwr2YDirty = true;
    pwr2ySaveNow();
    pwr2yDaySave();
    server.send(200, "application/json", "{\"ok\":true,\"reset\":\"month\"}");
    return;
  }

  // reset=all
  if (rtcDateValid()) {
    uint16_t curYM = ymKey(rtcYear, rtcMonth);
    pwr2yInitWindow(curYM);
    pwr2yDayInit((uint16_t)gPwr2Y.baseYM);
  } else {
    memset(&gPwr2Y, 0, sizeof(gPwr2Y));
    gPwr2Y.mark = PWR2Y_MARK;
    gPwr2YDirty = true;
    pwr2yDayInit((uint16_t)gPwr2Y.baseYM);
  }
  pwr2ySaveNow();
  pwr2yDaySave();
  server.send(200, "application/json", "{\"ok\":true,\"reset\":\"all\"}");
}


// =============================================================
// IR  和 蜂鳴器
// =============================================================

// 433（EV1527 24-bit：ID20 + KEY4）
// 配對流程（新版）：
// - 開機嗶 1 聲後，5 秒內按任一鍵；收到 ID 後，僅當 KEY4=0x8 才會進行配對
// - 配對完成後嗶 2 聲，之後只接受同一個 ID20 的按鍵
// - KEY4 對應：
//    0x4=OFF, 0x8=ON, 0xc=45°C, 0x2=55°C, 0xA=70°C, 0x6=殺菌(手動 1 小時)
// 接收腳：D2(GPIO4)

// ★重要：舊版 v11 曾用 304，會與 PWR2Y(320) 重疊；新址改到 480
#define EEPROM_ADDR_RF433_PAIR_OLD  304
#define EEPROM_ADDR_RF433_PAIR      480
#define RF433_PAIR_MARK             0x52463433UL // 'RF43'

typedef struct __attribute__((packed)) {
  uint32_t mark;
  uint32_t id20;   // 低 20-bit 有效 (0..0xFFFFF)
  uint32_t rsv0;
  uint32_t rsv1;
  uint32_t rsv2;
  uint32_t rsv3;
  uint32_t rsv4;
  uint32_t rsv5;
} RF433Pair_t;

static RF433Pair_t gRF = {0};

static uint32_t g433BootMs = 0;
static bool     g433NeedPair = false;
static bool     g433Pairing = false;
static uint32_t g433PairDeadlineMs = 0;

static inline bool rf433IsPaired(){
  return (gRF.mark == RF433_PAIR_MARK) && (gRF.id20 != 0) && (gRF.id20 <= 0xFFFFFUL);
}

static void rf433SavePair(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.put(EEPROM_ADDR_RF433_PAIR, gRF);
  EEPROM.commit();
}

static void rf433LoadPair(){
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.get(EEPROM_ADDR_RF433_PAIR, gRF);
  if (rf433IsPaired()) return;

  // 嘗試從舊址(304)遷移：舊版是 6 組 code，其中 codeOff 可推回同一個 ID20
  typedef struct __attribute__((packed)) {
    uint32_t mark;
    uint32_t codeSuper;
    uint32_t codeOn;
    uint32_t codeOff;
    uint32_t codeT65;
    uint32_t codeT55;
    uint32_t codeT45;
    uint32_t rsv;
  } OldRF433Pair_t;

  OldRF433Pair_t old = {0};
  EEPROM.get(EEPROM_ADDR_RF433_PAIR_OLD, old);
  if (old.mark == RF433_PAIR_MARK && old.codeOff) {
    uint32_t raw24 = ((uint32_t)old.codeOff) & 0xFFFFFFUL;
    uint32_t id20  = (raw24 >> 4) & 0xFFFFFUL;
    if (id20) {
      gRF = (RF433Pair_t){0};
      gRF.mark = RF433_PAIR_MARK;
      gRF.id20 = id20;
      rf433SavePair();
      Serial.printf("[433] migrated old pair -> ID20=0x%05lX\n", (unsigned long)id20);
    }
  }
}

static void rf433ClearPair(){
  gRF = (RF433Pair_t){0};
  rf433SavePair();
  g433NeedPair = true;
}

static void rf433BeginPairing(uint32_t windowMs){
  g433Pairing = true;
  g433PairDeadlineMs = millis() + windowMs;
}

static void rf433StopPairing(){
  g433Pairing = false;
  g433PairDeadlineMs = 0;
}


// =============================================================
// SUPER 模式（由 433 SUPER 按鍵觸發）
//  - 立即送出：暫存位置22=0、設定溫=70
//  - 1小時後自動恢復：暫存位置22=5、設定溫=65
//  - 若關機（MODE=0）則立即送：暫存位置22=5，並取消倒數
// =============================================================
static const uint16_t REG_TMP22          = 22;    // 暫存位置 22 (=0x0016)
static const uint16_t TMP22_SUPER_VAL    = 0;     // SUPER 期間
static const uint16_t TMP22_DEFAULT_VAL  = 5;     // 1 小時後回復預設
static const uint16_t TMP22_OFF_VAL      = 5;     // 關機時強制回復
static const int      SUPER_TEMP_C       = 70;
static const int      SUPER_DEFAULT_TEMP_C = 65;
static const unsigned long SUPER_HOLD_MS = 3600000UL; // 1 hour

static bool superPending = false;
static unsigned long superDueMs = 0;

// SUPER / 殺菌：回復用的「使用者原本設定溫」(RAM，重開就沒了)
static int  superPrevTemp = 65;
static bool superPrevValid = false;

static int  sterPrevTemp = 65;
static bool sterPrevValid = false;


// 手動殺菌（433 KEY4=0x3）：立即 ON + 70°C，維持 1 小時後回復
static bool          sterManualPending = false;
static unsigned long sterManualDueMs   = 0;
static int           sterManualPrevTemp = 65;
static bool          sterManualPrevValid = false;
static int           sterManualPrevMode = 0; // 0=OFF,1=ON

static void cancelSterManual(){
  sterManualPending = false;
  sterManualPrevValid = false;
}

static void startSterManual(){
  // 第一次啟動才備份，避免連按把備份覆蓋成 65
  if (!sterManualPending) {
    sterManualPrevTemp  = targetTemp;
    sterManualPrevValid = (sterManualPrevTemp >= 40 && sterManualPrevTemp <= 70);
    sterManualPrevMode  = (currentMode > 0) ? 1 : 0;
  }

  // 1) 先開機（主機比較會吃）
  sendBoardModeAutoKeepMask();
  currentMode = 1;
  gBoardSchedRaw[12] = BOARD_MODE_AUTO;
  gBoardSchedValid = true;
  gBoardSchedLastMs = millis();
  delay(2000); yield();

  // 2) 再設定 65°C（重送一次保險）
  targetTemp = 70;
  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
  delay(800); yield();
  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);

  sterManualPending = true;
  sterManualDueMs   = millis() + 3600000UL; // 1 小時
  Serial.println("[SterilizeManual] start 1h @70C");
}

static void restoreSterManual(){
  int restore = (sterManualPrevValid ? sterManualPrevTemp : SUPER_DEFAULT_TEMP_C);
  sterManualPrevValid = false;

  targetTemp = restore;
  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
  delay(800); yield();

  // 回復當時的 ON/OFF（若排程需要 ON，後面 checkSchedule(true) 會再補正）
  if (sterManualPrevMode == 0) {
    sendBoardModeOffKeepMask();
    currentMode = 0;
    gBoardSchedRaw[12] = BOARD_MODE_OFF;
    gBoardSchedValid = true;
    gBoardSchedLastMs = millis();
  }

  // 立即補正排程狀態
  checkSchedule(true);

  Serial.printf("[SterilizeManual] end, restore %dC, mode=%d\n", targetTemp, currentMode);
}

static void tickSterManual(){
  if (sterManualPending && (long)(millis() - sterManualDueMs) >= 0) {
    sterManualPending = false;
    restoreSterManual();
  }
}

static RCSwitch rf433;





void tickRTC() {
  uint32_t now = millis();
  uint32_t delta = now - rtcLastMs;
  if (delta < 1000) return;
  uint32_t addSec = delta / 1000;
  rtcLastMs += addSec * 1000;

  rtcSec += addSec;
  while (rtcSec >= 60) {
    rtcSec -= 60;
    uint16_t prevMin = rtcMinOfDay;
    rtcMinOfDay = (rtcMinOfDay + 1) % 1440; // 一天 1440 分

    // 每跨過午夜（23:59 → 00:00）就換一天
    if (rtcMinOfDay == 0 && prevMin == 1439) {
      if (rtcWday >= 1 && rtcWday <= 7) {
        rtcWday++;
        if (rtcWday > 7) rtcWday = 1;
      }
      // 日期也往前推一天（若已校正過日期）
      rtcAdvanceOneDay();
    }
  }
}


// 由手機時間校正（只用到時/分）
void rtcSetByHM(uint8_t hh, uint8_t mm, uint8_t wday = 0) {
  if (hh >= 24 || mm >= 60) return;
  rtcMinOfDay = hh * 60 + mm;
  rtcSec = 0;
  rtcLastMs = millis();
  if (wday >= 1 && wday <= 7) {
    rtcWday = wday;  // 1..7 = 一..日
  }
}


// ---- 排程結構：最多 9組 ----
#define MAX_SCHEDULES 9

struct TimeRange {
  bool     enabled;
  uint16_t startMin; // 0..1439
  uint16_t endMin;   // 0..1439
  uint8_t  wdayMask; // bit0=週一, bit1=週二 ... bit6=週日；0 = 未選（視為每天）
};


TimeRange gSchedules[MAX_SCHEDULES];

// EEPROM 位置（避開原有 0..160 區，以 192 起算）
static const int EEPROM_ADDR_SCHEDULE = 192;

// ===== 殺菌排程(排程0) + 遷移旗標 =====
static const int     EEPROM_ADDR_SCHEDULE_MIG = EEPROM_ADDR_SCHEDULE + (int)(sizeof(TimeRange) * 10); // 舊10組格式的尾端拿來放旗標
static const uint8_t SCHEDULE_MIG_MARK = 0xA7;

// 殺菌排程預設：週日 23:00~24:00（24:00 以 00:00 表示），預設關閉
static const uint16_t STER_START_MIN = 23 * 60; // 23:00
static const uint16_t STER_END_MIN   = 0;       // 00:00 (=24:00)
static const uint8_t  STER_WDAY_MASK = 0x40;    // bit6=週日
static const int      STER_TEMP_C    = 65;




void loadSchedules() {
  EEPROM.begin(EEPROM_BYTES);

  uint8_t mark = 0;
  EEPROM.get(EEPROM_ADDR_SCHEDULE_MIG, mark);

  if (mark != SCHEDULE_MIG_MARK) {
    // 從舊版(10組)遷移：插入「殺菌排程」到最前面，原本排程1→排程2、排程2→排程3...最多到排程9
    TimeRange old10[10];
    EEPROM.get(EEPROM_ADDR_SCHEDULE, old10);

    // 0: 殺菌排程（預設關）
    gSchedules[0].enabled  = false;
    gSchedules[0].startMin = STER_START_MIN;
    gSchedules[0].endMin   = STER_END_MIN;
    gSchedules[0].wdayMask = STER_WDAY_MASK;

    // 1..8: 原本 0..7（舊排程1..8）
    for (int i = 1; i < MAX_SCHEDULES; i++) {
      gSchedules[i] = old10[i - 1];
    }

    // 寫回新格式(9組) + 遷移旗標
    EEPROM.put(EEPROM_ADDR_SCHEDULE, gSchedules);
    mark = SCHEDULE_MIG_MARK;
    EEPROM.put(EEPROM_ADDR_SCHEDULE_MIG, mark);
    EEPROM.commit();
  } else {
    EEPROM.get(EEPROM_ADDR_SCHEDULE, gSchedules);
  }

  // 簡單驗證，避免第一次是亂碼
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (gSchedules[i].startMin >= 1440 || gSchedules[i].endMin >= 1440) {
      gSchedules[i].enabled  = false;
      gSchedules[i].startMin = 0;
      gSchedules[i].endMin   = 0;
    }
    if (i <= 3) gSchedules[i].wdayMask &= 0x7F;
  }

  // 若殺菌排程完全沒初始化（全 0），補預設：週日 23:00~00:00、預設關閉
  if (!gSchedules[0].enabled &&
      gSchedules[0].startMin == 0 &&
      gSchedules[0].endMin == 0 &&
      gSchedules[0].wdayMask == 0) {
    gSchedules[0].startMin = STER_START_MIN;
    gSchedules[0].endMin   = STER_END_MIN;
    gSchedules[0].wdayMask = STER_WDAY_MASK;
  }
}


void saveSchedules() {
  EEPROM.begin(EEPROM_BYTES);
  EEPROM.put(EEPROM_ADDR_SCHEDULE, gSchedules);
  EEPROM.commit();
}

// 轉成 "HH:MM"
String formatHM(uint16_t minOfDay) {
  if (minOfDay >= 1440) minOfDay = 0;
  uint8_t hh = minOfDay / 60;
  uint8_t mm = minOfDay % 60;
  char buf[6];
  sprintf(buf, "%02u:%02u", hh, mm);
  return String(buf);
}

// "HH:MM" → 0..1439（錯誤回 -1）
int parseHM(const String &s) {
  int colon = s.indexOf(':');
  if (colon < 0) return -1;
  int hh = s.substring(0, colon).toInt();
  int mm = s.substring(colon + 1).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
  return hh * 60 + mm;
}

// "1357" → bitmask（bit0=一, bit1=二 ... bit6=日）
uint8_t wdayStrToMask(const String &s) {
  uint8_t m = 0;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c >= '1' && c <= '7') {
      uint8_t bit = (uint8_t)(c - '1'); // '1'→0, '7'→6
      m |= (1 << bit);
    }
  }
  return m & 0x7F;
}

// bitmask → "1357"
String wdayMaskToStr(uint8_t m) {
  String s;
  m &= 0x7F;
  for (uint8_t bit = 0; bit < 7; bit++) {
    if (m & (1 << bit)) {
      s += String(bit + 1); // 0→"1"(週一)
    }
  }
  return s;
}






// 判斷時間是否落在一個區間內（支援跨日）
bool inRange(uint16_t T, uint16_t s, uint16_t e) {
  if (s == e) return false; // 視為無效
  if (s < e) {
    return (T >= s && T < e);
  } else {
    // 跨日：例如 22:00 → 02:00
    return (T >= s || T < e);
  }
}

static bool isSterScheduleActiveNow(){
  const TimeRange &tr = gSchedules[0];
  if (!tr.enabled) return false;

  uint8_t today = rtcWday;
  bool okToday = true;
  if (tr.wdayMask != 0 && today >= 1 && today <= 7) {
    uint8_t bit = (uint8_t)(1 << (today - 1));
    okToday = ((tr.wdayMask & bit) != 0);
  }
  if (!okToday) return false;
  return inRange(rtcMinOfDay, tr.startMin, tr.endMin);
}

static void estOnHeatStateUpdated(){
  if (!hostHeatValid) return;

  bool nowHeat = HEATING_NOW;
  if (!gEstHeatStateInit) {
    gEstHeatStateInit = true;
    gEstPrevHeating = false;
  }

  bool rising = (!gEstPrevHeating && nowHeat);
  gEstPrevHeating = nowHeat;

  if (!gEstCfg.enabled || !rising) return;
  if (currentMode == BOARD_MODE_OFF || currentMode == 0) return;
  if (superPending || sterManualPending) return;
  if (isSterScheduleActiveNow()) return;
  if (gEstHeatSeq >= 4) return;

  gEstHeatSeq++;
  estApplySeqTemp(gEstHeatSeq);
}

uint16_t crc16(const uint8_t* buf, uint16_t len);
void rs485TxEnable(bool en);

static void waitRs485BusQuiet() {
  uint32_t t0 = millis();
  while ((millis() - gRs485LastByteMs) < 25) {
    if (millis() - t0 > 200) break; // 最多等 200ms，避免卡死
    delay(1);
    yield();
  }
}

void send42Write16(uint16_t a, uint16_t v) {

  // ★新增：送出前先等總線安靜（避免撞到主機回包）
  waitRs485BusQuiet();

  uint8_t f[12]; uint8_t n=0;
  f[n++]=MY_ID; f[n++]=0x42;
  f[n++]=a>>8; f[n++]=a&0xFF;
  f[n++]=0; f[n++]=1; f[n++]=2;
  f[n++]=v & 0xFF; f[n++]=v >> 8;
  uint16_t c = crc16(f,n);
  f[n++]= c & 0xFF; f[n++]= c >> 8;

  for(int i=0;i<2;i++){
    rs485TxEnable(true);
    RS485.write(f,n);
    RS485.flush();
    delay(15);
    rs485TxEnable(false);
    delay(40);
  }
}

void send42Write16Pair(uint16_t startAddr, uint16_t v1, uint16_t v2) {
  // 連續兩個 16-bit 暫存位同一包送出，例如 reg19 + reg20
  waitRs485BusQuiet();

  uint8_t f[14]; uint8_t n=0;
  f[n++]=MY_ID; f[n++]=0x42;
  f[n++]=startAddr>>8; f[n++]=startAddr&0xFF;
  f[n++]=0; f[n++]=2; f[n++]=4;
  f[n++]=v1 & 0xFF; f[n++]=v1 >> 8;
  f[n++]=v2 & 0xFF; f[n++]=v2 >> 8;
  uint16_t c = crc16(f,n);
  f[n++]= c & 0xFF; f[n++]= c >> 8;

  for(int i=0;i<2;i++){
    rs485TxEnable(true);
    RS485.write(f,n);
    RS485.flush();
    delay(15);
    rs485TxEnable(false);
    delay(40);
  }
}

void send42Write16Block(uint16_t startAddr, const uint16_t* vals, uint8_t count) {
  if (!vals || count == 0) return;
  if (count > 32) count = 32;  // 安全上限

  waitRs485BusQuiet();

  uint8_t byteCount = (uint8_t)(count * 2);
  uint8_t f[5 + 64 + 2];
  uint8_t n = 0;
  f[n++]=MY_ID; f[n++]=0x42;
  f[n++]=startAddr>>8; f[n++]=startAddr&0xFF;
  f[n++]=0; f[n++]=count; f[n++]=byteCount;
  for (uint8_t i = 0; i < count; i++) {
    f[n++] = vals[i] & 0xFF;
    f[n++] = vals[i] >> 8;
  }
  uint16_t c = crc16(f,n);
  f[n++]= c & 0xFF; f[n++]= c >> 8;

  for(int i=0;i<2;i++){
    rs485TxEnable(true);
    RS485.write(f,n);
    RS485.flush();
    delay(15);
    rs485TxEnable(false);
    delay(40);
  }
}

static void sendBoardRegs7to20(uint8_t modeVal, uint8_t maskVal) {
  uint16_t vals[14];
  for (int i = 0; i < 12; i++) vals[i] = gBoardSchedRaw[i];
  vals[12] = modeVal;
  vals[13] = maskVal;
  send42Write16Block(BOARD_T1_START_ADDR, vals, 14);
}

static void writeBoardRegs7to18And20(uint8_t maskVal) {
  send42Write16Pair(7,  (uint16_t)gBoardSchedRaw[0],  (uint16_t)gBoardSchedRaw[1]);
  delay(120); yield();
  send42Write16Pair(9,  (uint16_t)gBoardSchedRaw[2],  (uint16_t)gBoardSchedRaw[3]);
  delay(120); yield();
  send42Write16Pair(11, (uint16_t)gBoardSchedRaw[4],  (uint16_t)gBoardSchedRaw[5]);
  delay(120); yield();
  send42Write16Pair(13, (uint16_t)gBoardSchedRaw[6],  (uint16_t)gBoardSchedRaw[7]);
  delay(120); yield();
  send42Write16Pair(15, (uint16_t)gBoardSchedRaw[8],  (uint16_t)gBoardSchedRaw[9]);
  delay(120); yield();
  send42Write16Pair(17, (uint16_t)gBoardSchedRaw[10], (uint16_t)gBoardSchedRaw[11]);
  delay(120); yield();
  send42Write16(BOARD_MASK_ADDR, maskVal);
  delay(120); yield();
}

static uint8_t boardMaskRawFromDevice(){
  return gBoardSchedValid ? (uint8_t)(gBoardSchedRaw[13] & 0x07) : 0;
}

static void sendBoardPackedModeWithMask(uint8_t modeVal, uint8_t maskVal) {
  // 依原廠規範：位址0x0013只寫 1 個 word，DATA1=模式、DATA2=啟用bit
  // low byte=mode, high byte=mask -> send42Write16(0x0013, 0xMMVV)
  const uint16_t packed = (uint16_t)modeVal | ((uint16_t)(maskVal & 0x07) << 8);
  send42Write16(BOARD_MODE_ADDR, packed);
  delay(120);
  yield();
}

static void sendBoardModeTimerWithMask(uint8_t maskVal) {
  sendBoardPackedModeWithMask(BOARD_MODE_TIMER, maskVal);
}

static void sendBoardModeAutoKeepMask() {
  sendBoardPackedModeWithMask(BOARD_MODE_AUTO, boardMaskRawFromDevice());
}

static void sendBoardModeOffKeepMask() {
  sendBoardPackedModeWithMask(BOARD_MODE_OFF, boardMaskRawFromDevice());
}

static inline bool boardSchedTimeValidRaw(uint8_t hhRaw, uint8_t mmRaw){
  if (hhRaw == 0xFF || mmRaw == 0xFF) return false;
  uint8_t hh = bcdToDec(hhRaw);
  uint8_t mm = bcdToDec(mmRaw);
  return (hh <= 23 && mm <= 59);
}

static inline uint16_t boardHmRawToMin(uint8_t hhRaw, uint8_t mmRaw){
  return (uint16_t)bcdToDec(hhRaw) * 60U + (uint16_t)bcdToDec(mmRaw);
}

static void boardSetRawSlot(int idx, int startMin, int endMin){
  if (idx < 0 || idx > 2) return;
  const int base = idx * 4;
  if (startMin < 0 || startMin >= 1440 || endMin < 0 || endMin >= 1440 || startMin == endMin) {
    gBoardSchedRaw[base + 0] = 0xFF;
    gBoardSchedRaw[base + 1] = 0xFF;
    gBoardSchedRaw[base + 2] = 0xFF;
    gBoardSchedRaw[base + 3] = 0xFF;
    return;
  }
  uint8_t sh = (uint8_t)(startMin / 60);
  uint8_t sm = (uint8_t)(startMin % 60);
  uint8_t eh = (uint8_t)(endMin / 60);
  uint8_t em = (uint8_t)(endMin % 60);
  gBoardSchedRaw[base + 0] = decToBcd(sh);
  gBoardSchedRaw[base + 1] = decToBcd(sm);
  gBoardSchedRaw[base + 2] = decToBcd(eh);
  gBoardSchedRaw[base + 3] = decToBcd(em);
}

struct SimpleRange {
  uint16_t s;
  uint16_t e;
};

static inline uint8_t normalizeWdayMask(uint8_t m){
  m &= 0x7F;
  return m ? m : 0x7F;
}

static inline bool wdayApplies(uint8_t m, uint8_t today){
  m = normalizeWdayMask(m);
  if (today < 1 || today > 7) return true;
  return (m & (uint8_t)(1 << (today - 1))) != 0;
}

static void rebuildBoardTimerPlanFromTriples(const bool enabled[3], const int startMin[3], const int endMin[3], const uint8_t wdayMask[3]){
  gBoardUnionCount = 0;
  gBoardMinuteEventCount = 0;

  auto addMinuteEvent = [](uint16_t minute, uint8_t daysMask, uint8_t setMask, uint8_t clearMask, bool sendTimer, bool sendOff){
    daysMask = normalizeWdayMask(daysMask);
    if (minute >= 1440) minute = 0;

    uint8_t idx = 0xFF;
    for (uint8_t k = 0; k < gBoardMinuteEventCount; k++) {
      if (gBoardMinuteEvents[k].minute == minute) { idx = k; break; }
    }
    if (idx == 0xFF) {
      if (gBoardMinuteEventCount >= 8) return;
      idx = gBoardMinuteEventCount++;
      gBoardMinuteEvents[idx].minute = minute;
      memset(gBoardMinuteEvents[idx].setMaskByDay, 0, sizeof(gBoardMinuteEvents[idx].setMaskByDay));
      memset(gBoardMinuteEvents[idx].clearMaskByDay, 0, sizeof(gBoardMinuteEvents[idx].clearMaskByDay));
      gBoardMinuteEvents[idx].sendTimerDays = 0;
      gBoardMinuteEvents[idx].sendOffDays = 0;
    }

    for (uint8_t d = 0; d < 7; d++) {
      if ((daysMask & (uint8_t)(1 << d)) == 0) continue;
      gBoardMinuteEvents[idx].setMaskByDay[d]   |= setMask;
      gBoardMinuteEvents[idx].clearMaskByDay[d] |= clearMask;
    }
    if (sendTimer) gBoardMinuteEvents[idx].sendTimerDays |= daysMask;
    if (sendOff)   gBoardMinuteEvents[idx].sendOffDays   |= daysMask;
  };

  for (int i = 0; i < 3; i++) {
    if (!enabled[i]) continue;
    int s = startMin[i];
    int e = endMin[i];
    if (s < 0 || s >= 1440 || e < 0 || e >= 1440 || s == e) continue;
    uint8_t bit = (uint8_t)(1 << i);
    uint8_t days = normalizeWdayMask(wdayMask[i]);
    uint16_t preStart = (uint16_t)((s + 1439) % 1440); // 定時鍵提早 1 分鐘
    addMinuteEvent(preStart, days, bit, 0, false, false);
    addMinuteEvent((uint16_t)e, days, 0, bit, false, false);
  }

  struct SimpleRangeDay {
    uint16_t s;
    uint16_t e;
  };

  for (uint8_t day = 1; day <= 7; day++) {
    SimpleRangeDay segs[6];
    uint8_t segCount = 0;

    for (int i = 0; i < 3; i++) {
      if (!enabled[i]) continue;
      if (!wdayApplies(wdayMask[i], day)) continue;
      int s = startMin[i];
      int e = endMin[i];
      if (s < 0 || s >= 1440 || e < 0 || e >= 1440 || s == e) continue;

      if (s < e) {
        segs[segCount++] = {(uint16_t)s, (uint16_t)e};
      } else {
        segs[segCount++] = {0, (uint16_t)e};
        segs[segCount++] = {(uint16_t)s, 1440};
      }
    }

    for (uint8_t i = 0; i < segCount; i++) {
      for (uint8_t j = i + 1; j < segCount; j++) {
        if (segs[j].s < segs[i].s) {
          SimpleRangeDay t = segs[i];
          segs[i] = segs[j];
          segs[j] = t;
        }
      }
    }

    SimpleRangeDay merged[6];
    uint8_t mergedCount = 0;
    for (uint8_t i = 0; i < segCount; i++) {
      if (mergedCount == 0) {
        merged[mergedCount++] = segs[i];
        continue;
      }
      SimpleRangeDay &last = merged[mergedCount - 1];
      if (segs[i].s <= last.e) {
        if (segs[i].e > last.e) last.e = segs[i].e;
      } else {
        merged[mergedCount++] = segs[i];
      }
    }

    if (mergedCount > 1 && merged[0].s == 0 && merged[mergedCount - 1].e == 1440) {
      uint16_t preStart = (uint16_t)((merged[mergedCount - 1].s + 1439) % 1440);
      addMinuteEvent(preStart, (uint8_t)(1 << (day - 1)), 0, 0, true, false);
      addMinuteEvent(merged[0].e >= 1440 ? 0 : merged[0].e, (uint8_t)(1 << (day - 1)), 0, 0, false, true);
      for (uint8_t i = 1; i + 1 < mergedCount; i++) {
        uint16_t preStart2 = (uint16_t)((merged[i].s + 1439) % 1440);
        addMinuteEvent(preStart2, (uint8_t)(1 << (day - 1)), 0, 0, true, false);
        addMinuteEvent(merged[i].e >= 1440 ? 0 : merged[i].e, (uint8_t)(1 << (day - 1)), 0, 0, false, true);
      }
    } else {
      for (uint8_t i = 0; i < mergedCount; i++) {
        uint16_t preStart = (uint16_t)((merged[i].s + 1439) % 1440);
        uint16_t endm = (merged[i].e >= 1440) ? 0 : merged[i].e;
        addMinuteEvent(preStart, (uint8_t)(1 << (day - 1)), 0, 0, true, false);
        addMinuteEvent(endm,      (uint8_t)(1 << (day - 1)), 0, 0, false, true);
      }
    }
  }

  for (uint8_t i = 0; i < gBoardMinuteEventCount; i++) {
    for (uint8_t j = i + 1; j < gBoardMinuteEventCount; j++) {
      if (gBoardMinuteEvents[j].minute < gBoardMinuteEvents[i].minute) {
        BoardMinuteEvent t = gBoardMinuteEvents[i];
        gBoardMinuteEvents[i] = gBoardMinuteEvents[j];
        gBoardMinuteEvents[j] = t;
      }
    }
  }
}

static void rebuildBoardTimerPlanFromSaved(){
  bool en[3] = {false,false,false};
  int s[3] = {-1,-1,-1};
  int e[3] = {-1,-1,-1};
  uint8_t wm[3] = {0,0,0};
  for (int i = 0; i < 3; i++) {
    const TimeRange &tr = gSchedules[i + 1];
    en[i] = tr.enabled;
    s[i] = tr.startMin;
    e[i] = tr.endMin;
    wm[i] = (uint8_t)(tr.wdayMask & 0x7F);
  }
  rebuildBoardTimerPlanFromTriples(en, s, e, wm);
}

static void rebuildBoardTimerPlanFromBoardRaw(){
  bool en[3] = {false,false,false};
  int s[3] = {-1,-1,-1};
  int e[3] = {-1,-1,-1};
  uint8_t wm[3] = {0,0,0};

  for (int i = 0; i < 3; i++) {
    wm[i] = (uint8_t)(gSchedules[i + 1].wdayMask & 0x7F);
  }

  if (!gBoardSchedValid) {
    rebuildBoardTimerPlanFromTriples(en, s, e, wm);
    return;
  }

  for (int i = 0; i < 3; i++) {
    int base = i * 4;
    bool tOk = boardSchedTimeValidRaw(gBoardSchedRaw[base + 0], gBoardSchedRaw[base + 1]) &&
               boardSchedTimeValidRaw(gBoardSchedRaw[base + 2], gBoardSchedRaw[base + 3]);
    bool enabled = ((gBoardSchedRaw[13] >> i) & 0x01) != 0;
    if (enabled && tOk) {
      en[i] = true;
      s[i] = boardHmRawToMin(gBoardSchedRaw[base + 0], gBoardSchedRaw[base + 1]);
      e[i] = boardHmRawToMin(gBoardSchedRaw[base + 2], gBoardSchedRaw[base + 3]);
    }
  }
  rebuildBoardTimerPlanFromTriples(en, s, e, wm);
}

static bool boardTimerWindowActiveNow(uint16_t nowMin){
  uint8_t today = (rtcWday >= 1 && rtcWday <= 7) ? rtcWday : 0;
  for (int i = 0; i < 3; i++) {
    const TimeRange &tr = gSchedules[i + 1];
    if (!tr.enabled) continue;
    if (tr.startMin >= 1440 || tr.endMin >= 1440 || tr.startMin == tr.endMin) continue;
    if (today && !wdayApplies((uint8_t)(tr.wdayMask & 0x7F), today)) continue;
    if (inRange(nowMin, tr.startMin, tr.endMin)) return true;
  }
  return false;
}

static void boardWritePair(uint16_t addr, uint8_t lo, uint8_t hi){
  // reg7/8、9/10... 是「兩個連續暫存位」，不是同一個 16-bit 暫存位的高低位元組
  // 所以這裡必須一次寫兩個 register：addr=小時、addr+1=分鐘
  send42Write16Pair(addr, (uint16_t)lo, (uint16_t)hi);
  delay(120);
  yield();
}

static void syncBoardSchedulesFromPhone(){
  uint8_t mask = 0;
  gBoardSchedRaw[13] = 0;

  for (int i = 0; i < 3; i++) {
    const TimeRange &tr = gSchedules[i + 1];
    bool hasTime = (tr.startMin < 1440 && tr.endMin < 1440 && tr.startMin != tr.endMin);
    int s = hasTime ? tr.startMin : -1;
    int e = hasTime ? tr.endMin   : -1;

    boardSetRawSlot(i, s, e);
    if (tr.enabled && hasTime) mask |= (uint8_t)(1 << i);
  }

  gBoardSchedRaw[12] = currentMode >= 0 ? (uint8_t)currentMode : BOARD_MODE_OFF;
  gBoardSchedRaw[13] = mask;
  gBoardSchedValid = true;
  gBoardSchedLastMs = millis();

  // reg7~18 與 reg20 先分段寫回設備端；reg19 模式不在儲存排程時改動
  // fix13 的 reg7~20 整包寫入會造成設備端無反應/存不了，這裡改回穩定寫法。
  writeBoardRegs7to18And20(mask);

  rebuildBoardTimerPlanFromSaved();
}


static void cancelSuper() { superPending = false; }

static void startSuper() {
  // 立即進入 SUPER：22=0、溫度=70，並啟動 1 小時倒數
  // 只在「第一次進入 SUPER」時備份，避免你按 SUPER 第二次時把備份變成 70
  if (!superPending) {
    superPrevTemp  = targetTemp;
    superPrevValid = (superPrevTemp >= 40 && superPrevTemp <= 70);
  }



  send42Write16(REG_TMP22, TMP22_SUPER_VAL);
  delay(500);
  targetTemp = SUPER_TEMP_C;
  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);

  superPending = true;
  superDueMs = millis() + SUPER_HOLD_MS;
}

static void restoreSuperDefault() {
  // 1 小時後回復：22=5、溫度=65
  send42Write16(REG_TMP22, TMP22_DEFAULT_VAL);
  delay(1000);
  int restore = (superPrevValid ? superPrevTemp : SUPER_DEFAULT_TEMP_C);
  superPrevValid = false; // 用完就清掉
  targetTemp = restore;

  send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
}

static void tickSuper() {
  if (superPending && (long)(millis() - superDueMs) >= 0) {
    restoreSuperDefault();
    superPending = false;
    Serial.printf("[SUPER] auto-restore: TMP22=5, TEMP=%d\n", targetTemp);

  }
}



// 每分鐘檢查一次排程 → 決定是否自動開/關機
void checkSchedule(bool force = false) {
  static int16_t lastMinChecked = -1;
  if (!force && rtcMinOfDay == lastMinChecked) return;
  lastMinChecked = rtcMinOfDay;

  uint8_t today = rtcWday; // 1..7，一..日；0 = 未設定

  bool sterActive = false;
  {
    const TimeRange &tr = gSchedules[0];
    if (tr.enabled) {
      bool okToday = true;
      if (tr.wdayMask != 0 && today >= 1 && today <= 7) {
        uint8_t bit = (1 << (today - 1));
        okToday = ((tr.wdayMask & bit) != 0);
      }
      if (okToday && inRange(rtcMinOfDay, tr.startMin, tr.endMin)) {
        sterActive = true;
      }
    }
  }

  static bool lastSterActive = false;

  if (sterActive != lastSterActive) {
    lastSterActive = sterActive;

    if (sterActive) {
      sendBoardModeAutoKeepMask();
      currentMode = BOARD_MODE_AUTO;
      gBoardSchedRaw[12] = BOARD_MODE_AUTO;
      gBoardSchedValid = true;
      gBoardSchedLastMs = millis();
      delay(2000); yield();

      sterPrevTemp  = targetTemp;
      sterPrevValid = (sterPrevTemp >= 40 && sterPrevTemp <= 70);

      targetTemp = STER_TEMP_C;
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
      delay(800); yield();
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);

      Serial.printf("[Sterilize] %02u:%02u -> start, set %dC & ON\n",
                    rtcMinOfDay/60, rtcMinOfDay%60, STER_TEMP_C);
      return;
    } else {
      int restore = (sterPrevValid ? sterPrevTemp : SUPER_DEFAULT_TEMP_C);
      sterPrevValid = false;
      targetTemp = restore;
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
      delay(800); yield();

      if (boardTimerWindowActiveNow(rtcMinOfDay)) {
        sendBoardModeTimerWithMask(gBoardSchedRaw[13]);
        currentMode = BOARD_MODE_TIMER;
        gBoardSchedRaw[12] = BOARD_MODE_TIMER;
        gBoardSchedValid = true;
        gBoardSchedLastMs = millis();
        Serial.printf("[Sterilize] %02u:%02u -> end, restore %dC & TIMER (mask=0x%02X)\n",
                      rtcMinOfDay/60, rtcMinOfDay%60, targetTemp, gBoardSchedRaw[13]);
        return;
      }

      Serial.printf("[Sterilize] %02u:%02u -> end, restore %dC\n",
                    rtcMinOfDay/60, rtcMinOfDay%60, targetTemp);
    }
  }

  if (sterActive) return;

  if (force) {
    return;
  }

  for (uint8_t i = 0; i < gBoardMinuteEventCount; i++) {
    if (gBoardMinuteEvents[i].minute != rtcMinOfDay) continue;

    uint8_t dayBit = 0;
    if (today >= 1 && today <= 7) {
      uint8_t dayIdx = (uint8_t)(today - 1);
      dayBit = (uint8_t)(1 << dayIdx);
    }

    // 重要：reg20 視為「設備端已儲存的啟用設定」，不是執行中暫態。
    // 因此定時開始/結束只切 reg19，不在事件觸發時改寫 reg20，避免 ERR 後把 1~3 段啟用狀態洗掉。
    if (dayBit && (gBoardMinuteEvents[i].sendTimerDays & dayBit)) {
      const uint8_t mask = boardTimerMaskFromSaved();
      if (mask != 0) {
        gBoardSchedRaw[13] = mask;
        sendBoardModeTimerWithMask(mask);
        currentMode = BOARD_MODE_TIMER;
        gBoardSchedRaw[12] = BOARD_MODE_TIMER;
        gBoardSchedValid = true;
        gBoardSchedLastMs = millis();
        Serial.printf("[BoardTimer] %02u:%02u -> TIMER (mask=0x%02X)\n", rtcMinOfDay/60, rtcMinOfDay%60, mask);
      } else {
        Serial.printf("[BoardTimer] %02u:%02u -> TIMER skipped (mask=0x00)\n", rtcMinOfDay/60, rtcMinOfDay%60);
      }
    }

    if (dayBit && (gBoardMinuteEvents[i].sendOffDays & dayBit)) {
      sendBoardModeOffKeepMask();
      currentMode = BOARD_MODE_OFF;
      gBoardSchedRaw[12] = BOARD_MODE_OFF;
      gBoardSchedValid = true;
      gBoardSchedLastMs = millis();
      Serial.printf("[BoardTimer] %02u:%02u -> OFF\n", rtcMinOfDay/60, rtcMinOfDay%60);
    }
    return;
  }

  static bool lastExtraActive = false;
  bool extraActive = false;
  for (int i = 4; i < MAX_SCHEDULES; i++) {
    const TimeRange &tr = gSchedules[i];
    if (!tr.enabled) continue;

    uint8_t wmask = (uint8_t)(tr.wdayMask & 0x7F);
    if (wmask != 0 && today >= 1 && today <= 7) {
      uint8_t bit = (1 << (today - 1));
      if ((wmask & bit) == 0) continue;
    }

    if (tr.startMin >= 1440 || tr.endMin >= 1440 || tr.startMin == tr.endMin) continue;
    if (inRange(rtcMinOfDay, tr.startMin, tr.endMin)) { extraActive = true; break; }
  }

  if (extraActive != lastExtraActive) {
    lastExtraActive = extraActive;
    if (extraActive) {
      sendBoardModeAutoKeepMask();
      currentMode = BOARD_MODE_AUTO;
      gBoardSchedRaw[12] = BOARD_MODE_AUTO;
      gBoardSchedValid = true;
      gBoardSchedLastMs = millis();
      Serial.printf("[ExtraSchedule] %02u:%02u -> AUTO ON\n", rtcMinOfDay/60, rtcMinOfDay%60);
    } else {
      if (boardTimerWindowActiveNow(rtcMinOfDay)) {
        sendBoardModeTimerWithMask(gBoardSchedRaw[13]);
        currentMode = BOARD_MODE_TIMER;
        gBoardSchedRaw[12] = BOARD_MODE_TIMER;
        gBoardSchedValid = true;
        gBoardSchedLastMs = millis();
        Serial.printf("[ExtraSchedule] %02u:%02u -> TIMER (board active)\n", rtcMinOfDay/60, rtcMinOfDay%60);
      } else {
        sendBoardModeOffKeepMask();
        currentMode = BOARD_MODE_OFF;
        gBoardSchedRaw[12] = BOARD_MODE_OFF;
        gBoardSchedValid = true;
        gBoardSchedLastMs = millis();
        Serial.printf("[ExtraSchedule] %02u:%02u -> OFF\n", rtcMinOfDay/60, rtcMinOfDay%60);
      }
    }
  }
}


// =============================================================
// Wi-Fi Events
// =============================================================
static void attachWifiEvents() {
  EH_GOTIP = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&) {
    WIFI_GOTIP = true;
  });
  EH_DISC = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected&) {
    WIFI_FAIL = true;
  });
}

static bool waitConnect(unsigned long safety_ms) {
  unsigned long t0 = millis();
  while (!WIFI_GOTIP && !WIFI_FAIL) {
    if (safety_ms && millis() - t0 > safety_ms) break;
    yield();
    delay(1);
  }
  return WIFI_GOTIP && WiFi.status() == WL_CONNECTED;
}

// =============================================================
// EEPROM Utilities
// =============================================================
void saveWiFi(const String& ssid, const String& pass) {
  EEPROM.begin(EEPROM_BYTES);
  for (int i=0;i<32;i++)  EEPROM.write(i,      i < (int)ssid.length() ? ssid[i] : 0);
  for (int i=0;i<64;i++)  EEPROM.write(32 + i, i < (int)pass.length() ? pass[i] : 0);
  EEPROM.commit();
}

void clearWiFi() {
  EEPROM.begin(EEPROM_BYTES);
  for (int i=0;i<96;i++) EEPROM.write(i, 0);
  EEPROM.commit();
}

bool loadApPass(char* out, size_t cap){
  EEPROM.begin(EEPROM_BYTES);
  if(EEPROM.read(AP_PASS_FLAG)!=0xA5){ if(cap) out[0]=0; return false; }
  size_t n = min((size_t)32, cap>0?cap-1:0);
  for(size_t i=0;i<n;i++) out[i]=EEPROM.read(AP_PASS_OFF+i);
  if(cap) out[n]=0;
  size_t L = strnlen(out, cap?cap-1:0);
  return L>0;
}
void saveApPass(const String& pass){
  EEPROM.begin(EEPROM_BYTES);
  for(int i=0;i<32;i++) EEPROM.write(AP_PASS_OFF+i, i<(int)pass.length()?pass[i]:0);
  EEPROM.write(AP_PASS_FLAG, 0xA5);
  EEPROM.commit();
  memset(AP_PASS_BUF,0,sizeof(AP_PASS_BUF));
  strncpy(AP_PASS_BUF, pass.c_str(), sizeof(AP_PASS_BUF)-1);
  AP_PASS_SET = pass.length()>0;
}
void clearApPass(){
  EEPROM.begin(EEPROM_BYTES);
  for(int i=0;i<32;i++) EEPROM.write(AP_PASS_OFF+i,0);
  EEPROM.write(AP_PASS_FLAG,0);
  EEPROM.commit();
  memset(AP_PASS_BUF,0,sizeof(AP_PASS_BUF));
  AP_PASS_SET = false;
}

bool loadWiFi(char* ssid, char* pass) {
  EEPROM.begin(EEPROM_BYTES);
  for (int i=0;i<32;i++) ssid[i] = EEPROM.read(i);
  ssid[31] = 0;
  for (int i=0;i<64;i++) pass[i] = EEPROM.read(32 + i);
  pass[63] = 0;
  return strlen(ssid) > 0;
}

bool checkDoubleReset() {
  EEPROM.begin(EEPROM_BYTES);
  uint8_t flag = EEPROM.read(100);
  if (flag == 0xA5) {
    EEPROM.write(100, 0);
    EEPROM.commit();
    return true;
  } else {
    EEPROM.write(100, 0xA5);
    EEPROM.commit();
    delay(3000);
    EEPROM.begin(EEPROM_BYTES);
    EEPROM.write(100, 0);
    EEPROM.commit();
    return false;
  }
}

// =============================================================
// CRC / RS485
// =============================================================
uint16_t crc16(const uint8_t* buf, uint16_t len) {
  uint16_t c = 0xFFFF;
  for (uint16_t i=0;i<len;i++) {
    c ^= buf[i];
    for (int b=0;b<8;b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}

void rs485TxEnable(bool en) { digitalWrite(PIN_REDE, en ? HIGH : LOW); }


// =============================================================
// RS485 Parse Host Status（新手勢邏輯）
// =============================================================
static void triggerFactoryResetByGesture() {
    // 手勢只做「AP 密碼清空」，不動 Wi-Fi / 已儲存 IP
  Serial.println("[Gesture] AP 密碼清空觸發（Wi-Fi / IP 保留）");
  //clearWiFi();
  clearApPass();

  // 433：快速手勢清空時，也清空已配對資料並重新配對
  rf433ClearPair();
  beepOnce(2600, 120);
  rf433BeginPairing(60000UL);

  //NEED_CLEAR_PWA_IP = true;
  
  // 2. 透過既有的延後動作，把 AP 改成開放式
  //    （重用 hDisableApPass 用的 AP_ACT_DISABLE_PASS 邏輯）
  PENDING_AP_ACTION = AP_ACT_DISABLE_PASS;
  PENDING_AP_TS     = millis() + 500;
  
  // 3. 手勢狀態歸零，避免殘留
  PATTERN_STAGE = 0;
  LAST_TEMP_VALID = false;
  SW_PHASE = SW_IDLE; SW_LAST = -1; SW_T0 = 0;
}

void parseHostStatus(const uint8_t* p, size_t n) {
  if (n < 10 || p[1] != 0x10) return;

  uint16_t addr = (p[2] << 8) | p[3];
  uint8_t  bc   = p[6];
  if (7 + bc + 2 > n) return;

  const uint8_t* d = &p[7];

  // 任何涵蓋 reg37 的封包都可更新異常位元，避免被特定 addr 格式綁死。
  if (HOST_FAULT_ADDR >= addr && HOST_FAULT_ADDR < (uint16_t)(addr + bc)) {
    hostFaultReg   = d[HOST_FAULT_ADDR - addr] & 0xFF;
    hostFaultValid = true;
  }
   
     // ===== 加熱判定：抓位址39(0x0027) 的 bit0 =====
  if (HEAT_ADDR >= addr && HEAT_ADDR < (uint16_t)(addr + bc)) {
    uint8_t v = d[HEAT_ADDR - addr];
    hostHeatReg   = v;
    HEATING_NOW   = ((v & 0x01) != 0);
    hostHeatValid = true;
    HEAT_LAST_MS  = millis();
  }

  // ===== 機板時間：位址 1..6（addr=0x0001, bc=0x06）=====
  if (addr == 0x0001 && bc >= 6) {
    memcpy(gBoardTimeRaw, d, 6);     // reg1..reg6
    gBoardTimeValid = true;
    gBoardTimeLastMs = millis();
    return;
  }

    // ===== 機板定時：位址 7..20（addr=0x0007, bc=0x0E）=====
  if (addr == 0x0007 && bc >= 14) {
    memcpy(gBoardSchedRaw, d, 14);   // reg7..reg20
    gBoardSchedValid = true;
    gBoardSchedLastMs = millis();

    if (MODE_ADDR >= addr && MODE_ADDR < (uint16_t)(addr + bc)) {
      currentMode = d[MODE_ADDR - addr];
    }

    rebuildBoardTimerPlanFromSaved();
    return;
  }


  // ===== 從主機狀態包同步「模式/設定溫度」=====
  if (MODE_ADDR >= addr && MODE_ADDR < (uint16_t)(addr + bc)) {
    currentMode = d[MODE_ADDR - addr];
  }
  if (TEMP_ADDR_WRITE >= addr && TEMP_ADDR_WRITE < (uint16_t)(addr + bc)) {
    targetTemp = d[TEMP_ADDR_WRITE - addr];
  }

  if (HEAT_ADDR >= addr && HEAT_ADDR < (uint16_t)(addr + bc)) {
    estOnHeatStateUpdated();
  }

  // ===== 只監控溫度：addr=0x0015 的 d[10]（含手勢）=====
  static int TEMP_PRINT_LAST = -1;

  if (addr == 0x0015 && bc > 10) {
    uint8_t val = d[10];
    unsigned long now = millis();

    LAST_TEMP_RAW   = val;
    LAST_TEMP_VALID = true;

    if (TEMP_PRINT_LAST != val) {
      TEMP_PRINT_LAST = val;
      Serial.printf("[Temp] set=%u\n", val);
    }

    // === 新手勢偵測：20 秒內 40/41 → ≥44 → 回到 ≤41 ===
    if (SW_PHASE == SW_IDLE) {
      if (val == 40 || val == SW_START_TEMP) { // 40 或 41
        SW_PHASE = SW_UP;
        SW_T0    = now;
        SW_LAST  = val;
        Serial.printf("[Gesture] START (val=%u)\n", val);
      }
      return;
    }

    if (now - SW_T0 > SWEEP_WINDOW_MS) {
      Serial.println("[Gesture] TIMEOUT → reset");
      SW_PHASE = SW_IDLE;
      SW_LAST  = -1;
      SW_T0    = 0;
      return;
    }

    if (SW_PHASE == SW_UP && val >= SW_PEAK_TEMP) {
      SW_PHASE = SW_DOWN;
      Serial.printf("[Gesture] PEAK (>= %u, val=%u)\n", SW_PEAK_TEMP, val);
    }

    if (SW_PHASE == SW_DOWN && val <= SW_END_TEMP) {
      Serial.printf("[Gesture] RETURN (<= %u, val=%u) → 執行清空\n", SW_END_TEMP, val);
      triggerFactoryResetByGesture();
      SW_PHASE = SW_IDLE;
      SW_LAST  = -1;
      SW_T0    = 0;
    }

    return;
  }

  // ===== 主機異常狀態：位址 37 (=0x0025) =====
  if (addr == HOST_FAULT_ADDR && bc >= 1) return;

  // ===== 舊 fallback（保留）=====
  if (addr == TEMP_ADDR_FALLBACK && bc >= 2) {
    LAST_TEMP_RAW   = (uint16_t)(d[0] | (d[1] << 8));
    LAST_TEMP_VALID = true;
    // 不 return，讓下面也能繼續判斷其他包
  }

  // ===== addr=0x0023：抓 tUp（異常位元已由上面的 reg37 通用解析處理）=====
  if (addr == 0x0023) {
    // tUp
    for (int i = 0; i < 3 && i < bc; i++) {
      int v = d[i];
      if (v >= 5 && v <= 99) { tUp = v; break; }
    }

    if (bc >= 3) {
      heaterHealthTick((int)tUp);
    }
    return;
  }
}


void onFrame(const uint8_t* p, size_t n) {
  if (n < 4) return;
  uint16_t c = p[n-2] | (p[n-1]<<8);
  if (c == crc16(p,n-2)) parseHostStatus(p,n);
}

// =============================================================
// Wi-Fi Connect
// =============================================================
TargetAP findTargetAP(const String& ssid) {
  TargetAP t = {0};
  t.ok=false; t.rssi=-1000;
  int n = WiFi.scanNetworks();
  for(int i=0;i<n;i++){
    if(WiFi.SSID(i)==ssid){
      int r=WiFi.RSSI(i);
      if(r>t.rssi){
        memcpy(t.bssid,WiFi.BSSID(i),6);
        t.channel=WiFi.channel(i);
        t.rssi=r;
        t.ok=true;
      }
    }
    yield();
  }
  return t;
}

bool chooseStaticAndConnect(const char* ssid, const char* pass, IPAddress* out) {
  TargetAP tap = findTargetAP(ssid);
  if(!tap.ok) return false;

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(150);
  WiFi.config(0U,0U,0U);
  WiFi.begin(ssid,pass,tap.channel,tap.bssid,true);
  WIFI_GOTIP=false; WIFI_FAIL=false;
  if(!waitConnect(4000)){
    WiFi.disconnect(true);
    WiFi.persistent(true);
    return false;
  }

  IPAddress base = WiFi.localIP();
  IPAddress gw   = WiFi.gatewayIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress dns  = WiFi.dnsIP();
  uint8_t net0=base[0], net1=base[1], net2=base[2];

  WiFi.disconnect(false);
  delay(120);
  for(int last=225; last<=245; ++last){
    IPAddress ip(net0,net1,net2,last);
    WiFi.config(ip,gw,mask,dns);
    WiFi.begin(ssid,pass,tap.channel,tap.bssid,true);
    WIFI_GOTIP=false; WIFI_FAIL=false;
    if(waitConnect(2500)){
  if(out) *out = ip;
  LAST_STA_IP       = ip;
  LAST_STA_IP_VALID = true;
  WiFi.persistent(true);
  return true;
}

    WiFi.disconnect(false);
    delay(50);
  }
  WiFi.persistent(true);
  return false;
}

// =============================================================
// 共用訊息頁
// =============================================================
String makeInfoPage(const String& title, const String& body, bool doClearIp, unsigned autoMs = 3000) {
  String html = R"rawliteral(
<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>%TITLE%</title>
<style>
body{margin:0;background:#f7f9fc;font-family:"Noto Sans TC",sans-serif;color:#004aad;text-align:center;padding:34px}
h2{margin:0 0 16px;font-size:22px}
.msg{white-space:pre-line;font-size:15px;line-height:1.55;margin:0 auto;max-width:360px}
small{display:block;margin-top:14px;color:#666;font-size:12px}
.btn{margin-top:22px;padding:10px 20px;font-size:15px;border:0;border-radius:10px;background:#1877f2;color:#fff;cursor:pointer;font-weight:700}
</style></head><body>
<h2>%TITLE%</h2>
<div class="msg">%BODY%</div>
<button id="go" class="btn">返回 PWA 首頁</button>
<small>將於 %MS% 秒後回到 PWA 首頁。</small>
<script>
(function(){
  var sec = %MS%;
  function goHome(){
    try{
      if(window.opener && !window.opener.closed) window.opener.postMessage('gohome','*');
      else if(window.parent && window.parent!==window) window.parent.postMessage('gohome','*');
      %POSTMSG%
    }catch(e){}
    try{ window.close(); }catch(e){}
  }
  document.getElementById('go').onclick = goHome;
  setTimeout(goHome, sec*1000);
})();
</script>
</body></html>
)rawliteral";
  html.replace("%TITLE%", title);
  html.replace("%BODY%", body);
  html.replace("%MS%", String(autoMs/1000));
  if (doClearIp) {
    html.replace("%POSTMSG%", "try{if(window.opener&&!window.opener.closed)window.opener.postMessage('saveip:','*');else if(window.parent&&window.parent!==window)window.parent.postMessage('saveip:','*');}catch(e){}");
  } else {
    html.replace("%POSTMSG%", "");
  }
  return html;
}

// =============================================================
// 時間同步共用（手機 / NTP）
// =============================================================
static void applyTimeAndPushToDevice(uint16_t yy, uint8_t mo, uint8_t dd, uint8_t hh, uint8_t mm, uint8_t wday) {
  if (yy >= 2020 && yy <= 2099 && mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31) {
    rtcYear  = yy;
    rtcMonth = mo;
    rtcDay   = dd;
  }

  rtcSetByHM(hh, mm, wday);

  const uint8_t hhBcd = decToBcd(hh);
  const uint8_t mmBcd = decToBcd(mm);
  const uint16_t hmWord = (uint16_t)hhBcd | ((uint16_t)mmBcd << 8);
  send42Write16(0x0005, hmWord);
}

static bool queryNtpUtc(const char* host, time_t* outUtc) {
  if (!host || !host[0] || !outUtc) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  IPAddress ip;
  if (!WiFi.hostByName(host, ip)) {
    Serial.printf("[NTP] DNS fail: %s\n", host);
    return false;
  }

  uint8_t pkt[48];
  memset(pkt, 0, sizeof(pkt));
  pkt[0] = 0b11100011;
  pkt[1] = 0;
  pkt[2] = 6;
  pkt[3] = 0xEC;
  pkt[12] = 49;
  pkt[13] = 0x4E;
  pkt[14] = 49;
  pkt[15] = 52;

  while (NTP_UDP.parsePacket() > 0) {
    uint8_t dump[48];
    NTP_UDP.read(dump, sizeof(dump));
    yield();
  }

  if (!NTP_UDP.beginPacket(ip, 123)) {
    Serial.printf("[NTP] beginPacket fail: %s\n", host);
    return false;
  }
  NTP_UDP.write(pkt, sizeof(pkt));
  if (!NTP_UDP.endPacket()) {
    Serial.printf("[NTP] endPacket fail: %s\n", host);
    return false;
  }

  unsigned long t0 = millis();
  while (millis() - t0 < NTP_QUERY_TIMEOUT_MS) {
    int cb = NTP_UDP.parsePacket();
    if (cb >= 48) {
      NTP_UDP.read(pkt, sizeof(pkt));
      uint32_t secs1900 = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) | ((uint32_t)pkt[42] << 8) | (uint32_t)pkt[43];
      if (secs1900 >= 2208988800UL) {
        *outUtc = (time_t)(secs1900 - 2208988800UL);
        return true;
      }
      break;
    }
    delay(10);
    yield();
  }

  Serial.printf("[NTP] timeout: %s\n", host);
  return false;
}

static bool syncTimeFromNtpServer(const char* host) {
  time_t utc = 0;
  if (!queryNtpUtc(host, &utc)) return false;

  time_t localTs = utc + TZ_OFFSET_SEC;
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  gmtime_r(&localTs, &tmv);

  const uint16_t yy = (uint16_t)(tmv.tm_year + 1900);
  const uint8_t  mo = (uint8_t)(tmv.tm_mon + 1);
  const uint8_t  dd = (uint8_t)tmv.tm_mday;
  const uint8_t  hh = (uint8_t)tmv.tm_hour;
  const uint8_t  mm = (uint8_t)tmv.tm_min;
  const uint8_t  wd = (tmv.tm_wday == 0) ? 7 : (uint8_t)tmv.tm_wday;

  if (yy < 2020 || yy > 2099 || mo < 1 || mo > 12 || dd < 1 || dd > 31 || hh > 23 || mm > 59) {
    Serial.printf("[NTP] bad local time from %s\n", host);
    return false;
  }

  applyTimeAndPushToDevice(yy, mo, dd, hh, mm, wd);

  NTP_HAS_SYNC = true;
  NTP_LAST_SYNC_MS = millis();
  NTP_LAST_TRY_MS  = NTP_LAST_SYNC_MS;
  memset(NTP_LAST_SOURCE, 0, sizeof(NTP_LAST_SOURCE));
  strncpy(NTP_LAST_SOURCE, host, sizeof(NTP_LAST_SOURCE) - 1);

  Serial.printf("[NTP] synced via %s -> %04u-%02u-%02u %02u:%02u\n",
                host, (unsigned)yy, (unsigned)mo, (unsigned)dd, (unsigned)hh, (unsigned)mm);
  return true;
}

static bool syncTimeFromNtpInOrder(const char* reason) {
  if (WiFi.status() != WL_CONNECTED) return false;

  unsigned long now = millis();
  if ((now - NTP_LAST_TRY_MS) < NTP_RETRY_GAP_MS) {
    return false;
  }
  NTP_LAST_TRY_MS = now;

  Serial.printf("[NTP] start sync (%s)\n", reason ? reason : "-");

  if (syncTimeFromNtpServer(NTP_SERVER_1)) return true;
  if (syncTimeFromNtpServer(NTP_SERVER_2)) return true;
  if (syncTimeFromNtpServer(NTP_SERVER_3)) return true;

  Serial.printf("[NTP] all failed (%s)\n", reason ? reason : "-");
  return false;
}

// =============================================================
// HTTP Handlers
// =============================================================
void handleApiHHReset();


// 手機時間校正：/api/time  (POST h=0..23&m=0..59[&wd=1..7])
// 這裡保留 D1 RTC 校時用途，但真正的設備端時間校正要同步寫到 reg5/reg6。
// reg5/reg6 實測為 BCD，所以送出前要先轉 BCD；畫面則等設備端後續實際回報。
void handleApiTime() {
  if (!server.hasArg("h") || !server.hasArg("m")) {
    addCors();
    server.send(400, "text/plain; charset=utf-8", "missing h or m");
    return;
  }

  int hh = server.arg("h").toInt();
  int mm = server.arg("m").toInt();
  int wd = 0;
  uint16_t yy = rtcYear;
  uint8_t  mo = rtcMonth;
  uint8_t  dd = rtcDay;

  if (server.hasArg("wd")) {
    wd = server.arg("wd").toInt();  // 1..7 = 週一..週日
  }

  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
    addCors();
    server.send(400, "text/plain; charset=utf-8", "bad time");
    return;
  }

  // 可選：同步日期（年/月/日）到 D1 RTC；不直接寫設備端日期。
  if (server.hasArg("y") && server.hasArg("mo") && server.hasArg("d")) {
    int yyi = server.arg("y").toInt();
    int moi = server.arg("mo").toInt();
    int ddi = server.arg("d").toInt();
    if (yyi >= 2020 && yyi <= 2099 && moi >= 1 && moi <= 12 && ddi >= 1 && ddi <= 31) {
      yy = (uint16_t)yyi;
      mo = (uint8_t)moi;
      dd = (uint8_t)ddi;
    }
  }

  uint8_t wday = 0;
  if (wd >= 1 && wd <= 7) {
    wday = (uint8_t)wd;
  }

  applyTimeAndPushToDevice(yy, mo, dd, (uint8_t)hh, (uint8_t)mm, wday);

  String out = "{\"ok\":true";
  out += ",\"target\":\"";
  if (hh < 10) out += "0";
  out += String(hh);
  out += ":";
  if (mm < 10) out += "0";
  out += String(mm);
  out += "\"}";
  addCors();
  server.send(200, "application/json", out);
}



// 共用：CORS Header，給 PWA 跨站呼叫 /api/... 用
void addCors() {
  // 允許所有來源（如果你之後要鎖公司網域，也可以改成 http://xxx.xxx）
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  // 給 Private Network Access 用（Chrome / Safari 對 192.168.x.x 之類的私有網段）
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}





// 排程讀取：/api/schedule (GET)
void handleApiScheduleGet() {
  String json = "{\"items\":[";
  for (int i = 0; i < MAX_SCHEDULES; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"enabled\":"; json += (gSchedules[i].enabled ? "true" : "false"); json += ",";
    json += "\"start\":\""; json += formatHM(gSchedules[i].startMin); json += "\",";
    json += "\"end\":\"";   json += formatHM(gSchedules[i].endMin);   json += "\",";
    json += "\"wday\":\"";  json += wdayMaskToStr((uint8_t)(gSchedules[i].wdayMask & 0x7F)); json += "\",";
    json += "\"act\":\"on\"";
    json += "}";
  }
  json += "]}";
  addCors();
  server.send(200, "application/json", json);
}


// 排程寫入：/api/schedule (POST)
// 0=殺菌排程
// 1~3=設備三段定時（寫入機板 reg7..20）
// 4~8=附加排程（定時送 AUTO 或 OFF）
void handleApiSchedulePost() {
  bool anyError = false;

  for (int i = 0; i < MAX_SCHEDULES; i++) {
    String idx = String(i);

    bool enabled = false;
    if (server.hasArg("en" + idx)) {
      String e = server.arg("en" + idx);
      enabled = (e == "1" || e == "true" || e == "on");
    }

    int startMin = gSchedules[i].startMin;
    int endMin   = gSchedules[i].endMin;
    uint8_t wMask = (uint8_t)(gSchedules[i].wdayMask & 0x7F);
    bool actionOff = (i >= 4) ? ((gSchedules[i].wdayMask & 0x80) != 0) : false;

    if (server.hasArg("s" + idx)) {
      int m = parseHM(server.arg("s" + idx));
      if (m >= 0) startMin = m; else anyError = true;
    }
    if (server.hasArg("e" + idx)) {
      int m = parseHM(server.arg("e" + idx));
      if (m >= 0) endMin = m; else anyError = true;
    }
    if (server.hasArg("w" + idx)) {
      wMask = wdayStrToMask(server.arg("w" + idx));
    }

    gSchedules[i].enabled  = enabled;
    gSchedules[i].startMin = startMin;
    gSchedules[i].endMin   = endMin;
    gSchedules[i].wdayMask = (uint8_t)(wMask & 0x7F);
  }

  saveSchedules();
  syncBoardSchedulesFromPhone();
  checkSchedule(true);

  addCors();
  if (anyError) {
    server.send(200, "application/json", "{\"ok\":true,\"warn\":\"some items invalid\"}");
  } else {
    server.send(200, "application/json", "{\"ok\":true}");
  }
}

static void getBoardSched(int idx, bool &enabled, int &startMin, int &endMin);

// ========= 機板排程 =========
// GET /api/boardsched
// 回傳：valid + items[3]（timer1~timer3）
void handleApiBoardSchedGet() {
  String json = "{\"valid\":";
  json += (gBoardSchedValid ? "true" : "false");
  json += ",\"ageMs\":";
  json += (gBoardSchedValid ? String((uint32_t)(millis() - gBoardSchedLastMs)) : "0");
  json += ",\"boardTime\":\"";
  json += boardTimeHM();
  json += "\"";
  json += ",\"mode\":";
  json += String((int)(gBoardSchedValid ? gBoardSchedRaw[12] : (currentMode >= 0 ? currentMode : 0)));
  json += ",\"mask\":";
  json += String((int)(gBoardSchedValid ? gBoardSchedRaw[13] : 0));
  json += ",\"items\":[";

  for (int i = 0; i < 3; i++) {
    if (i > 0) json += ",";
    bool en = false;
    int sMin = -1, eMin = -1;

    if (gBoardSchedValid) {
      getBoardSched(i, en, sMin, eMin);
    }

    bool active = false;
    if (en && sMin >= 0 && eMin >= 0) {
      active = (currentMode == BOARD_MODE_TIMER) && inRange(rtcMinOfDay, (uint16_t)sMin, (uint16_t)eMin);
    }

    json += "{";
    json += "\"enabled\":"; json += (en ? "true" : "false"); json += ",";
    json += "\"active\":";  json += (active ? "true" : "false"); json += ",";
    json += "\"start\":\"";  json += (sMin >= 0 ? formatHM(sMin) : "--:--"); json += "\",";
    json += "\"end\":\"";    json += (eMin >= 0 ? formatHM(eMin) : "--:--"); json += "\"";
    json += "}";
  }

  json += "]}";
  addCors();
  server.send(200, "application/json", json);
}

static void getBoardSched(int idx, bool &enabled, int &startMin, int &endMin){
  enabled  = false;
  startMin = -1;
  endMin   = -1;

  if (!gBoardSchedValid) return;
  if (idx < 0 || idx > 2) return;

  const uint8_t mask = gBoardSchedRaw[13];
  enabled = ((mask >> idx) & 0x01) != 0;

  const int base = idx * 4;
  const uint8_t sh_bcd = gBoardSchedRaw[base + 0];
  const uint8_t sm_bcd = gBoardSchedRaw[base + 1];
  const uint8_t eh_bcd = gBoardSchedRaw[base + 2];
  const uint8_t em_bcd = gBoardSchedRaw[base + 3];

  auto bcd2dec = [](uint8_t v)->int{
    if (v == 0xFF) return -1;
    int hi = (v >> 4) & 0x0F;
    int lo = v & 0x0F;
    if (lo > 9) return -1;
    return hi * 10 + lo;
  };

  int sh = bcd2dec(sh_bcd);
  int sm = bcd2dec(sm_bcd);
  int eh = bcd2dec(eh_bcd);
  int em = bcd2dec(em_bcd);

  auto okHM = [](int h, int m)->bool{
    if (h < 0 || m < 0) return false;
    if (h > 23 || m > 59) return false;
    return true;
  };

  if (okHM(sh, sm)) startMin = sh * 60 + sm;
  if (okHM(eh, em)) endMin   = eh * 60 + em;
}

void handleApiBoardSchedPost(){
  bool anyError = false;
  for (int i = 0; i < 3; i++) {
    String idx = String(i + 1);
    bool enabled = false;
    if (server.hasArg("en" + idx)) {
      String e = server.arg("en" + idx);
      enabled = (e == "1" || e == "true" || e == "on");
    }

    int startMin = 0;
    int endMin = 0;
    if (server.hasArg("s" + idx)) {
      int m = parseHM(server.arg("s" + idx));
      if (m >= 0) startMin = m; else anyError = true;
    }
    if (server.hasArg("e" + idx)) {
      int m = parseHM(server.arg("e" + idx));
      if (m >= 0) endMin = m; else anyError = true;
    }

    gSchedules[i + 1].enabled = enabled;
    gSchedules[i + 1].startMin = startMin;
    gSchedules[i + 1].endMin = endMin;
    if (server.hasArg("w" + idx)) gSchedules[i + 1].wdayMask = wdayStrToMask(server.arg("w" + idx));
  }

  saveSchedules();
  syncBoardSchedulesFromPhone();
  checkSchedule(true);

  addCors();
  if (anyError) server.send(200, "application/json", "{\"ok\":true,\"warn\":\"some items invalid\"}");
  else server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiHHReset(){
  // 清除基準資料
  gHeaterHealth.count   = 0;
  gHeaterHealth.stdSec  = 0;
  gHeaterHealth.lastSec = 0;
  gHeaterHealth.sumSec  = 0;
  gHeaterHealth.mark    = HEATER_HEALTH_MARK;

  // 清除量測狀態（避免卡在 measuring）
  HH_MEASURING = false;
  HH_LAST_TEMP = -100;

  heaterHealthSave();

  addCors();
  server.send(200, "application/json", "{\"ok\":true}");
}

// 設備狀態：回傳位址 37 的原始值（0~255）＋是否已成功讀取
void handleApiDevStatus() {
  
    const uint8_t pct = heaterHealthPct();
  const bool ready  = (gHeaterHealth.stdSec > 0 && gHeaterHealth.lastSec > 0);

  String j = "{";
  j += "\"raw\":"   + String(hostFaultReg);
  j += ",\"valid\":" + String(hostFaultValid ? "true" : "false");

    // 電熱管健康度參考
  j += ",\"hhStdSec\":"  + String(gHeaterHealth.stdSec);
  j += ",\"hhLastSec\":" + String(gHeaterHealth.lastSec);
  j += ",\"hhCount\":"   + String(gHeaterHealth.count);
  j += ",\"hhPct\":"     + String((int)pct);
  j += ",\"hhReady\":"   + String(ready ? "true" : "false");

  j += "}";
  addCors();
  server.send(200, "application/json", j);
}
// ★ 新增：回報最近一次成功的 STA IP 給 PWA
void handleApiMyIp() {
  String j = "{";
  if (LAST_STA_IP_VALID) {
    j += "\"ok\":true,\"ip\":\"" + LAST_STA_IP.toString() + "\"";
  } else {
    j += "\"ok\":false,\"ip\":\"\"";
  }
  j += "}";
  addCors();
  server.send(200, "application/json", j);
}

void handleApiEstGet(){
  String j = "{";
  j += "\"ok\":true";
  j += ",\"enabled\":" + String(gEstCfg.enabled ? "true" : "false");
  j += ",\"t1\":" + String((int)gEstCfg.t1);
  j += ",\"t2\":" + String((int)gEstCfg.t2);
  j += ",\"t3\":" + String((int)gEstCfg.t3);
  j += ",\"t4\":" + String((int)gEstCfg.t4);
  j += ",\"seq\":" + String((int)gEstHeatSeq);
  j += ",\"lastSeq\":" + String((int)gEstLastAppliedSeq);
  j += ",\"lastTemp\":" + String((int)gEstLastAppliedTemp);
  j += ",\"heating\":" + String((hostHeatValid && HEATING_NOW) ? "true" : "false");
  j += "}";
  addCors();
  server.send(200, "application/json", j);
}

void handleApiEstPost(){
  bool wasEnabled = (gEstCfg.enabled != 0);

  if (server.hasArg("enabled")) {
    String e = server.arg("enabled");
    gEstCfg.enabled = (e == "1" || e == "true" || e == "on") ? 1 : 0;
  }
  if (server.hasArg("t1")) gEstCfg.t1 = clampEstTemp(server.arg("t1").toInt());
  if (server.hasArg("t2")) gEstCfg.t2 = clampEstTemp(server.arg("t2").toInt());
  if (server.hasArg("t3")) gEstCfg.t3 = clampEstTemp(server.arg("t3").toInt());
  if (server.hasArg("t4")) gEstCfg.t4 = clampEstTemp(server.arg("t4").toInt());
  gEstCfg.mark = EST_CFG_MARK;
  estSave();

  if (!wasEnabled && gEstCfg.enabled) {
    estResetRuntime(true);
  }

  handleApiEstGet();
}

void hRoot() {
  String html = render_control_html_v28c();
  if (NEED_CLEAR_PWA_IP) {
    // 手勢清空時：告訴 PWA 把儲存的 IP 清成空字串
    html += F("<script>try{"
              "if(window.opener && !window.opener.closed)"
              "  window.opener.postMessage('saveip:','*');"
              "else if(window.parent && window.parent!==window)"
              "  window.parent.postMessage('saveip:','*');"
              "}catch(e){}"
              "</script>");
    NEED_CLEAR_PWA_IP = false;
  }
  server.send(200,"text/html; charset=utf-8", html);
}

// 新增：排程頁與設備狀態頁（HTML），由設備本機直接送出
void hSchedulePage(){
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Connection", "close");
  server.send_P(200, "text/html; charset=utf-8", SCHEDULE_HTML);
}

void hStatusPage(){
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Connection", "close");
  server.send_P(200, "text/html; charset=utf-8", STATUS_HTML);
}


void hPowerLogPage(){
  server.send_P(200, "text/html; charset=utf-8", POWERLOG_HTML);
}

void hEstPage(){
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Connection", "close");
  server.send_P(200, "text/html; charset=utf-8", EST_HTML);
}





void hStatus() {
  addCors();
  String j = "{\"tUp\":" + String(tUp) +
             ",\"tSet\":" + String(targetTemp) +
             ",\"m\":" + String(currentMode) +
             ",\"heat\":" + String(HEATING_NOW ? "true" : "false") +
             ",\"heatValid\":" + String(hostHeatValid ? "true" : "false") + "}";
  server.send(200,"application/json", j);
}

void hPowerOn(){ addCors(); if(sterManualPending) cancelSterManual(); sendBoardModeAutoKeepMask(); currentMode=1; gBoardSchedRaw[12]=BOARD_MODE_AUTO; gBoardSchedValid=true; gBoardSchedLastMs=millis(); server.send(204); }

static uint8_t boardTimerMaskFromSaved(){
  if (!gBoardSchedValid) return 0;

  uint8_t mask = 0;
  for (int i = 0; i < 3; i++) {
    const int base = i * 4;
    bool tOk = boardSchedTimeValidRaw(gBoardSchedRaw[base + 0], gBoardSchedRaw[base + 1]) &&
               boardSchedTimeValidRaw(gBoardSchedRaw[base + 2], gBoardSchedRaw[base + 3]);
    bool en  = ((gBoardSchedRaw[13] >> i) & 0x01) != 0;
    if (en && tOk) mask |= (uint8_t)(1 << i);
  }
  return mask;
}

void hTimerMode(){
  if (!gBoardSchedValid) {
    server.send(409, "text/plain; charset=utf-8", "設備端定時資料尚未同步，請先進入排程頁確認");
    return;
  }

  uint8_t mask = boardTimerMaskFromSaved();
  if (mask == 0) {
    server.send(409, "text/plain; charset=utf-8", "設備端第一~第三段尚無有效且已啟用的定時資料");
    return;
  }

  gBoardSchedRaw[13] = mask;
  gBoardSchedValid = true;
  gBoardSchedLastMs = millis();

  sendBoardModeTimerWithMask(mask);
  currentMode = BOARD_MODE_TIMER;
  gBoardSchedRaw[12] = BOARD_MODE_TIMER;
  gBoardSchedValid = true;
  gBoardSchedLastMs = millis();
  server.send(204);
}

void hShutdown(){
  addCors();
  if(sterManualPending) cancelSterManual();
  sendBoardModeOffKeepMask();
  delay(500);
  currentMode = 0;
  gBoardSchedRaw[12] = BOARD_MODE_OFF;
  gBoardSchedValid = true;
  gBoardSchedLastMs = millis();
  server.send(204);
}


void hTempUp(){ addCors(); if(sterManualPending) cancelSterManual(); targetTemp=min(targetTemp+1,70); send42Write16(TEMP_ADDR_WRITE,targetTemp); server.send(204); }
void hTempDown(){ addCors(); if(sterManualPending) cancelSterManual(); targetTemp=max(targetTemp-1,40); send42Write16(TEMP_ADDR_WRITE,targetTemp); server.send(204); }
void hSetTemp(){
  addCors();
  if(sterManualPending) cancelSterManual();
  if(!server.hasArg("t")) { server.send(400, "text/plain", "missing t"); return; }
  int t = server.arg("t").toInt();
  if(t < 40) t = 40;
  if(t > 70) t = 70;
  targetTemp = t;
  send42Write16(TEMP_ADDR_WRITE, targetTemp);
  server.send(204);
}
void hSuper(){ addCors(); if(sterManualPending) cancelSterManual(); sendBoardModeAutoKeepMask(); startSuper(); currentMode=1; gBoardSchedRaw[12]=BOARD_MODE_AUTO; gBoardSchedValid=true; gBoardSchedLastMs=millis(); server.send(204); }
void hSterilize(){ addCors(); startSterManual(); currentMode=1; gBoardSchedRaw[12]=BOARD_MODE_AUTO; gBoardSchedValid=true; gBoardSchedLastMs=millis(); server.send(204); }

void hSetup(){ server.send(200,"text/html; charset=utf-8", pageSetupAsync()); }
void hSetupMini(){ server.send(200,"text/html; charset=utf-8", pageSetupMiniAsync()); }

// 使用者頁（更新手勢說明）
void hUserPage(){
  String html = F(R"rawliteral(
<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>使用者選項</title>
<style>
body{margin:0;background:#f7f9fc;font-family:"Noto Sans TC",sans-serif;color:#004aad;text-align:center;padding:30px}
h2{margin:0 0 20px;font-size:22px}
.btn{display:block;width:84%;max-width:360px;margin:12px auto;padding:14px 18px;font-size:17px;
border:0;border-radius:14px;background:#004aad;color:#fff;font-weight:700;letter-spacing:.5px;cursor:pointer}
.btn-alt{background:#ff8c3a}
.btn-danger{background:#d43737}
.warn{margin:10px auto 0;max-width:360px;background:#ffe0c4;color:#7a3d00;padding:10px 14px;
border-radius:10px;font-size:13px;line-height:1.4;display:none}
.desc{margin:22px auto 0;max-width:360px;text-align:left;font-size:13px;line-height:1.55;color:#555}
.desc b{display:inline-block;width:1.2em}
</style></head><body>
<h2>使用者選項</h2>
<div id="netWarn" class="warn"></div>
<button id="b1" class="btn">1. AP 密碼設定</button>
<button id="b2" class="btn btn-alt">2. AP 密碼不使用</button>
<button id="b3" class="btn btn-danger">3. IP 清空（含 Wi-Fi / AP 密碼）</button>
<div class="desc">
  <div><b>1</b> AP 密碼設定：為設備 AP 加入保護。</div>
  <div><b>2</b> AP 密碼不使用：移除密碼，AP 變開放式。</div>
  <div><b>3</b> IP 清空：清除儲存的 Wi-Fi SSID/密碼與 AP 密碼。</div>
  <div style="margin-top:10px">快速清空手勢：20 秒內，將溫度從 40 升到 ≥45，再降回 40。(數字閃一次按一次的速度)</div>
</div>
<script>
const netWarn=document.getElementById('netWarn');
if(!location.hostname.startsWith('192.168.4.')){
  netWarn.style.display='block';
  netWarn.textContent='⚠ 非設備主機網段，操作將直接導向設備頁面。';
}
function submitAction(path){
  const f=document.createElement('form');
  f.method='POST'; f.action=path; f.style.display='none';
  document.body.appendChild(f); f.submit();
}
document.getElementById('b1').onclick=()=>{ location.href='/ap-pass'; };
document.getElementById('b2').onclick=()=>{
  if(!confirm('停用後 AP 為開放式，繼續？')) return;
  submitAction('/disableappass');
};
document.getElementById('b3').onclick=()=>{
  if(!confirm('確定清空 Wi-Fi / AP 密碼並回初始狀態？')) return;
  submitAction('/factoryreset');
};
</script></body></html>
)rawliteral");
  server.send(200,"text/html; charset=utf-8", html);
}

// AP 密碼設定頁
void hApPassPage(){
  String html = F(R"rawliteral(
<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AP 密碼設定</title>
<style>
body{margin:0;background:#f7f9fc;font-family:"Noto Sans TC",sans-serif;color:#004aad;text-align:center;padding:30px}
h2{margin:0 0 20px;font-size:22px}
input{padding:10px 12px;font-size:16px;width:82%;max-width:320px;border:1px solid #ccc;border-radius:10px}
button{margin-top:16px;padding:11px 20px;font-size:16px;border:0;border-radius:10px;background:#004aad;color:#fff;font-weight:700;cursor:pointer}
small{display:block;margin-top:14px;font-size:12px;color:#666;line-height:1.5}
</style></head><body>
<h2>設定 AP 密碼</h2>
<form id="f" method="POST" action="/setappass">
  <input id="pw" name="pass" type="password" placeholder="至少 8 碼 (8~32)">
  <button id="ok" type="submit">儲存</button>
</form>
<small>快速清空手勢：20 秒內，將溫度從 40 升到 ≥45，再降回 40。(數字閃一次按一次的速度)</small>
<script>
document.getElementById('f').addEventListener('submit',function(e){
  var p=document.getElementById('pw').value.trim();
  if(p.length<8||p.length>32){ e.preventDefault(); alert('長度需 8~32'); }
});
</script></body></html>
)rawliteral");
  server.send(200,"text/html; charset=utf-8", html);
}

void hSetApPass(){
  if(server.method()!=HTTP_POST){ server.send(405,"text/plain","Method Not Allowed"); return; }
  String p = server.arg("pass"); p.trim();
  if(p.length()<8||p.length()>32){
    server.send(400,"text/plain; charset=utf-8","密碼長度需 8~32"); return;
  }
  saveApPass(p);
  String body = "AP 密碼已儲存。\n請稍後重新連線 HMK-Setup（輸入新密碼）。\nAP 將於 1.5 秒後重新啟動。";
  server.send(200,"text/html; charset=utf-8", makeInfoPage("AP 密碼設定完成", body, false, 3000));
  PENDING_AP_ACTION = AP_ACT_SET_PASS;
  PENDING_AP_TS     = millis() + 1500;
}

void hDisableApPass(){
  if(server.method()!=HTTP_POST){ server.send(405,"text/plain","Method Not Allowed"); return; }
  clearApPass();
  String body = "AP 密碼已停用，將轉為開放式。\n請稍後重新連線 HMK-Setup（不需密碼）。\nAP 將於 1.5 秒後套用。";
  server.send(200,"text/html; charset=utf-8", makeInfoPage("AP 密碼停用", body, false, 3000));
  PENDING_AP_ACTION = AP_ACT_DISABLE_PASS;
  PENDING_AP_TS     = millis() + 1500;
}

void hFactoryReset(){
  if(server.method()!=HTTP_POST){ server.send(405,"text/plain","Method Not Allowed"); return; }
  clearWiFi();
  clearApPass();
  String body = "工廠清空完成：Wi-Fi / AP 密碼已清除。\n本機暫時仍維持舊 AP 連線方便顯示結果。\n將於 1.5 秒後重新啟動為『開放式 AP』。\nPWA 儲存 IP 已清除。";
  server.send(200,"text/html; charset=utf-8", makeInfoPage("工廠清空完成", body, true, 3000));
  PENDING_AP_ACTION = AP_ACT_FACTORY_RESET;
  PENDING_AP_TS     = millis() + 1500;
}

// 掃描

int safeScan(int retries=3, unsigned long delayBetweenMs=400) {
  int n = -1;

  // 判斷目前是不是已經有 STA IP（IP 模式在用的那顆）
  bool hasStaIP = (WiFi.localIP()[0] != 0);

  for (int i = 0; i < retries; i++) {
    WiFi.scanDelete();

    // 如果「還沒有 IP」（多半是 AP 設定階段），才去斷線 + delay
    if (!hasStaIP) {
      WiFi.disconnect(true);
      delay(120);
    }

    yield();
    n = WiFi.scanNetworks();
    Serial.printf("[safeScan] attempt %d -> %d networks\n", i + 1, n);
    if (n > 0) break;
    delay(delayBetweenMs);
  }
  return n;
}


void hScan(){
  addCors();
  server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma","no-cache");
  server.sendHeader("Expires","0");
  int n = safeScan(3,500);
  String opts;
  if(n<=0){
    opts = "<option value=''>暫無結果（請稍候或手動輸入）</option>";
  } else {
    for(int i=0;i<n;i++){
      String s = WiFi.SSID(i);
      if(s.length()==0) continue;
      int r = WiFi.RSSI(i);
      opts += "<option value='"+s+"'>"+s+" ("+String(r)+"dBm)</option>";
    }
    if(opts.length()==0) opts="<option value=''>僅掃到空 SSID（請重試）</option>";
  }
  server.send(200,"text/html; charset=utf-8", opts);
}

void hSaveWifi(){
  addCors();
  if(server.method()!=HTTP_POST){ server.send(405,"text/plain","Method Not Allowed"); return; }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false);
  delay(150);
  WIFI_GOTIP=false; WIFI_FAIL=false;

  saveWiFi(ssid,pass);
  delay(60);

  IPAddress chosen;
  bool ok = false;

  const int maxAttempts = 2;
  for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
    Serial.printf("[hSaveWifi] connect attempt %d/%d\n", attempt, maxAttempts);
    ok = chooseStaticAndConnect(ssid.c_str(), pass.c_str(), &chosen);
    if (ok) break;
    if (attempt < maxAttempts) {
      delay(700);
    }
  }

  if(ok){
    String html = R"rawliteral(
<!doctype html><html lang="zh-Hant"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>設定完成</title>
<style>
body{font-family:"Noto Sans TC",sans-serif;text-align:center;background:#f7f9fc;color:#004aad;padding:22px;margin:0}
h2{margin:6px 0 10px}
.ip{font-weight:700;margin:8px 0}
#prog{height:12px;background:#e8eefb;border-radius:10px;overflow:hidden;margin:12px auto;max-width:320px}
#bar{height:100%;width:0;background:linear-gradient(90deg,#4f84ff,#2aa8ff);transition:width .2s}
#txt{margin-top:12px;font-size:15px}
.btn{display:inline-block;margin:10px 8px;padding:10px 16px;border-radius:10px;border:0;cursor:pointer;font-weight:700;color:#fff}
.btn-primary{background:#1877f2}
.btn-plain{background:#6cace4;color:#fff}
#hint{margin-top:12px;color:#c00;white-space:pre-line}
#actions{display:none;margin-top:12px}
</style></head><body>
<h2>設定已儲存</h2>
<div>裝置已取得之 IP：</div>
<div class="ip" id="ip">%IP%</div>
<div id="prog"><div id="bar"></div></div>
<div id="txt">系統正在嘗試連向裝置，若自動成功會自動開啟控制頁；若超時會顯示進一步指示。</div>
<div id="actions">
  <button id="btnRecheck" class="btn btn-primary">我已切換，重新檢查</button>
  <button id="btnUseAp"  class="btn btn-plain">使用設備 Wi-Fi 連接就好</button>
</div>
<div id="hint" style="display:none"></div>
<script>
(function(){
 const ip="%IP%";
 document.getElementById('ip').textContent=ip;
 const bar=document.getElementById('bar');
 const txt=document.getElementById('txt');
 const hint=document.getElementById('hint');
 const actions=document.getElementById('actions');
 const btnRe=document.getElementById('btnRecheck');
 const btnAp=document.getElementById('btnUseAp');
 function setPct(p){ bar.style.width=Math.round(p)+'%'; }
 try{
   if(window.opener && !window.opener.closed) window.opener.postMessage('saveip:'+ip,'*');
   else if(window.parent && window.parent!==window) window.parent.postMessage('saveip:'+ip,'*');
 }catch(e){}
 async function probeOnce(ms=1000){
   try{
     const c=new AbortController();
     const t=setTimeout(()=>c.abort(),ms);
     await fetch('http://'+ip+'/ping?ts='+Date.now(),{mode:'no-cors',cache:'no-store',signal:c.signal});
     clearTimeout(t); return true;
   }catch(e){return false;}
 }
 (async function autoTry(){
   setPct(5);
   const tries=8;
   for(let i=0;i<tries;i++){
     const ok=await probeOnce(1000);
     setPct(5+Math.round((i+1)/tries*80));
     if(ok){
       setPct(100);
       txt.textContent='自動檢測成功，正在開啟控制頁…';
       setTimeout(()=>location.href='http://'+ip+'/',700);
       return;
     }
     txt.textContent='等待裝置回應中…（'+(i+1)+'/'+tries+'）';
     await new Promise(r=>setTimeout(r,700));
   }
   setPct(100);
   actions.style.display='block';
   hint.style.display='block';
   hint.textContent='未偵測到裝置回應，請切回您平常的 Wi-Fi 後按「我已切換，重新檢查」。\n\n或按「使用設備 Wi-Fi 連接就好」維持連回 HMK-Setup。';
   txt.textContent='若您已切回 Wi-Fi，請按「我已切換，重新檢查」。';
 })();
 btnRe.addEventListener('click', async ()=>{
   hint.style.display='none';
   txt.textContent='正在檢查是否能連上裝置，請稍候…';
   setPct(10);
   const tries=8;
   for(let i=0;i<tries;i++){
     const ok=await probeOnce(1200);
     setPct(10+Math.round((i+1)/tries*80));
     if(ok){
       setPct(100);
       txt.textContent='已連上裝置，正在進入控制頁…（AP 熱點維持開啟）';
       setTimeout(()=>location.href='http://'+ip+'/',1000);
       return;
     }
     txt.textContent='尚未連上（'+(i+1)+'/'+tries+'），請確認手機已切回家用 Wi-Fi';
     await new Promise(r=>setTimeout(r,900));
   }
   hint.style.display='block';
   hint.textContent='仍無法連上裝置。請確認手機網路或保持連回 HMK-Setup 再重新設定。';
   txt.textContent='如需，按「使用設備 Wi-Fi 連接就好」。';
 });
 btnAp.addEventListener('click',()=>{ location.href='http://192.168.4.1/'; });
 setPct(0);
})();
</script></body></html>
)rawliteral";
    html.replace("%IP%", chosen.toString());
    server.send(200,"text/html; charset=utf-8", html);
    return;
  }

  String htmlFail = R"rawliteral(
<!doctype html><html lang="zh-Hant"><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>連線失敗</title></head>
<body style="font-family:'Noto Sans TC';text-align:center;background:#f7f9fc;color:#004aad;padding:40px;">
<h2>連線失敗</h2>
<p>請重新進行 Wi-Fi 設定。</p>
<script>setTimeout(()=>location.href='/setup',2000);</script>
</body></html>
)rawliteral";
  server.send(200,"text/html; charset=utf-8", htmlFail);
}

void hOpenAP() {
  if(loadApPass(AP_PASS_BUF, sizeof(AP_PASS_BUF))){
    AP_PASS_SET = strlen(AP_PASS_BUF) > 0;
  } else {
    AP_PASS_SET = false;
    AP_PASS_BUF[0] = 0;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS_SET ? AP_PASS_BUF : "");
  AP_WINDOW_ON        = true;
  AP_WINDOW_DEADLINE  = millis() + 120000;
  server.send(200,"text/plain; charset=utf-8", String("AP 已開啟 120 秒，SSID: ") + AP_SSID);
}

void hCloseAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS_SET ? AP_PASS_BUF : "");
  server.send(200,"text/plain; charset=utf-8","AP 維持開啟（避免熱點消失）");
}

// 手勢除錯端點
void hGestureDebug() {
  String j = "{";
  j += "\"stage\":" + String(PATTERN_STAGE) + ",";
  j += "\"lastMode\":" + String(LAST_MODE) + ",";
  j += "\"lastTempRaw\":" + String(LAST_TEMP_RAW) + ",";
  j += "\"lastTempValid\":" + String(LAST_TEMP_VALID ? "true" : "false") + ",";
  j += "\"pendingAction\":" + String((int)PENDING_AP_ACTION) + ",";
  j += "\"needClearPwaIp\":" + String(NEED_CLEAR_PWA_IP ? "true" : "false");
  j += "}";
  server.send(200,"application/json", j);
}

void hGestureForceReset() {
  triggerFactoryResetByGesture();
  server.send(200,"text/plain; charset=utf-8","手勢模擬觸發完成");
}

// =============================================================
// Setup / Loop
// =============================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  LED_ON();
  pinMode(PIN_REDE, OUTPUT);
  digitalWrite(PIN_REDE, LOW);

  RS485.begin(9600);
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  g433BootMs = millis();

  pinMode(RF_RX_PIN, INPUT_PULLUP);
  rf433.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
  rf433.setReceiveTolerance(85);
  Serial.println("[433] receiver ready (D2)");

  rf433LoadPair();
  g433NeedPair = !rf433IsPaired();
  Serial.println(g433NeedPair ? "[433] not paired" : "[433] paired OK");

  // 開機嗶一聲 + 5 秒配對視窗（收到 KEY4=0x8 才會配對 ID20）
  beepOnce(2600, 120);
  rf433BeginPairing(5000UL);


  attachWifiEvents();
  NTP_UDP.begin(2390);

  // 初始化排程（從 EEPROM 載入）
  loadSchedules();
  rebuildBoardTimerPlanFromSaved();

  // 用電記錄（2 年）
  pwr2yLoad();
  pwr2yDayLoad();   // ★放這裡最穩：gPwr2Y 已經載入了
    
  heaterHealthLoad(); // 電熱管健康度參考（從 EEPROM 載入）
  estLoad();

  if(loadApPass(AP_PASS_BUF, sizeof(AP_PASS_BUF))){
    AP_PASS_SET = strlen(AP_PASS_BUF) > 0;
  } else {
    AP_PASS_SET = false;
    AP_PASS_BUF[0] = 0;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS_SET ? AP_PASS_BUF : "");
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);

  AP_WINDOW_ON       = true;
  AP_WINDOW_DEADLINE = millis() + 120000;
  Serial.println(String("AP 啟動完成，SSID: ") + AP_SSID + (AP_PASS_SET ? " (有密碼)" : " (開放式)"));

  if (checkDoubleReset()) {
    clearWiFi();
    Serial.println("⚠️ 快速重啟 → 清除 Wi-Fi → AP 模式");
  } else {
    char ssid[32]={0}, pass[64]={0};
    if(loadWiFi(ssid,pass)){
      Serial.print("已儲存 Wi-Fi："); Serial.println(ssid);
      strncpy(BOOT_SSID, ssid, sizeof(BOOT_SSID)-1);
      strncpy(BOOT_PASS, pass, sizeof(BOOT_PASS)-1);
      BOOT_STA_PENDING = true;
      BOOT_STA_TS = millis() + 1000;
      Serial.println("排程背景 STA 連線（1 秒後嘗試）");
    } else {
      Serial.println("未儲存 Wi-Fi → AP 模式待命");
    }
  }

  // 路由
  server.on("/",        hRoot);
  server.on("/setup",   hSetup);
  server.on("/setup-mini", hSetupMini);
  server.on("/user",    hUserPage);
  server.on("/ap-pass", hApPassPage);
  server.on("/schedule.html", hSchedulePage);
  server.on("/est.html",      hEstPage);
  server.on("/status.html",   hStatusPage);
  server.on("/powerlog.html", hPowerLogPage);







  server.on("/setappass",     HTTP_POST, hSetApPass);
  server.on("/disableappass", HTTP_POST, hDisableApPass);
  server.on("/factoryreset",  HTTP_POST, hFactoryReset);
  server.on("/scan",    hScan);
  server.on("/savewifi",HTTP_POST,hSaveWifi);
  server.on("/clearwifi", [](){ clearWiFi(); server.send(200,"text/plain","CLEARED"); });
  server.on("/status",  hStatus);
  server.on("/poweron", HTTP_POST, hPowerOn);
  server.on("/timermode",HTTP_POST, hTimerMode);
  server.on("/shutdown",HTTP_POST, hShutdown);
  server.on("/tempup",  HTTP_POST, hTempUp);
  server.on("/tempdown",HTTP_POST, hTempDown);
  server.on("/settemp", HTTP_POST, hSetTemp);
  server.on("/super",   HTTP_POST, hSuper);
  server.on("/sterilize", HTTP_POST, hSterilize);
  server.on("/closeap", hCloseAP);
  server.on("/openap",  hOpenAP);
  server.on("/ping", [](){ addCors(); server.send(200,"text/plain","ok"); });
  server.on("/debuggesture", hGestureDebug);
  server.on("/forcegesture", hGestureForceReset);
  server.on("/appassstate", [](){
    String j = String("{\"apPassSet\":") + (AP_PASS_SET?"true":"false") + "}";
    server.send(200,"application/json", j);
  });

  // 新增排程/時間 API
  server.on("/api/time",     HTTP_POST, handleApiTime);
  server.on("/api/schedule", HTTP_GET,  handleApiScheduleGet);
  server.on("/api/schedule", HTTP_POST, handleApiSchedulePost);
  server.on("/api/est",      HTTP_GET,  handleApiEstGet);
  server.on("/api/est",      HTTP_POST, handleApiEstPost);
  server.on("/api/devstatus", HTTP_GET, handleApiDevStatus);
  server.on("/api/myip",      HTTP_GET, handleApiMyIp);   // ★ 新增

  // 用電記錄（2 年）
  server.on("/api/powerlog", HTTP_GET, handleApiPowerLog);
  server.on("/api/powerlog", HTTP_POST, handleApiPowerLogPost);

  //機板排程
  server.on("/api/boardsched", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/api/boardsched", HTTP_GET, handleApiBoardSchedGet);
  server.on("/api/boardsched", HTTP_POST, handleApiBoardSchedPost);


  // CORS 預檢（OPTIONS）
  server.on("/status", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/scan", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/savewifi", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/ping", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/poweron", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/shutdown", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/tempup", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/tempdown", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/settemp", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/super", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/sterilize", HTTP_OPTIONS, [](){ addCors(); server.send(204); });
  server.on("/api/time", HTTP_OPTIONS, [](){
    addCors();
    server.send(204);   // no content
  });
  server.on("/api/schedule", HTTP_OPTIONS, [](){
    addCors();
    server.send(204);
  });
  server.on("/api/est", HTTP_OPTIONS, [](){
    addCors();
    server.send(204);
  });
  server.on("/api/devstatus", HTTP_OPTIONS, [](){
    addCors();
    server.send(204);
  });
  server.on("/api/powerlog", HTTP_OPTIONS, [](){
    addCors();
    server.send(204);
  });
  server.on("/api/myip", HTTP_OPTIONS, [](){              // ★ 新增
    addCors();
    server.send(204);
  });

   server.on("/api/hhreset", HTTP_POST, handleApiHHReset);








  server.begin();
  Serial.println("HMK v9 ready.");
  LED_OFF();
}

void loop() {
  // 軟體時鐘 + 排程檢查
  tickRTC();
  checkSchedule();

  // 用電記錄（2 年）
  tickPower2Y();

  // 健康度採樣改為每秒持續評估，避免只靠特定位址回包導致漏計。
  static uint32_t hhTickMs = 0;
  if ((millis() - hhTickMs) >= 1000UL) {
    hhTickMs = millis();
    heaterHealthTick((int)tUp);
  }

  tickSuper();          // ★新增：1小時到就回復 22=5 + 65°C
  tickSterManual();     // ★新增：手動殺菌 1 小時到就恢復
  server.handleClient();
  // 433 配對視窗：由 rf433BeginPairing() 設定（開機 5 秒；手勢重配對 60 秒）
  if (g433Pairing && g433PairDeadlineMs && (long)(millis() - g433PairDeadlineMs) >= 0) {
    rf433StopPairing();
    Serial.println("[433] pairing window closed");
  }


  if (BOOT_STA_PENDING && millis() > BOOT_STA_TS) {
    BOOT_STA_PENDING = false;
    IPAddress chosen;
    if (chooseStaticAndConnect(BOOT_SSID, BOOT_PASS, &chosen)) {
      Serial.println(String("開機 STA 連線成功，IP: ") + chosen.toString());
    } else {
      Serial.println("開機 STA 連線失敗，維持 AP 待命");
    }
  }

  bool staUp = (WiFi.status() == WL_CONNECTED);
  if (staUp && !STA_LINK_WAS_UP) {
    syncTimeFromNtpInOrder("sta-up");
  }
  if (staUp && (!NTP_HAS_SYNC || (millis() - NTP_LAST_SYNC_MS >= NTP_RESYNC_MS))) {
    syncTimeFromNtpInOrder(NTP_HAS_SYNC ? "24h" : "boot");
  }
  STA_LINK_WAS_UP = staUp;

  if(PENDING_AP_ACTION != AP_ACT_NONE && millis() > PENDING_AP_TS){
    PendingApAction act = PENDING_AP_ACTION;
    PENDING_AP_ACTION = AP_ACT_NONE;
    if(act == AP_ACT_SET_PASS){
      WiFi.softAPdisconnect(true);
      delay(120);
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(AP_SSID.c_str(), AP_PASS_BUF);
      Serial.println("[Deferred] AP 密碼已套用並重啟");
    } else if(act == AP_ACT_DISABLE_PASS){
      WiFi.softAPdisconnect(true);
      delay(120);
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(AP_SSID.c_str(), "");
      Serial.println("[Deferred] AP 已改為開放式");
    } else if(act == AP_ACT_FACTORY_RESET){
      WiFi.disconnect(true);
      WiFi.config(0U,0U,0U);
      delay(150);
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(AP_SSID.c_str(), "");
      Serial.println("[Deferred] 工廠清空套用完成，AP 開放式");
    }
  }

  static uint8_t rxBuf[256];
  static size_t  rxLen = 0;
  static unsigned long lastByteMs = 0;

  while (RS485.available()) {
    int b = RS485.read();
    if (b >= 0 && rxLen < sizeof(rxBuf)) {
      rxBuf[rxLen++] = (uint8_t)b;
      lastByteMs = millis();
      gRs485LastByteMs = lastByteMs;
    }
  }
  if (rxLen > 0 && millis() - lastByteMs > 20) {
    onFrame(rxBuf, rxLen);
    rxLen = 0;
  }

if (rf433.available()) {
  unsigned long v  = rf433.getReceivedValue();
  unsigned int  bl = rf433.getReceivedBitlength();
  rf433.resetAvailable();

  if (v == 0) return;
  if (bl < 24) return; // 雜訊 / 不足位數忽略

  // 只取低 24-bit 當作 EV1527 raw24（多數 EV1527 就是 24-bit）
  uint32_t raw24 = ((uint32_t)v) & 0xFFFFFFUL;
  uint32_t id20  = (raw24 >> 4) & 0xFFFFFUL;
  uint8_t  key4  = (uint8_t)(raw24 & 0x0FUL);

  // 防抖：同一碼 250ms 內不重複處理（長按不洗版）
  static uint32_t lastRaw = 0;
  static uint32_t lastMs  = 0;
  uint32_t now = millis();
  if (raw24 == lastRaw && (now - lastMs) < 250) return;
  lastRaw = raw24;
  lastMs  = now;

  // ✅ 你要的輸出
  Serial.printf("EV1527 ID20=0x%05lX  KEY4=0x%X\n", (unsigned long)id20, key4);

  // ---- 配對模式：僅接受 KEY4=0x8 (OFF) 來配對 ID ----
  if (g433Pairing) {
    if (id20 && key4 == 0x8) {
      gRF.mark = RF433_PAIR_MARK;
      gRF.id20 = id20;
      rf433SavePair();
      g433NeedPair = false;
      rf433StopPairing();
      beep2(1800, 120, 100);
      Serial.printf("[433] paired: ID20=0x%05lX (KEY4=0x8)\n", (unsigned long)id20);
    }
    return;
  }

  if (!rf433IsPaired()) return;
  if (id20 != (gRF.id20 & 0xFFFFFUL)) return; // 只接受同一個遙控器 ID

  // 只要按了其他功能鍵，就視為手動操作，取消「手動殺菌」倒數
  if (key4 != 0x6 && sterManualPending) {
    cancelSterManual();
    Serial.println("[SterilizeManual] canceled by manual key");
  }

  switch (key4) {
    case 0x4: // OFF
      beep2(1500, 120, 100);
      sendBoardModeOffKeepMask();
      currentMode = 0;
      gBoardSchedRaw[12] = BOARD_MODE_OFF;
      gBoardSchedValid = true;
      gBoardSchedLastMs = millis();
      Serial.println("[433] OFF -> MODE=0");
      break;

    case 0x8: // ON
      sendBoardModeAutoKeepMask();
      currentMode = 1;
      gBoardSchedRaw[12] = BOARD_MODE_AUTO;
      gBoardSchedValid = true;
      gBoardSchedLastMs = millis();
      beepOnce(2000, 120);
      Serial.println("[433] ON -> MODE=1");
      break;

    case 0xC: // 45°C
      targetTemp = 45;
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
      beepOnce(1500, 90);
      Serial.println("[433] SET TEMP -> 45");
      break;

    case 0x2: // 55°C
      targetTemp = 55;
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
      beepOnce(1500, 90);
      Serial.println("[433] SET TEMP -> 55");
      break;

    case 0xA: // 70°C
      targetTemp = 70;
      send42Write16(TEMP_ADDR_WRITE, (uint16_t)targetTemp);
      beepOnce(1500, 90);
      Serial.println("[433] SET TEMP -> 70");
      break;

    case 0x6: // 殺菌（手動 1 小時）
      beepOnce(900, 120);
      startSterManual();
      break;

    default:
      break;
  }
}




}
