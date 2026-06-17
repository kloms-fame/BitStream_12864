/**
 * @file    WebServerManager.h
 * @brief   V2 Web 服务器管理 — 静态资源本地化 + DNS 劫持 + Captive Portal
 *
 * @details 本模块负责 HTTP (80 端口) 和 DNS (53 端口) 的所有逻辑。
 *          根据 NetworkManager::isAPMode() 动态切换行为：
 *
 *          AP 模式 (离线):
 *          - DNS 劫持 * → 192.168.4.1
 *          - Captive Portal 精准拦截 (generate_204 等)
 *          - GET / → LittleFS 直接提供 index.html(.gz)
 *          - POST /api/setwifi → NetworkManager::saveWiFiAndReboot()
 *
 *          STA 模式 (在线):
 *          - GET / → LittleFS 直接提供 index.html(.gz)
 *          - DNS 不启动
 *
 *          关键变更：彻底废弃 302 重定向至 GitHub Pages，
 *          统一以本地静态资源方式提供前端控制台，
 *          根治 HTTPS 页面无法连接局域网 ws:// 的 Mixed Content 问题。
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
    /** @brief 统一控制台页面 — 优先 .gz 压缩版，回退至 .html */
    void serveConsolePage();

    /** @brief 配网 API — 接收 SSID/密码并持久化重启 */
    void handleSetWiFi();

    /** @brief 302 重定向至 /，用于 Captive Portal 检测端点 */
    void redirectToRoot();

    DNSServer        m_dnsServer;
    ESP8266WebServer m_httpServer;
};

#endif // WEB_SERVER_MANAGER_H