/**
 * @file    ConfigManager.cpp
 * @brief   ConfigManager 类的方法实现
 *
 * @details 完整实现 AP 强制门户配网、凭据 LittleFS 持久化、HTTP 302 重定向。
 *          遵循 V2 架构原则：高内聚低耦合、企业级健壮性、零硬编码凭据。
 */

#include "ConfigManager.h"

/* ========================================================================== */
/*  常量定义                                                                  */
/* ========================================================================== */

const char* ConfigManager::CONFIG_FILE       = "/wifi.json";
const char* ConfigManager::AP_SSID           = "OLED-BitStream";
const IPAddress ConfigManager::AP_IP         = IPAddress(192, 168, 4, 1);
const IPAddress ConfigManager::AP_SUBNET     = IPAddress(255, 255, 255, 0);
const char* ConfigManager::REDIRECT_BASE_URL = "https://kloms-fame.github.io/BitStream_12864/";

/* ========================================================================== */
/*  Captive Portal 配网页 — 极客终端风格（存储于 Flash 以节省 RAM）            */
/* ========================================================================== */

static const char PAGE_CONFIG[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>OLED-BitStream · 配网</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{
  background:#050505;
  color:#00ff41;
  font-family:'Courier New',Consolas,monospace;
  display:flex;justify-content:center;align-items:center;
  min-height:100vh;padding:16px;
}
.term{
  background:#0a0a0f;
  border:1px solid rgba(0,255,65,.2);
  border-radius:6px;
  padding:28px 24px;
  max-width:400px;width:100%;
  box-shadow:0 0 40px rgba(0,255,65,.06),inset 0 0 40px rgba(0,255,65,.015);
  position:relative;overflow:hidden;
}
.term::before{
  content:'''';position:absolute;top:0;left:0;right:0;
  height:1px;background:linear-gradient(90deg,transparent,rgba(0,255,65,.5),transparent);
  animation:scan 3s linear infinite;
}
@keyframes scan{0%{top:0}100%{top:100%}}
.ascii{
  color:#00cc33;font-size:10px;line-height:1.2;
  text-align:center;margin-bottom:16px;
  white-space:pre;opacity:.85;
}
h2{
  font-size:13px;text-align:center;margin-bottom:4px;
  letter-spacing:6px;color:#00ff41;text-transform:uppercase;
}
.sub{
  text-align:center;font-size:10px;color:#008833;margin-bottom:24px;
}
.blink{animation:blink 1s step-end infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0}}
label{
  display:block;font-size:10px;color:#00cc33;
  margin:14px 0 4px;text-transform:uppercase;letter-spacing:2px;
}
input[type=text],input[type=password]{
  width:100%;padding:10px 12px;
  background:#0d0d12;border:1px solid rgba(0,255,65,.2);border-radius:4px;
  color:#00ff41;font-family:inherit;font-size:13px;outline:none;
  transition:border-color .3s;
}
input:focus{border-color:#00ff41;box-shadow:0 0 10px rgba(0,255,65,.1)}
::placeholder{color:#003311}
.btn{
  display:block;width:100%;padding:12px;margin-top:20px;
  background:transparent;border:1px solid #00ff41;border-radius:4px;
  color:#00ff41;font-family:inherit;font-size:12px;cursor:pointer;
  text-transform:uppercase;letter-spacing:4px;transition:all .3s;
}
.btn:hover{background:#00ff41;color:#050505;box-shadow:0 0 20px rgba(0,255,65,.3)}
.btn:active{transform:scale(.97)}
.footer{text-align:center;font-size:9px;color:#003311;margin-top:20px}
</style>
</head>
<body>
<div class="term">
<div class="ascii">
  ____  _ _   _       ____                              TM
 |  _ \(_) |_| |_ ___/ ___| _ __ ___  __ _ _ __   ___
 | | | | | __| __/ _ \___ \| ''__/ _ \/ _` | '_''\ / _ \
 | |_| | | |_| || (_) |__) | | |  __/ (_| | |_) | (_) |
 |____/|_|\__|\__\___/____/|_|  \___|\__,_| .__/ \___/
                                          |_|    V2.0
</div>
<h2>WiFi 配网终端</h2>
<div class="sub">ENCRYPTED CHANNEL <span class="blink">&#9608;</span></div>
<form action="/save" method="POST">
<label>&#9654; SSID (网络名称)</label>
<input type="text" name="ssid" placeholder="输入 WiFi 名称..." required autofocus>
<label>&#9654; PASS (网络密钥)</label>
<input type="password" name="pass" placeholder="输入 WiFi 密码..." required>
<button type="submit" class="btn">[ 建立连接 ]</button>
</form>
<div class="footer">ESP8266 &middot; SSD1306 128&times;64 &middot; WebSocket 推流</div>
</div>
</body>
</html>
)=====";

/* ========================================================================== */
/*  配网成功确认页（提交凭据后的过渡页）                                        */
/* ========================================================================== */

static const char PAGE_SAVED[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>凭据已保存</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{
  background:#050505;color:#00ff41;
  font-family:'Courier New',Consolas,monospace;
  display:flex;justify-content:center;align-items:center;
  min-height:100vh;padding:16px;
}
.msg{
  text-align:center;max-width:360px;
}
h1{font-size:14px;letter-spacing:3px;margin-bottom:12px}
p{font-size:11px;color:#008833;line-height:1.6}
.dots{display:inline-block}
.dots::after{content:'''';animation:dots 1.5s steps(4,end) infinite}
@keyframes dots{0%{content:''''}25%{content:''.''}50%{content:''..''}75%{content:''...''}100%{content:''''}}
</style>
</head>
<body>
<div class="msg">
<h1>&#10003; 凭据已加密存储</h1>
<p>设备正在切换至 Station 模式并连接 WiFi<span class="dots"></span></p>
<p style="margin-top:12px;font-size:10px;color:#003311">请稍候 — OLED 屏幕将显示连接状态</p>
</div>
</body>
</html>
)=====";

/* ========================================================================== */
/*  构造函数                                                                  */
/* ========================================================================== */

ConfigManager::ConfigManager()
    : m_mode(Mode::INIT)
    , m_dnsServer()
    , m_httpServer(HTTP_PORT)
    , m_apStartTime(0)
{
}

/* ========================================================================== */
/*  公有方法 — begin()                                                        */
/* ========================================================================== */

void ConfigManager::begin(StatusCallback onStatus)
{
    // 保存状态回调（用于 OLED 实时反馈）
    m_statusCallback = onStatus;

    Serial.println(F("\n========================================"));
    Serial.println(F("  BitStream V2 — ConfigManager 启动"));
    Serial.println(F("========================================"));

    // 1. 挂载 LittleFS 文件系统
    Serial.print(F("[CFG] 挂载 LittleFS ... "));
    if (!LittleFS.begin())
    {
        Serial.println(F("失败！格式化中 ..."));
        if (!LittleFS.format())
        {
            Serial.println(F("[CFG] 格式化也失败了，放弃文件系统"));
        }
        if (!LittleFS.begin())
        {
            Serial.println(F("[CFG] 二次挂载仍然失败，进入 AP 模式"));
            startAPMode();
            return;
        }
    }
    Serial.println(F("成功"));

    // 2. 尝试加载凭据
    String savedSSID;
    String savedPass;
    const bool hasCreds = loadCredentials(savedSSID, savedPass);

    if (hasCreds && savedSSID.length() > 0)
    {
        Serial.printf("[CFG] 找到已保存凭据: SSID=\"%s\"\n", savedSSID.c_str());

        // 3. 尝试连接 WiFi
        if (connectWiFi(savedSSID, savedPass, WIFI_CONNECT_TIMEOUT_SEC))
        {
            // 连接成功 → 进入 Station 模式
            startStationMode();
            return;
        }

        // 连接失败 → 清除旧凭据，进入 AP 模式让用户重新配网
        Serial.println(F("[CFG] 保存的凭据连接失败，清除并进入 AP 模式"));
        LittleFS.remove(CONFIG_FILE);
    }
    else
    {
        Serial.println(F("[CFG] 无已保存凭据"));
    }

    // 4. 进入 AP 配网模式
    startAPMode();
}

/* ========================================================================== */
/*  公有方法 — loop()                                                         */
/* ========================================================================== */

void ConfigManager::loop()
{
    // DNS 劫持在 AP 模式下持续运行；Station 模式下 harmless（无客户端查询）
    m_dnsServer.processNextRequest();

    // HTTP 服务在两种模式下均需驱动
    m_httpServer.handleClient();

    // AP 模式超时检测：5 分钟无人配网则自动重启
    if (m_mode == Mode::AP_CONFIG)
    {
        if (millis() - m_apStartTime >= AP_TIMEOUT_MS)
        {
            Serial.println(F("[CFG] AP 配网超时（5分钟），自动重启..."));
            delay(500);
            ESP.restart();
        }
    }
}

/* ========================================================================== */
/*  公有方法 — 状态查询                                                       */
/* ========================================================================== */

ConfigManager::Mode ConfigManager::getMode() const
{
    return m_mode;
}

bool ConfigManager::isWiFiConnected() const
{
    return (m_mode == Mode::STATION_CONNECTED)
        && (WiFi.status() == WL_CONNECTED);
}

/* ========================================================================== */
/*  私有方法 — 凭据持久化                                                     */
/* ========================================================================== */

bool ConfigManager::loadCredentials(String& ssid, String& password)
{
    if (!LittleFS.exists(CONFIG_FILE))
    {
        return false;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file)
    {
        Serial.println(F("[CFG] 无法打开凭据文件"));
        return false;
    }

    // 读取文件内容到字符串
    String jsonStr;
    while (file.available())
    {
        jsonStr += (char)file.read();
    }
    file.close();

    // 使用 ArduinoJson 解析（静态缓冲区，栈上分配，零堆碎片）
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, jsonStr);

    if (error)
    {
        Serial.printf("[CFG] JSON 解析失败: %s\n", error.c_str());
        return false;
    }

    // 提取字段（"s"=ssid, "p"=password — 极致压缩键名）
    const char* s = doc["s"] | "";
    const char* p = doc["p"] | "";

    if (strlen(s) == 0)
    {
        Serial.println(F("[CFG] 凭据文件中 SSID 为空"));
        return false;
    }

    ssid     = String(s);
    password = String(p);
    return true;
}

bool ConfigManager::saveCredentials(const String& ssid, const String& password)
{
    // 构建 JSON 文档
    JsonDocument doc;
    doc["s"] = ssid;
    doc["p"] = password;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file)
    {
        Serial.println(F("[CFG] 无法创建凭据文件"));
        return false;
    }

    const size_t written = serializeJson(doc, file);
    file.close();

    if (written == 0)
    {
        Serial.println(F("[CFG] 凭据序列化写入失败"));
        return false;
    }

    Serial.printf("[CFG] 凭据已保存: %u 字节写入 %s\n", written, CONFIG_FILE);
    return true;
}

/* ========================================================================== */
/*  私有方法 — WiFi 连接（带超时）                                            */
/* ========================================================================== */

bool ConfigManager::connectWiFi(const String& ssid, const String& password,
                                 uint16_t timeoutSec)
{
    Serial.printf("[CFG] 正在连接 WiFi: \"%s\" ...\n", ssid.c_str());

    // 将 WiFi 设置为 Station 模式（关闭可能残留的 SoftAP）
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    // 非阻塞轮询，带超时
    const unsigned long deadline = millis() + (unsigned long)timeoutSec * 1000UL;

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() > deadline)
        {
            Serial.println(F("[CFG] WiFi 连接超时"));
            WiFi.disconnect(true);  // 关闭 Station 模式，释放资源
            return false;
        }
        delay(200);
        Serial.print('.');
    }

    Serial.println();
    Serial.printf("[CFG] WiFi 已连接 — IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());

    // 将当前连接的凭据回写到文件（确保凭据始终是最新的正确值）
    saveCredentials(ssid, password);

    return true;
}

/* ========================================================================== */
/*  私有方法 — AP 模式                                                        */
/* ========================================================================== */

void ConfigManager::startAPMode()
{
    Serial.println(F("[CFG] >>> 进入 AP 配网模式 <<<"));

    // 1. 配置 SoftAP
    WiFi.mode(WIFI_AP);
    const bool apOk = WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET)
                   && WiFi.softAP(AP_SSID);
    // 无论返回值如何（某些旧版 core 有 bug 返回 false 但实际成功），继续执行

    Serial.printf("[CFG] AP 热点已开放: SSID=\"%s\"\n", AP_SSID);
    Serial.printf("[CFG] 请用手机连接此热点，浏览器将自动弹出配网页\n");
    Serial.printf("[CFG] 或手动访问: http://%s\n", AP_IP.toString().c_str());

    // 1.5. 注册 AP 客户端连接/断开事件 — 实时反馈到 OLED
    {
        m_onStationConnected = WiFi.onSoftAPModeStationConnected(
            [this](const WiFiEventSoftAPModeStationConnected& evt) {
                Serial.printf("[CFG] AP client connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    evt.mac[0],evt.mac[1],evt.mac[2],evt.mac[3],evt.mac[4],evt.mac[5]);
                if (m_statusCallback) m_statusCallback("Client Connected!");
            });
        m_onStationDisconnected = WiFi.onSoftAPModeStationDisconnected(
            [this](const WiFiEventSoftAPModeStationDisconnected& evt) {
                Serial.printf("[CFG] AP client disconnected: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    evt.mac[0],evt.mac[1],evt.mac[2],evt.mac[3],evt.mac[4],evt.mac[5]);
            });
    }

    //// 2. 启动 DNS 劫持 — 将所有域名解析到 ESP 自身 IP
    m_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    m_dnsServer.start(DNS_PORT, "*", AP_IP);
    // 显式添加常见检测域名（部分平台对通配符 DNS 处理不佳）
    {
        static const char* domains[] = {
            "www.msftconnecttest.com",   // Windows
            "msftconnecttest.com",       // Windows (no www)
            "captive.apple.com",         // Apple / iOS / macOS
            "connectivitycheck.gstatic.com", // Android
            "clients3.google.com",       // Android / Chrome
            "detectportal.firefox.com",  // Firefox
            "www.msftncsi.com",          // Windows NCSI
        };
        // DNS wildcard "*" already covers all domains
    }
    Serial.println(F("[CFG] DNS 劫持已启动 (53 端口 → 192.168.4.1)"));

    // 3. 注册 HTTP 路由
    m_httpServer.on("/",      HTTP_GET,  [this]() { serveConfigPage(); });
    m_httpServer.on("/save",  HTTP_POST, [this]() { handleSaveConfig(); });

    //  层级 3: 平台专用检测 URL — 直接返回 200 OK 配网页（非 302）
    //
    //  Windows 检测端点
    m_httpServer.on("/redirect",            HTTP_GET, [this]() { serveCaptivePage(); });
    m_httpServer.on("/fwlink",              HTTP_GET, [this]() { serveCaptivePage(); });
    m_httpServer.on("/ncsi.txt",            HTTP_GET, [this]() { serveNcsiOK(); });
    m_httpServer.on("/connecttest.txt",     HTTP_GET, [this]() { serveNcsiOK(); });
    //  Android 检测端点
    m_httpServer.on("/generate_204",        HTTP_GET, [this]() { serve204NoContent(); });
    //  Apple / iOS / macOS 检测端点
    m_httpServer.on("/hotspot-detect.html",         HTTP_GET, [this]() { serveCaptivePage(); });
    m_httpServer.on("/library/test/success.html",   HTTP_GET, [this]() { serveCaptivePage(); });
    //  Firefox 检测端点
    m_httpServer.on("/canonical.html",              HTTP_GET, [this]() { serveCaptivePage(); });
    m_httpServer.on("/success.txt",                 HTTP_GET, [this]() { serveCaptivePage(); });
    //  Chrome 检测端点
    m_httpServer.on("/check_network_status.txt",    HTTP_GET, [this]() { serveCaptivePage(); });

    //// 通配路由：覆盖所有 Captive Portal 检测 URL 及其他未知路径
    // Android:  /generate_204
    // Apple:    /hotspot-detect.html, /library/test/success.html
    // Windows:  /ncsi.txt, /connecttest.txt, /redirect, /fwlink
    m_httpServer.onNotFound([this]()
    {
        // 将一切未匹配请求重定向到配网页（302 触发浏览器弹出）
        m_httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_CONFIG);
    });

    m_httpServer.begin();
    Serial.println(F("[CFG] Captive Portal HTTP 服务已启动 (80 端口)"));

    // 4. 记录启动时间用于超时检测
    // OLED 状态反馈：通知主程序更新屏幕
    if (m_statusCallback) { m_statusCallback("AP:OLED-BitStream"); }

    m_apStartTime = millis();
    m_mode        = Mode::AP_CONFIG;
}

void ConfigManager::serveConfigPage()
{
    m_httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_CONFIG);
}

void ConfigManager::handleSaveConfig()
{
    // 解析表单参数
    const String ssid = m_httpServer.arg("ssid");
    const String pass = m_httpServer.arg("pass");

    // 输入校验
    if (ssid.length() == 0)
    {
        m_httpServer.send(400, "text/plain", "ERROR: SSID cannot be empty");
        return;
    }

    Serial.printf("[CFG] 收到配网请求: SSID=\"%s\", PASS 长度=%u\n",
                  ssid.c_str(), pass.length());

    // 存储凭据到 LittleFS
    if (!saveCredentials(ssid, pass))
    {
        m_httpServer.send(500, "text/plain", "ERROR: Failed to save credentials");
        return;
    }

    // 返回确认页
    m_httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_SAVED);

    // 延迟 1 秒后自动重启，让新的凭据生效
    Serial.println(F("[CFG] 配网完成，1 秒后自动重启..."));
    delay(1000);
    ESP.restart();
}


/* ========================================================================== */
/*  私有方法 — Captive Portal 检测 URL 辅助处理器                               */
/* ========================================================================== */

void ConfigManager::serveCaptivePage()
{
    m_httpServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_CONFIG);
}

void ConfigManager::serveNcsiOK()
{
    m_httpServer.send(200, "text/plain", "Microsoft Connect Test");
}

void ConfigManager::serve204NoContent()
{
    m_httpServer.send(204, "text/plain", "");
}

/* ========================================================================== */
void ConfigManager::startStationMode()
{
    Serial.println(F("[CFG] >>> 进入 Station 模式（HTTP 重定向）<<<"));
    Serial.printf("[CFG] 设备 IP: %s\n", WiFi.localIP().toString().c_str());

    // 关闭 DNS 劫持（Station 模式下不需要）
    m_dnsServer.stop();
    Serial.println(F("[CFG] DNS 劫持已关闭"));

    // 重新配置 HTTP 服务器为 302 重定向模式
    // 注意：ESP8266WebServer 不支持动态切换路由，因此关闭后重新 begin()
    m_httpServer.stop();

    // 注册 302 重定向路由
    m_httpServer.on("/", HTTP_GET, [this]()
    {
        const String url = String(REDIRECT_BASE_URL) + "?ip="
                         + WiFi.localIP().toString();
        m_httpServer.sendHeader("Location", url, true);
        m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_CONFIG);
    });

    // 其他所有路径也做 302 重定向
    m_httpServer.onNotFound([this]()
    {
        const String url = String(REDIRECT_BASE_URL) + "?ip="
                         + WiFi.localIP().toString();
        m_httpServer.sendHeader("Location", url, true);
        m_httpServer.send_P(200, "text/html; charset=utf-8", PAGE_CONFIG);
    });

    m_httpServer.begin();
    Serial.println(F("[CFG] HTTP 302 重定向服务已启动 (80 端口)"));
    Serial.printf("[CFG] 访问 http://%s/ 自动跳转至前端控制台\n",
                  WiFi.localIP().toString().c_str());

    m_mode = Mode::STATION_CONNECTED;
}
