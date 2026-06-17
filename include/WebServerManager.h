/**
 * @file    WebServerManager.h
 * @brief   V2 Web 服务器管理 — DNS 劫持 + 动态路由 + 强制门户
 *
 * @details 本模块负责 HTTP (80 端口) 和 DNS (53 端口) 的所有逻辑。
 *          根据 NetworkManager::isAPMode() 动态切换行为：
 *
 *          AP 模式 (离线):
 *          - DNS 劫持 * → 192.168.4.1
 *          - Captive Portal 精准拦截 (generate_204 等)
 *          - GET / → LittleFS 提供 /offline.html
 *          - POST /api/setwifi → NetworkManager::saveWiFiAndReboot()
 *          - 所有 Captive Portal 探测端点 → 302 重定向至 /
 *
 *          STA 模式 (在线):
 *          - GET / → HTTP 302 → GitHub Pages 控制台
 *          - DNS 不启动
 */

#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

class WebServerManager
{
public:
    static constexpr uint16_t HTTP_PORT = 80;
    static constexpr uint16_t DNS_PORT  = 53;

    WebServerManager();
    void begin();
    void loop();

private:
    void serveRoot();
    void handleSetWiFi();
    void serveOfflinePage();
    void redirectToRoot();

    DNSServer        m_dnsServer;
    ESP8266WebServer m_httpServer;
};

#endif // WEB_SERVER_MANAGER_H
