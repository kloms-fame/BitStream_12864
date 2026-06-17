/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager — DNS 劫持 + HTTP 动态路由 + Captive Portal
 *
 * @details 全链路日志覆盖：每条路由命中均通过 Serial 输出。
 *          Captive Portal 策略：所有检测端点一律 302 重定向至 /，
 *          绝不返回 200/204 误让设备以为有外网。
 */

#include "WebServerManager.h"
#include "NetworkManager.h"
#include <ESP8266WiFi.h>

/* ========================================================================== */
/*  常量                                                                      */
/* ========================================================================== */

static const char* REDIRECT_BASE_URL = "https://kloms-fame.github.io/BitStream_12864/";

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

    // 根路由 — 模式感知分发 (AP → 离线页 / STA → 302)
    m_httpServer.on("/", HTTP_GET, [this]() { serveRoot(); });

    // 配网 API
    m_httpServer.on("/api/setwifi", HTTP_POST,
        [this]() { handleSetWiFi(); });

    // ── Captive Portal 检测端点 ───────────────────────────────────────
    // 策略：全部 302 重定向至 /，强制唤起门户登录页

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

    // Windows (ncsi/connecttest — 必须 302 不能 200，否则 Windows 误判有外网)
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

    // 404 兜底 — 打印被拦截的原始 URI 后 302 重定向
    m_httpServer.onNotFound([this]() {
        Serial.printf("[WEB] 拦截未知请求: %s | 客户端: %s\n",
                      m_httpServer.uri().c_str(),
                      m_httpServer.client().remoteIP().toString().c_str());
        redirectToRoot();
    });

    /* ---- 3. 启动 HTTP 服务 ---------------------------------------------- */
    m_httpServer.begin();
    Serial.printf("[WEB] HTTP 服务已启动 (端口 %u)\n", HTTP_PORT);
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

        static uint32_t s_dnsCount = 0;
        if (++s_dnsCount % 100 == 0) {
            Serial.printf("[WEB] DNS 已处理 %u 次查询\n", s_dnsCount);
        }
    }
}

/* ========================================================================== */
/*  路由处理器 — serveRoot()                                                   */
/* ========================================================================== */

void WebServerManager::serveRoot()
{
    Serial.printf("[WEB] 命中路由: %s | 客户端: %s\n",
                  m_httpServer.uri().c_str(),
                  m_httpServer.client().remoteIP().toString().c_str());

    if (NetworkManager::isAPMode())
    {
        serveOfflinePage();
    }
    else
    {
        const String url = String(REDIRECT_BASE_URL)
                         + "?ip=" + WiFi.localIP().toString();

        Serial.printf("[WEB] STA 302 → %s\n", url.c_str());
        m_httpServer.sendHeader("Location", url, true);
        m_httpServer.send(302, "text/plain", "");
    }
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
/*  路由处理器 — serveOfflinePage()                                            */
/* ========================================================================== */

void WebServerManager::serveOfflinePage()
{
    Serial.printf("[WEB] 命中路由: %s → 直出 /offline.html | 客户端: %s\n",
                  m_httpServer.uri().c_str(),
                  m_httpServer.client().remoteIP().toString().c_str());

    if (!LittleFS.exists("/offline.html"))
    {
        Serial.println(F("[WEB] /offline.html 不存在!"));
        m_httpServer.send(500, "text/plain", "offline.html not found");
        return;
    }

    File f = LittleFS.open("/offline.html", "r");
    if (!f)
    {
        m_httpServer.send(500, "text/plain", "Cannot open offline.html");
        return;
    }

    m_httpServer.streamFile(f, "text/html; charset=utf-8");
    f.close();
}

/* ========================================================================== */
/*  路由处理器 — redirectToRoot()                                              */
/* ========================================================================== */

void WebServerManager::redirectToRoot()
{
    const String uri = m_httpServer.uri();
    const String clientIP = m_httpServer.client().remoteIP().toString();
    Serial.printf("[WEB] 命中路由: %s | 客户端: %s", uri.c_str(), clientIP.c_str());

    // AP 模式：直出离线页面，避免 302 被代理软件拦截
    if (NetworkManager::isAPMode())
    {
        Serial.println(F(" → 直出 offline.html (防代理拦截)"));
        serveOfflinePage();
    }
    else
    {
        Serial.println(F(" → 302 http://192.168.4.1/"));
        m_httpServer.sendHeader("Location", "http://192.168.4.1/", true);
        m_httpServer.send(302, "text/plain", "");
    }
}
