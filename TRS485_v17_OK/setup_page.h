const char SETUP_MINI_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi 設定</title>
<style>
  *{box-sizing:border-box}
  html,body{margin:0;padding:0;background:transparent;font-family:"Noto Sans TC",system-ui,sans-serif;color:#1a8ea5}
  .wrap{padding:14px;background:#fff;border-radius:16px}
  .stepLabel{display:block;margin:10px 0 8px;font-size:16px;font-weight:800;color:#1a8ea5;text-align:left}
  input,select{padding:10px 12px;font-size:16px;width:100%;border:1px solid #b8d4db;border-radius:10px;color:#245c67;background:#f7fcfd}
  .saveBtn{margin-top:14px;width:100%;min-height:44px;border:none;border-radius:999px;background:linear-gradient(180deg,#2aa6bf,#1a8ea5);color:#fff;font-size:17px;font-weight:800;cursor:pointer}
  #msg{margin-top:10px;min-height:20px;color:#5f8b95;font-size:13px;text-align:center}
</style>
</head>
<body>
<div class="wrap">
  <form id="wifiForm">
    <label class="stepLabel">1. 選擇您的 Wi-Fi 名稱</label>
    <select id="ssidSelect" name="ssid"></select>

    <label class="stepLabel">2. 輸入您的 Wi-Fi 密碼</label>
    <input id="pass" name="pass" type="password" placeholder="輸入密碼">

    <button class="saveBtn" type="submit">3. 儲存密碼完成設定</button>
  </form>
  <p id="msg"></p>
</div>

<script>
async function scan(){
  const sel=document.getElementById('ssidSelect');
  sel.innerHTML='<option>掃描中...</option>';
  try{
    const r=await fetch('/scan',{cache:'no-store'});
    const t=await r.text();
    sel.innerHTML=t;
  }catch(e){
    sel.innerHTML='<option>無法取得清單</option>';
  }
}

async function doSave(ssid, pass){
  const form=new FormData();
  form.append('ssid',ssid);
  form.append('pass',pass);
  // 儲存流程可能超過 60 秒（設備重連/探測 IP），避免前端過早中止造成誤判失敗。
  const r=await fetch('/savewifi',{method:'POST',body:form,cache:'no-store'});
  return await r.text();
}

document.getElementById('wifiForm').onsubmit = async (ev) => {
  ev.preventDefault();
  const ssid = document.getElementById('ssidSelect').value;
  const pass = document.getElementById('pass').value;
  document.getElementById('msg').textContent = '儲存中...';

  try{
    const t = await doSave(ssid, pass);
    document.open(); document.write(t); document.close();
  }catch(e){
    document.getElementById('msg').textContent = '儲存失敗，請再試一次。';
  }
};

scan();
</script>
</body>
</html>
)rawliteral";

String pageSetupAsync(){ return FPSTR(SETUP_MINI_HTML); }
String pageSetupMiniAsync(){ return FPSTR(SETUP_MINI_HTML); }