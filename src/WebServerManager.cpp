/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager — 静态资源本地化 + DNS 劫持 + Captive Portal
 *
 * @details AP 模式：DNS 劫持 + Captive Portal 精准拦截
 *          STA 模式：仅提供 HTTP 服务
 *          统一路由：GET / 直接从 LittleFS 返回 index.html（或 index.html.gz），
 *          彻底摒弃 302 重定向至 GitHub Pages 的旧方案，
 *          消除 HTTPS→ws:// Mixed Content 阻断问题。
 */

#include "WebServerManager.h"
#include "NetworkManager.h"

/* ========================================================================== */
/*  常量 — 前端控制台路径（优先 .gz 压缩版以节省带宽）                          */
/* ========================================================================== */

static const char* CONSOLE_PAGE_GZ = "/index.html.gz";
static const char* CONSOLE_PAGE    = "/index.html";

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

    // 根路由 — 统一从 LittleFS 返回本地控制台页面
    // （AP 和 STA 模式均走此路由，彻底废弃 302 重定向）
    m_httpServer.on("/", HTTP_GET, [this]() { serveConsolePage(); });

    // 配网 API（AP 模式下使用）
    m_httpServer.on("/api/setwifi", HTTP_POST,
        [this]() { handleSetWiFi(); });

    // ── Captive Portal 检测端点 ───────────────────────────────────────
    // 策略：全部返回配网页内容，让设备浏览器弹出强制门户

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
    m_httpServer.onNotFound([this]() {
        Serial.printf("[WEB] 拦截未知请求: %s | 客户端: %s\n",
                      m_httpServer.uri().c_str(),
                      m_httpServer.client().remoteIP().toString().c_str());
        redirectToRoot();
    });

    /* ---- 3. 启动 HTTP 服务 ---------------------------------------------- */
    m_httpServer.begin();
    Serial.printf("[WEB] HTTP 服务已启动 (端口 %u) | 静态页面直出模式\n", HTTP_PORT);
}

/* ========================================================================== */
/*  loop()                                                                     */
/* ========================================================================== */

void WebServerManager::loop()
{
    m_httpServer.handleClient();

    // DNS 劫持仅在 AP 模式有效，STA 模式直接跳过
    if (NetworkManager::isAPMode()) {
        m_dnsServer.processNextRequest();
        // 注意：已移除高频 DNS 刷屏日志，避免占用单片机串口资源
    }
}

/* ========================================================================== */
/*  路由处理器 — serveConsolePage()                                            */
/*  @brief 统一控制台页面入口 — 优先返回 .gz 压缩版，回退至 .html               */
/* ========================================================================== */

void WebServerManager::serveConsolePage()
{
    Serial.printf("[WEB] 命中路由: %s → 直出控制台 | 客户端: %s\n",
                  m_httpServer.uri().c_str(),
                  m_httpServer.client().remoteIP().toString().c_str());

    // 优先尝试 gzip 压缩版本（节省带宽，ESP8266 CPU 无需实时压缩）
    if (LittleFS.exists(CONSOLE_PAGE_GZ))
    {
        File f = LittleFS.open(CONSOLE_PAGE_GZ, "r");
        if (f)
        {
            m_httpServer.streamFile(f, "text/html; charset=utf-8");
            f.close();
            return;
        }
    }

    // 回退：未压缩版本
    if (LittleFS.exists(CONSOLE_PAGE))
    {
        File f = LittleFS.open(CONSOLE_PAGE, "r");
        if (f)
        {
            m_httpServer.streamFile(f, "text/html; charset=utf-8");
            f.close();
            return;
        }
    }

    // 最终兜底：文件不存在
    Serial.println(F("[WEB] 控制台页面文件不存在! 请上传 index.html 或 index.html.gz 至 data/"));
    m_httpServer.send(500, "text/plain", "Console page not found");
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