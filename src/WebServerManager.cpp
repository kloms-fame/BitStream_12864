/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager — 静态资源本地化 + DNS 劫持 + Captive Portal
 *
 * @details AP 模式：DNS 劫持 + Captive Portal + 提供 offline.html（配网表单）
 *          STA 模式：仅提供 HTTP 服务，返回 index.html（推流控制台）
 *          彻底摒弃 302 重定向至 GitHub Pages 的旧方案，
 *          消除 HTTPS→ws:// Mixed Content 阻断问题。
 */

#include "WebServerManager.h"
#include "DebugMacros.h"  // [P3-6]
#include "NetworkManager.h"

/* ========================================================================== */
/*  常量 — 前端页面路径（AP 配网页 / STA 控制台页）                            */
/* ========================================================================== */

static const char* AP_CONFIG_PAGE    = "/offline.html";    // AP 模式：配网 + 连接
static const char* CONSOLE_PAGE_GZ   = "/index.html.gz";   // STA 模式：完整控制台（压缩）
static const char* CONSOLE_PAGE      = "/index.html";      // STA 模式：完整控制台（未压缩）

/* ========================================================================== */
/*  构造函数                                                                  */
/* ========================================================================== */

WebServerManager::WebServerManager()
    : m_httpServer(HTTP_PORT)
{
}

/* ========================================================================== */
/*  begin() — 注册路由 + 启动 HTTP / DNS                                      */
/* ========================================================================== */

void WebServerManager::begin()
{
    /* ---- 1. DNS 劫持（仅 AP 模式）---------------------------------------- */
    if (NetworkManager::isAPMode())
    {
        m_dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        m_dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));

        Serial.println(F("[WEB] DNS 劫持已启动 (53 → 192.168.4.1)"));
    }

    /* ---- 2. HTTP 路由注册 ------------------------------------------------ */

    // 根路由 — 根据运行模式分发不同页面
    //   AP 模式 → offline.html（配网表单 + 默认 IP 192.168.4.1）
    //   STA 模式 → index.html（完整推流控制台）
    m_httpServer.on("/", HTTP_GET, [this]() { serveModePage(); });

    // 配网 API（仅 AP 模式下 offline.html 会调用）
    m_httpServer.on("/api/setwifi", HTTP_POST,
        [this]() { handleSetWiFi(); });

    // ── [P3-5] 版本信息 ──────────────────────────────────────
    m_httpServer.on("/api/version", HTTP_GET, [this]() {
        m_httpServer.send(200, "application/json",
            "{\"version\":\"2.1.0\",\"platform\":\"ESP8266\"}");
    });

    // ── Captive Portal 检测端点 ───────────────────────────────────────
    // 全部 302 重定向至 /，让设备浏览器弹出强制门户

    // Android
    m_httpServer.on("/generate_204", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Apple / iOS / macOS
    m_httpServer.on("/hotspot-detect.html", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/library/test/success.html", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Firefox
    m_httpServer.on("/canonical.html", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/success.txt", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Windows
    m_httpServer.on("/ncsi.txt", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/connecttest.txt", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/redirect", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/fwlink", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Chrome
    m_httpServer.on("/check_network_status.txt", HTTP_GET,
        [this]() { redirectToRoot(); });

    // 404 兜底 — 打印被拦截的原始 URI 后返回首页

    // ── 共享推流引擎 JS (供两个前端页面共用) ───────────────────────
    m_httpServer.on("/stream-engine.js", HTTP_GET, [this]() {
        const char* engineFile = "/stream-engine.js";
        if (LittleFS.exists(engineFile)) {
            File f = LittleFS.open(engineFile, "r");
            if (f) {
                m_httpServer.streamFile(f, "application/javascript; charset=utf-8");
                f.close();
                return;
            }
        }
        m_httpServer.send(404, "text/plain", "engine not found");
    });
    m_httpServer.onNotFound([this]() {
        Serial.printf("[WEB] 拦截未知请求: %s | 客户端: %s\n",
                      m_httpServer.uri().c_str(),
                      m_httpServer.client().remoteIP().toString().c_str());
        redirectToRoot();
    });

    /* ---- 3. 启动 HTTP 服务 ---------------------------------------------- */
    m_httpServer.begin();
    if (NetworkManager::isAPMode())
        Serial.printf("[WEB] HTTP 服务已启动 (端口 %u) | AP 配网模式 → offline.html\n", HTTP_PORT);
    else
        Serial.printf("[WEB] HTTP 服务已启动 (端口 %u) | STA 控制台模式 → index.html\n", HTTP_PORT);
}

/* ========================================================================== */
/*  loop()                                                                     */
/* ========================================================================== */

void WebServerManager::loop()
{
    m_httpServer.handleClient();

    // DNS 劫持仅在 AP 模式有效，STA 模式直接跳过
    if (NetworkManager::isAPMode()) {
    // [P2-3] DNS 限流：仅每 100ms 处理一次 DNS 请求，防止 CAPTIVE PORTAL
    // 探测风暴占满 ESP8266 CPU
    if (NetworkManager::isAPMode()) {
        static uint32_t s_lastDnsProcess = 0;
        const uint32_t nowDns = millis();
        if (nowDns - s_lastDnsProcess >= 100) {
            s_lastDnsProcess = nowDns;
            m_dnsServer.processNextRequest();
        }
    }
        // 注意：已移除高频 DNS 刷屏日志，避免占用单片机串口资源
    }
}

/* ========================================================================== */
/*  路由处理器 — serveModePage()                                               */
/*  @brief 根据当前运行模式返回对应页面                                         */
/*         AP 模式 → offline.html（配网表单，默认 IP 192.168.4.1）              */
/*         STA 模式 → index.html(.gz)（完整推流控制台）                         */
/* ========================================================================== */

void WebServerManager::serveModePage()
{
    const char* pageName;
    const char* pageFile;

    if (NetworkManager::isAPMode())
    {
        // AP 模式：提供配网页面，内置 SSID/密码表单和 WebSocket 连接
        pageName = "offline.html (配网控制台)";
        pageFile = AP_CONFIG_PAGE;
    }
    else
    {
        // STA 模式：提供完整推流控制台（视频文件选择、倍速、绿幕等）
        // 优先使用 gzip 压缩版
        if (LittleFS.exists(CONSOLE_PAGE_GZ))
        {
            pageFile = CONSOLE_PAGE_GZ;
        }
        else
        {
            pageFile = CONSOLE_PAGE;
        }
        pageName = "index.html (推流控制台)";
    }

    Serial.printf("[WEB] 命中路由: %s → 直出 %s | 客户端: %s\n",
                  m_httpServer.uri().c_str(), pageName,
                  m_httpServer.client().remoteIP().toString().c_str());

    if (LittleFS.exists(pageFile))
    {
        File f = LittleFS.open(pageFile, "r");
        if (f)
        {
            m_httpServer.streamFile(f, "text/html; charset=utf-8");
            f.close();
            return;
        }
    }

    // 最终兜底：文件不存在
    Serial.printf("[WEB] 页面文件 %s 不存在! 请上传至 data/\n", pageFile);
    m_httpServer.send(500, "text/plain", "Page not found on ESP8266");
}

/* ========================================================================== */
/*  路由处理器 — handleSetWiFi()                                               */
/* ========================================================================== */

void WebServerManager::handleSetWiFi()
{
    Serial.printf("[WEB] 命中路由: %s | 客户端: %s\n",
                  m_httpServer.uri().c_str(),
                  m_httpServer.client().remoteIP().toString().c_str());

    const String ssid = m_httpServer.arg("ssid");
    const String pass = m_httpServer.arg("pwd");

    if (ssid.length() == 0)
    {
        m_httpServer.send(400, "text/plain", "ERROR: SSID required");
        return;
    }

    Serial.printf("[WEB] 收到配网请求: SSID=\"%s\"\n", ssid.c_str());
    m_httpServer.send(200, "text/plain", "OK: Rebooting...");
    NetworkManager::saveWiFiAndReboot(ssid.c_str(), pass.c_str());
}

/* ========================================================================== */
/*  路由处理器 — redirectToRoot()                                              */
/*  @brief 302 重定向至 /，用于 Captive Portal 检测端点                         */
/* ========================================================================== */

void WebServerManager::redirectToRoot()
{
    const String uri = m_httpServer.uri();
    const String clientIP = m_httpServer.client().remoteIP().toString();
    Serial.printf("[WEB] 命中路由: %s | 客户端: %s → 302 /\n",
                  uri.c_str(), clientIP.c_str());

    m_httpServer.sendHeader("Location", "/", true);
    m_httpServer.send(302, "text/plain", "");
}
