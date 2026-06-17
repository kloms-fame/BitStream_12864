/**
 * @file    WebServerManager.cpp
 * @brief   WebServerManager — DNS 劫持 + HTTP 动态路由 + Captive Portal
 *
 * @details 全链路日志覆盖：每条路由命中均通过 Serial 输出，
 *          404 兜底拦截打印被拦截的原始 URI，方便排查 Captive Portal 行为。
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

        // 显式添加常见检测域名（部分平台对通配符 DNS 处理不佳）
        const char* domains[] = {
            "www.msftconnecttest.com",
            "msftconnecttest.com",
            "captive.apple.com",
            "connectivitycheck.gstatic.com",
            "clients3.google.com",
            "detectportal.firefox.com",
            "www.msftncsi.com",
        };
        for (const char* d : domains)
        {
            (void)d;  // 通配符已覆盖，保留数组供文档参考
        }

        Serial.println(F("[WEB] DNS 劫持已启动 (53 → 192.168.4.1)"));
    }

    /* ---- 2. HTTP 路由注册 ------------------------------------------------ */

    // 根路由 — 模式感知分发
    m_httpServer.on("/", HTTP_GET, [this]() { serveRoot(); });

    // 配网 API
    m_httpServer.on("/api/setwifi", HTTP_POST,
        [this]() { handleSetWiFi(); });

    // Android Captive Portal 检测 — 必须 302 重定向，绝不能返回 204
    m_httpServer.on("/generate_204", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Apple / iOS / macOS Captive Portal 检测
    m_httpServer.on("/hotspot-detect.html", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/library/test/success.html", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Firefox Captive Portal 检测
    m_httpServer.on("/canonical.html", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/success.txt", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Windows Captive Portal 检测
    m_httpServer.on("/ncsi.txt", HTTP_GET,
        [this]() { serveNcsiOK(); });
    m_httpServer.on("/connecttest.txt", HTTP_GET,
        [this]() { serveNcsiOK(); });
    m_httpServer.on("/redirect", HTTP_GET,
        [this]() { redirectToRoot(); });
    m_httpServer.on("/fwlink", HTTP_GET,
        [this]() { redirectToRoot(); });

    // Chrome 检测
    m_httpServer.on("/check_network_status.txt", HTTP_GET,
        [this]() { redirectToRoot(); });

    // 404 兜底 — 打印被拦截的原始 URI 后 302 重定向
    m_httpServer.onNotFound([this]() {
        Serial.printf("[WEB] 拦截未知请求: %s → 重定向至 /\n",
                      m_httpServer.uri().c_str());
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
    m_dnsServer.processNextRequest();
    m_httpServer.handleClient();
}

/* ========================================================================== */
/*  路由处理器 — serveRoot()                                                   */
/* ========================================================================== */

void WebServerManager::serveRoot()
{
    Serial.printf("[WEB] 命中路由: %s\n", m_httpServer.uri().c_str());

    if (NetworkManager::isAPMode())
    {
        serveOfflinePage();
    }
    else
    {
        const String url = String(REDIRECT_BASE_URL)
                         + "?ip=" + WiFi.localIP().toString();

        m_httpServer.sendHeader("Location", url, true);
        m_httpServer.send(302, "text/plain", "");
    }
}

/* ========================================================================== */
/*  路由处理器 — handleSetWiFi()                                               */
/* ========================================================================== */

void WebServerManager::handleSetWiFi()
{
    Serial.printf("[WEB] 命中路由: %s\n", m_httpServer.uri().c_str());

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
    Serial.printf("[WEB] 命中路由: %s\n", m_httpServer.uri().c_str());

    if (!LittleFS.exists("/offline.html"))
    {
        Serial.println(F("[WEB] /offline.html 不存在!"));
        m_httpServer.send(500, "text/plain", "offline.html not found on device");
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
    Serial.printf("[WEB] 命中路由: %s\n", m_httpServer.uri().c_str());

    m_httpServer.sendHeader("Location", "/", true);
    m_httpServer.send(302, "text/plain", "");
}

/* ========================================================================== */
/*  路由处理器 — serveNcsiOK() (Windows NCSI)                                  */
/* ========================================================================== */

void WebServerManager::serveNcsiOK()
{
    Serial.printf("[WEB] 命中路由: %s (NCSI)\n", m_httpServer.uri().c_str());
    m_httpServer.send(200, "text/plain", "Microsoft Connect Test");
}
